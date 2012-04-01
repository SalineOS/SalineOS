/*
 * DIB driver GDI objects.
 *
 * Copyright 2011 Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdlib.h>

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

/*
 *
 * Decompose the 16 ROP2s into an expression of the form
 *
 * D = (D & A) ^ X
 *
 * Where A and X depend only on P (and so can be precomputed).
 *
 *                                       A    X
 *
 * R2_BLACK         0                    0    0
 * R2_NOTMERGEPEN   ~(D | P)            ~P   ~P
 * R2_MASKNOTPEN    ~P & D              ~P    0
 * R2_NOTCOPYPEN    ~P                   0   ~P
 * R2_MASKPENNOT    P & ~D               P    P
 * R2_NOT           ~D                   1    1
 * R2_XORPEN        P ^ D                1    P
 * R2_NOTMASKPEN    ~(P & D)             P    1
 * R2_MASKPEN       P & D                P    0
 * R2_NOTXORPEN     ~(P ^ D)             1   ~P
 * R2_NOP           D                    1    0
 * R2_MERGENOTPEN   ~P | D               P   ~P
 * R2_COPYPEN       P                    0    P
 * R2_MERGEPENNOT   P | ~D              ~P    1
 * R2_MERGEPEN      P | D               ~P    P
 * R2_WHITE         1                    0    1
 *
 */

/* A = (P & A1) ^ A2 */
#define ZERO  { 0u,  0u}
#define ONE   { 0u, ~0u}
#define P     {~0u,  0u}
#define NOT_P {~0u, ~0u}

static const DWORD rop2_and_array[16][2] =
{
    ZERO, NOT_P, NOT_P, ZERO,
    P,    ONE,   ONE,   P,
    P,    ONE,   ONE,   P,
    ZERO, NOT_P, NOT_P, ZERO
};

/* X = (P & X1) ^ X2 */
static const DWORD rop2_xor_array[16][2] =
{
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE,
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE
};

#undef NOT_P
#undef P
#undef ONE
#undef ZERO

void get_rop_codes(INT rop, struct rop_codes *codes)
{
    /* NB The ROP2 codes start at one and the arrays are zero-based */
    codes->a1 = rop2_and_array[rop-1][0];
    codes->a2 = rop2_and_array[rop-1][1];
    codes->x1 = rop2_xor_array[rop-1][0];
    codes->x2 = rop2_xor_array[rop-1][1];
}

static inline void calc_and_xor_masks(INT rop, DWORD color, DWORD *and, DWORD *xor)
{
    struct rop_codes codes;
    get_rop_codes( rop, &codes );

    *and = (color & codes.a1) ^ codes.a2;
    *xor = (color & codes.x1) ^ codes.x2;
}

static inline void calc_rop_masks(INT rop, DWORD color, rop_mask *masks)
{
    calc_and_xor_masks( rop, color, &masks->and, &masks->xor );
}

static inline RGBQUAD rgbquad_from_colorref(COLORREF c)
{
    RGBQUAD ret;

    ret.rgbRed      = GetRValue(c);
    ret.rgbGreen    = GetGValue(c);
    ret.rgbBlue     = GetBValue(c);
    ret.rgbReserved = 0;
    return ret;
}

static inline BOOL rgbquad_equal(const RGBQUAD *a, const RGBQUAD *b)
{
    if(a->rgbRed   == b->rgbRed   &&
       a->rgbGreen == b->rgbGreen &&
       a->rgbBlue  == b->rgbBlue)
        return TRUE;
    return FALSE;
}

COLORREF make_rgb_colorref( HDC hdc, dib_info *dib, COLORREF color, BOOL *got_pixel, DWORD *pixel )
{
    *pixel = 0;
    *got_pixel = FALSE;

    if (color & (1 << 24))  /* PALETTEINDEX */
    {
        HPALETTE pal = GetCurrentObject( hdc, OBJ_PAL );
        PALETTEENTRY pal_ent;

        if (!GetPaletteEntries( pal, LOWORD(color), 1, &pal_ent ))
            GetPaletteEntries( pal, 0, 1, &pal_ent );
        return RGB( pal_ent.peRed, pal_ent.peGreen, pal_ent.peBlue );
    }

    if (color >> 16 == 0x10ff)  /* DIBINDEX */
    {
        WORD index = LOWORD( color );
        *got_pixel = TRUE;
        if (!dib->color_table || index >= (1 << dib->bit_count)) return 0;
        *pixel = index;
        return RGB( dib->color_table[index].rgbRed,
                    dib->color_table[index].rgbGreen,
                    dib->color_table[index].rgbBlue );
    }

    return color & 0xffffff;
}

/******************************************************************
 *                   get_pixel_color
 *
 * 1 bit bitmaps map the fg/bg colors as follows:
 * If the fg colorref exactly matches one of the color table entries then
 * that entry is the fg color and the other is the bg.
 * Otherwise the bg color is mapped to the closest entry in the table and
 * the fg takes the other one.
 */
DWORD get_pixel_color( dibdrv_physdev *pdev, COLORREF color, BOOL mono_fixup )
{
    RGBQUAD fg_quad;
    BOOL got_pixel;
    DWORD pixel;
    COLORREF rgb_ref;

    rgb_ref = make_rgb_colorref( pdev->dev.hdc, &pdev->dib, color, &got_pixel, &pixel );
    if (got_pixel) return pixel;

    if (pdev->dib.bit_count != 1 || !mono_fixup)
        return pdev->dib.funcs->colorref_to_pixel( &pdev->dib, rgb_ref );

    fg_quad = rgbquad_from_colorref( rgb_ref );
    if(rgbquad_equal(&fg_quad, pdev->dib.color_table))
        return 0;
    if(rgbquad_equal(&fg_quad, pdev->dib.color_table + 1))
        return 1;

    pixel = get_pixel_color( pdev, GetBkColor(pdev->dev.hdc), FALSE );
    if (color == GetBkColor(pdev->dev.hdc)) return pixel;
    else return !pixel;
}

/***************************************************************************
 *                get_color_masks
 *
 * Returns the color masks unless the dib is 1 bpp.  In this case since
 * there are several fg sources (pen, brush, text) we take as bg the inverse
 * of the relevant fg color (which is always set up correctly).
 */
static inline void get_color_masks( dibdrv_physdev *pdev, UINT rop, COLORREF colorref,
                                    INT bkgnd_mode, rop_mask *fg_mask, rop_mask *bg_mask )
{
    DWORD color = get_pixel_color( pdev, colorref, TRUE );

    calc_rop_masks( rop, color, fg_mask );

    if (bkgnd_mode == TRANSPARENT)
    {
        bg_mask->and = ~0u;
        bg_mask->xor = 0;
        return;
    }

    if (pdev->dib.bit_count != 1) color = get_pixel_color( pdev, GetBkColor(pdev->dev.hdc), FALSE );
    else if (colorref != GetBkColor(pdev->dev.hdc)) color = !color;

    calc_rop_masks( rop, color, bg_mask );
}

static inline void order_end_points(int *s, int *e)
{
    if(*s > *e)
    {
        int tmp;
        tmp = *s + 1;
        *s = *e + 1;
        *e = tmp;
    }
}

#define Y_INCREASING_MASK 0x0f
#define X_INCREASING_MASK 0xc3
#define X_MAJOR_MASK      0x99
#define POS_SLOPE_MASK    0x33

static inline BOOL is_xmajor(DWORD octant)
{
    return octant & X_MAJOR_MASK;
}

static inline BOOL is_pos_slope(DWORD octant)
{
    return octant & POS_SLOPE_MASK;
}

static inline BOOL is_x_increasing(DWORD octant)
{
    return octant & X_INCREASING_MASK;
}

static inline BOOL is_y_increasing(DWORD octant)
{
    return octant & Y_INCREASING_MASK;
}

/**********************************************************************
 *                  get_octant_number
 *
 * Return the octant number starting clockwise from the +ve x-axis.
 */
static inline int get_octant_number(int dx, int dy)
{
    if(dy > 0)
        if(dx > 0)
            return ( dx >  dy) ? 1 : 2;
        else
            return (-dx >  dy) ? 4 : 3;
    else
        if(dx < 0)
            return (-dx > -dy) ? 5 : 6;
        else
            return ( dx > -dy) ? 8 : 7;
}

static inline DWORD get_octant_mask(int dx, int dy)
{
    return 1 << (get_octant_number(dx, dy) - 1);
}

static inline int get_bias( DWORD mask )
{
    /* Octants 3, 5, 6 and 8 take a bias */
    return (mask & 0xb4) ? 1 : 0;
}

#define OUT_LEFT    1
#define OUT_RIGHT   2
#define OUT_TOP     4
#define OUT_BOTTOM  8

static inline DWORD calc_outcode(const POINT *pt, const RECT *clip)
{
    DWORD out = 0;
    if(pt->x < clip->left)         out |= OUT_LEFT;
    else if(pt->x >= clip->right)  out |= OUT_RIGHT;
    if(pt->y < clip->top)          out |= OUT_TOP;
    else if(pt->y >= clip->bottom) out |= OUT_BOTTOM;

    return out;
}

/******************************************************************************
 *                clip_line
 *
 * Clips the start and end points to a rectangle.
 *
 * Note, this treats the end point like the start point.  If the
 * caller doesn't want it displayed, it should exclude it.  If the end
 * point is clipped out, then the likelihood is that the new end point
 * should be displayed.
 *
 * Returns 0 if totally excluded, 1 if partially clipped and 2 if unclipped.
 *
 * This derivation is based on the comments in X.org's xserver/mi/mizerclip.c,
 * however the Bresenham error term is defined differently so the equations
 * will also differ.
 *
 * For x major lines we have 2dy >= err + bias > 2dy - 2dx
 *                           0   >= err + bias - 2dy > -2dx
 *
 * Note dx, dy, m and n are all +ve.
 *
 * Moving the start pt from x1 to x1 + m, we need to figure out y1 + n.
 *                     err = 2dy - dx + 2mdy - 2ndx
 *                      0 >= 2dy - dx + 2mdy - 2ndx + bias - 2dy > -2dx
 *                      0 >= 2mdy - 2ndx + bias - dx > -2dx
 *                      which of course will give exactly one solution for n,
 *                      so looking at the >= inequality
 *                      n >= (2mdy + bias - dx) / 2dx
 *                      n = ceiling((2mdy + bias - dx) / 2dx)
 *                        = (2mdy + bias + dx - 1) / 2dx (assuming division truncation)
 *
 * Moving start pt from y1 to y1 + n we need to figure out x1 + m - there may be several
 * solutions we pick the one that minimizes m (ie that first unlipped pt). As above:
 *                     0 >= 2mdy - 2ndx + bias - dx > -2dx
 *                  2mdy > 2ndx - bias - dx
 *                     m > (2ndx - bias - dx) / 2dy
 *                     m = floor((2ndx - bias - dx) / 2dy) + 1
 *                     m = (2ndx - bias - dx) / 2dy + 1
 *
 * Moving end pt from x2 to x2 - m, we need to figure out y2 - n
 *                  err = 2dy - dx + 2(dx - m)dy - 2(dy - n)dx
 *                      = 2dy - dx - 2mdy + 2ndx
 *                   0 >= 2dy - dx - 2mdy + 2ndx + bias - 2dy > -2dx
 *                   0 >= 2ndx - 2mdy + bias - dx > -2dx
 *                   again exactly one solution.
 *                   2ndx <= 2mdy - bias + dx
 *                   n = floor((2mdy - bias + dx) / 2dx)
 *                     = (2mdy - bias + dx) / 2dx
 *
 * Moving end pt from y2 to y2 - n when need x2 - m this time maximizing x2 - m so
 * mininizing m to include all of the points at y = y2 - n.  As above:
 *                  0 >= 2ndx - 2mdy + bias - dx > -2dx
 *               2mdy >= 2ndx + bias - dx
 *                   m = ceiling((2ndx + bias - dx) / 2dy)
 *                     = (2ndx + bias - dx - 1) / 2dy + 1
 *
 * For y major lines, symmetry (dx <-> dy and swap the cases over) gives:
 *
 * Moving start point from y1 to y1 + n find x1 + m
 *                     m = (2ndx + bias + dy - 1) / 2dy
 *
 * Moving start point from x1 to x1 + m find y1 + n
 *                     n = (2mdy - bias - dy) / 2ndx + 1
 *
 * Moving end point from y2 to y2 - n find x1 - m
 *                     m = (2ndx - bias + dy) / 2dy
 *
 * Moving end point from x2 to x2 - m find y2 - n
 *                     n = (2mdy + bias - dy - 1) / 2dx + 1
 */
int clip_line(const POINT *start, const POINT *end, const RECT *clip,
              const bres_params *params, POINT *pt1, POINT *pt2)
{

    INT64 m, n;  /* 64-bit to avoid overflows (FIXME: find a more efficient way) */
    BOOL clipped = FALSE;
    DWORD start_oc, end_oc;
    const int bias = params->bias;
    const unsigned int dx = params->dx;
    const unsigned int dy = params->dy;
    const unsigned int two_dx = params->dx * 2;
    const unsigned int two_dy = params->dy * 2;
    const BOOL xmajor = is_xmajor(params->octant);
    const BOOL neg_slope = !is_pos_slope(params->octant);

    *pt1 = *start;
    *pt2 = *end;

    start_oc = calc_outcode(start, clip);
    end_oc = calc_outcode(end, clip);

    while(1)
    {
        if(start_oc == 0 && end_oc == 0) return clipped ? 1 : 2; /* trivial accept */
        if(start_oc & end_oc)            return 0; /* trivial reject */

        clipped = TRUE;
        if(start_oc & OUT_LEFT)
        {
            m = clip->left - start->x;
            if(xmajor)
                n = (m * two_dy + bias + dx - 1) / two_dx;
            else
                n = (m * two_dy - bias - dy) / two_dx + 1;

            pt1->x = clip->left;
            if(neg_slope) n = -n;
            pt1->y = start->y + n;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_RIGHT)
        {
            m = start->x - clip->right + 1;
            if(xmajor)
                n = (m * two_dy + bias + dx - 1) / two_dx;
            else
                n = (m * two_dy - bias - dy) / two_dx + 1;

            pt1->x = clip->right - 1;
            if(neg_slope) n = -n;
            pt1->y = start->y - n;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_TOP)
        {
            n = clip->top - start->y;
            if(xmajor)
                m = (n * two_dx - bias - dx) / two_dy + 1;
            else
                m = (n * two_dx + bias + dy - 1) / two_dy;

            pt1->y = clip->top;
            if(neg_slope) m = -m;
            pt1->x = start->x + m;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_BOTTOM)
        {
            n = start->y - clip->bottom + 1;
            if(xmajor)
                m = (n * two_dx - bias - dx) / two_dy + 1;
            else
                m = (n * two_dx + bias + dy - 1) / two_dy;

            pt1->y = clip->bottom - 1;
            if(neg_slope) m = -m;
            pt1->x = start->x - m;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(end_oc & OUT_LEFT)
        {
            m = clip->left - end->x;
            if(xmajor)
                n = (m * two_dy - bias + dx) / two_dx;
            else
                n = (m * two_dy + bias - dy - 1) / two_dx + 1;

            pt2->x = clip->left;
            if(neg_slope) n = -n;
            pt2->y = end->y + n;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_RIGHT)
        {
            m = end->x - clip->right + 1;
            if(xmajor)
                n = (m * two_dy - bias + dx) / two_dx;
            else
                n = (m * two_dy + bias - dy - 1) / two_dx + 1;

            pt2->x = clip->right - 1;
            if(neg_slope) n = -n;
            pt2->y = end->y - n;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_TOP)
        {
            n = clip->top - end->y;
            if(xmajor)
                m = (n * two_dx + bias - dx - 1) / two_dy + 1;
            else
                m = (n * two_dx - bias + dy) / two_dy;

            pt2->y = clip->top;
            if(neg_slope) m = -m;
            pt2->x = end->x + m;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_BOTTOM)
        {
            n = end->y - clip->bottom + 1;
            if(xmajor)
                m = (n * two_dx + bias - dx - 1) / two_dy + 1;
            else
                m = (n * two_dx - bias + dy) / two_dy;

            pt2->y = clip->bottom - 1;
            if(neg_slope) m = -m;
            pt2->x = end->x - m;
            end_oc = calc_outcode(pt2, clip);
        }
    }
}

static void bres_line_with_bias(const POINT *start, const struct line_params *params,
                                void (* callback)(dibdrv_physdev*,INT,INT), dibdrv_physdev *pdev)
{
    POINT pt = *start;
    int len = params->length, err = params->err_start;

    if (params->x_major)
    {
        while(len--)
        {
            callback(pdev, pt.x, pt.y);
            if (err + params->bias > 0)
            {
                pt.y += params->y_inc;
                err += params->err_add_1;
            }
            else err += params->err_add_2;
            pt.x += params->x_inc;
        }
    }
    else
    {
        while(len--)
        {
            callback(pdev, pt.x, pt.y);
            if (err + params->bias > 0)
            {
                pt.x += params->x_inc;
                err += params->err_add_1;
            }
            else err += params->err_add_2;
            pt.y += params->y_inc;
        }
    }
}

static BOOL solid_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end, DWORD and, DWORD xor)
{
    struct clipped_rects clipped_rects;
    RECT rect;
    int i;

    if(start->y == end->y)
    {
        rect.left   = start->x;
        rect.top    = start->y;
        rect.right  = end->x;
        rect.bottom = end->y + 1;
        order_end_points(&rect.left, &rect.right);
        if (!get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects )) return TRUE;
        pdev->dib.funcs->solid_rects(&pdev->dib, clipped_rects.count, clipped_rects.rects, and, xor);
    }
    else if(start->x == end->x)
    {
        rect.left   = start->x;
        rect.top    = start->y;
        rect.right  = end->x + 1;
        rect.bottom = end->y;
        order_end_points(&rect.top, &rect.bottom);
        if (!get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects )) return TRUE;
        pdev->dib.funcs->solid_rects(&pdev->dib, clipped_rects.count, clipped_rects.rects, and, xor);
    }
    else
    {
        bres_params clip_params;
        struct line_params line_params;
        INT dx = end->x - start->x, dy = end->y - start->y;
        INT abs_dx = abs(dx), abs_dy = abs(dy);

        clip_params.dx = abs_dx;
        clip_params.dy = abs_dy;
        clip_params.octant = get_octant_mask(dx, dy);
        clip_params.bias   = get_bias( clip_params.octant );

        line_params.bias    = clip_params.bias;
        line_params.x_major = is_xmajor( clip_params.octant );
        line_params.x_inc   = is_x_increasing( clip_params.octant ) ? 1 : -1;
        line_params.y_inc   = is_y_increasing( clip_params.octant ) ? 1 : -1;

        if (line_params.x_major)
        {
            line_params.err_add_1 = 2 * abs_dy - 2 * abs_dx;
            line_params.err_add_2 = 2 * abs_dy;
        }
        else
        {
            line_params.err_add_1 = 2 * abs_dx - 2 * abs_dy;
            line_params.err_add_2 = 2 * abs_dx;
        }

        rect.left   = min( start->x, end->x );
        rect.top    = min( start->y, end->y );
        rect.right  = max( start->x, end->x ) + 1;
        rect.bottom = max( start->y, end->y ) + 1;
        if (!get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects )) return TRUE;
        for (i = 0; i < clipped_rects.count; i++)
        {
            POINT clipped_start, clipped_end;
            int clip_status;

            clip_status = clip_line(start, end, clipped_rects.rects + i, &clip_params, &clipped_start, &clipped_end);
            if(clip_status)
            {
                int m = abs(clipped_start.x - start->x);
                int n = abs(clipped_start.y - start->y);

                if (line_params.x_major)
                {
                    line_params.err_start = 2 * abs_dy - abs_dx + m * 2 * abs_dy - n * 2 * abs_dx;
                    line_params.length = abs( clipped_end.x - clipped_start.x ) + 1;
                }
                else
                {
                    line_params.err_start = 2 * abs_dx - abs_dy + n * 2 * abs_dx - m * 2 * abs_dy;
                    line_params.length = abs( clipped_end.y - clipped_start.y ) + 1;
                }

                if (clipped_end.x == end->x && clipped_end.y == end->y) line_params.length--;

                pdev->dib.funcs->solid_line( &pdev->dib, &clipped_start, &line_params, and, xor );

                if(clip_status == 2) break; /* completely unclipped, so we can finish */
            }
        }
    }
    free_clipped_rects( &clipped_rects );
    return TRUE;
}

static BOOL solid_pen_line_region( dibdrv_physdev *pdev, POINT *start, POINT *end, HRGN region )
{
    RECT rect;

    rect.left   = start->x;
    rect.top    = start->y;
    rect.right  = start->x + 1;
    rect.bottom = start->y + 1;

    if (start->y == end->y)
    {
        rect.right = end->x;
        order_end_points(&rect.left, &rect.right);
        add_rect_to_region( region, &rect );
    }
    else if(start->x == end->x)
    {
        rect.bottom = end->y;
        order_end_points(&rect.top, &rect.bottom);
        add_rect_to_region( region, &rect );
    }
    else
    {
        INT dx = end->x - start->x, dy = end->y - start->y;
        INT abs_dx = abs(dx), abs_dy = abs(dy);
        DWORD octant = get_octant_mask(dx, dy);
        INT bias = get_bias( octant );

        if (is_xmajor( octant ))
        {
            int y_inc = is_y_increasing( octant ) ? 1 : -1;
            int err_add_1 = 2 * abs_dy - 2 * abs_dx;
            int err_add_2 = 2 * abs_dy;
            int err = 2 * abs_dy - abs_dx;

            if (is_x_increasing( octant ))
            {
                for (rect.right = start->x + 1; rect.right <= end->x; rect.right++)
                {
                    if (err + bias > 0)
                    {
                        add_rect_to_region( region, &rect );
                        rect.left = rect.right;
                        rect.top += y_inc;
                        rect.bottom += y_inc;
                        err += err_add_1;
                    }
                    else err += err_add_2;
                }
            }
            else
            {
                for (rect.left = start->x; rect.left > end->x; rect.left--)
                {
                    if (err + bias > 0)
                    {
                        add_rect_to_region( region, &rect );
                        rect.right = rect.left;
                        rect.top += y_inc;
                        rect.bottom += y_inc;
                        err += err_add_1;
                    }
                    else err += err_add_2;
                }
            }
        }
        else
        {
            int x_inc = is_x_increasing( octant ) ? 1 : -1;
            int err_add_1 = 2 * abs_dx - 2 * abs_dy;
            int err_add_2 = 2 * abs_dx;
            int err = 2 * abs_dx - abs_dy;

            if (is_y_increasing( octant ))
            {
                for (rect.bottom = start->y + 1; rect.bottom <= end->y; rect.bottom++)
                {
                    if (err + bias > 0)
                    {
                        add_rect_to_region( region, &rect );
                        rect.top = rect.bottom;
                        rect.left += x_inc;
                        rect.right += x_inc;
                        err += err_add_1;
                    }
                    else err += err_add_2;
                }
            }
            else
            {
                for (rect.top = start->y; rect.top > end->y; rect.top--)
                {
                    if (err + bias > 0)
                    {
                        add_rect_to_region( region, &rect );
                        rect.bottom = rect.top;
                        rect.left += x_inc;
                        rect.right += x_inc;
                        err += err_add_1;
                    }
                    else err += err_add_2;
                }
            }
        }
        /* add final rect */
        add_rect_to_region( region, &rect );
    }
    return TRUE;
}

static BOOL solid_pen_lines(dibdrv_physdev *pdev, int num, POINT *pts, BOOL close, HRGN region)
{
    int i;

    assert( num >= 2 );

    if (region)
    {
        for (i = 0; i < num - 1; i++)
            if (!solid_pen_line_region( pdev, pts + i, pts + i + 1, region ))
                return FALSE;
        if (close) return solid_pen_line_region( pdev, pts + num - 1, pts, region );
    }
    else
    {
        DWORD color, and, xor;

        color = get_pixel_color( pdev, pdev->pen_brush.colorref, TRUE );
        calc_and_xor_masks( GetROP2(pdev->dev.hdc), color, &and, &xor );

        for (i = 0; i < num - 1; i++)
            if (!solid_pen_line( pdev, pts + i, pts + i + 1, and, xor ))
                return FALSE;
        if (close) return solid_pen_line( pdev, pts + num - 1, pts, and, xor );
    }
    return TRUE;
}

void reset_dash_origin(dibdrv_physdev *pdev)
{
    pdev->dash_pos.cur_dash = 0;
    pdev->dash_pos.left_in_dash = pdev->pen_pattern.dashes[0];
    pdev->dash_pos.mark = TRUE;
}

static inline void skip_dash(dibdrv_physdev *pdev, unsigned int skip)
{
    skip %= pdev->pen_pattern.total_len;
    do
    {
        if(pdev->dash_pos.left_in_dash > skip)
        {
            pdev->dash_pos.left_in_dash -= skip;
            return;
        }
        skip -= pdev->dash_pos.left_in_dash;
        pdev->dash_pos.cur_dash++;
        if(pdev->dash_pos.cur_dash == pdev->pen_pattern.count) pdev->dash_pos.cur_dash = 0;
        pdev->dash_pos.left_in_dash = pdev->pen_pattern.dashes[pdev->dash_pos.cur_dash];
        pdev->dash_pos.mark = !pdev->dash_pos.mark;
    }
    while (skip);
}

static void dashed_pen_line_callback(dibdrv_physdev *pdev, INT x, INT y)
{
    RECT rect;
    rop_mask mask = pdev->dash_masks[pdev->dash_pos.mark];

    skip_dash(pdev, 1);
    rect.left   = x;
    rect.right  = x + 1;
    rect.top    = y;
    rect.bottom = y + 1;
    pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, mask.and, mask.xor);
    return;
}

static BOOL dashed_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end)
{
    struct clipped_rects clipped_rects;
    int i, dash_len;
    RECT rect;
    const dash_pos start_pos = pdev->dash_pos;

    if(start->y == end->y) /* hline */
    {
        BOOL l_to_r;
        INT left, right, cur_x;

        rect.top = start->y;
        rect.bottom = start->y + 1;

        if(start->x <= end->x)
        {
            left = start->x;
            right = end->x - 1;
            l_to_r = TRUE;
        }
        else
        {
            left = end->x + 1;
            right = start->x;
            l_to_r = FALSE;
        }

        rect.left = min( start->x, end->x );
        rect.right = max( start->x, end->x ) + 1;
        get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects );
        for (i = 0; i < clipped_rects.count; i++)
        {
            if(clipped_rects.rects[i].right > left && clipped_rects.rects[i].left <= right)
            {
                int clipped_left  = max(clipped_rects.rects[i].left, left);
                int clipped_right = min(clipped_rects.rects[i].right - 1, right);

                pdev->dash_pos = start_pos;

                if(l_to_r)
                {
                    cur_x = clipped_left;
                    if(cur_x != left)
                        skip_dash(pdev, clipped_left - left);

                    while(cur_x <= clipped_right)
                    {
                        rop_mask mask = pdev->dash_masks[pdev->dash_pos.mark];
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_x + dash_len > clipped_right + 1)
                            dash_len = clipped_right - cur_x + 1;
                        rect.left = cur_x;
                        rect.right = cur_x + dash_len;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, mask.and, mask.xor);
                        cur_x += dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
                else
                {
                    cur_x = clipped_right;
                    if(cur_x != right)
                        skip_dash(pdev, right - clipped_right);

                    while(cur_x >= clipped_left)
                    {
                        rop_mask mask = pdev->dash_masks[pdev->dash_pos.mark];
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_x - dash_len < clipped_left - 1)
                            dash_len = cur_x - clipped_left + 1;
                        rect.left = cur_x - dash_len + 1;
                        rect.right = cur_x + 1;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, mask.and, mask.xor);
                        cur_x -= dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
            }
        }
        pdev->dash_pos = start_pos;
        skip_dash(pdev, right - left + 1);
    }
    else if(start->x == end->x) /* vline */
    {
        BOOL t_to_b;
        INT top, bottom, cur_y;

        rect.left = start->x;
        rect.right = start->x + 1;

        if(start->y <= end->y)
        {
            top = start->y;
            bottom = end->y - 1;
            t_to_b = TRUE;
        }
        else
        {
            top = end->y + 1;
            bottom = start->y;
            t_to_b = FALSE;
        }

        rect.top = min( start->y, end->y );
        rect.bottom = max( start->y, end->y ) + 1;
        get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects );
        for (i = 0; i < clipped_rects.count; i++)
        {
            if(clipped_rects.rects[i].right > start->x && clipped_rects.rects[i].left <= start->x)
            {
                int clipped_top    = max(clipped_rects.rects[i].top, top);
                int clipped_bottom = min(clipped_rects.rects[i].bottom - 1, bottom);

                pdev->dash_pos = start_pos;

                if(t_to_b)
                {
                    cur_y = clipped_top;
                    if(cur_y != top)
                        skip_dash(pdev, clipped_top - top);

                    while(cur_y <= clipped_bottom)
                    {
                        rop_mask mask = pdev->dash_masks[pdev->dash_pos.mark];
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_y + dash_len > clipped_bottom + 1)
                            dash_len = clipped_bottom - cur_y + 1;
                        rect.top = cur_y;
                        rect.bottom = cur_y + dash_len;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, mask.and, mask.xor);
                        cur_y += dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
                else
                {
                    cur_y = clipped_bottom;
                    if(cur_y != bottom)
                        skip_dash(pdev, bottom - clipped_bottom);

                    while(cur_y >= clipped_top)
                    {
                        rop_mask mask = pdev->dash_masks[pdev->dash_pos.mark];
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_y - dash_len < clipped_top - 1)
                            dash_len = cur_y - clipped_top + 1;
                        rect.top = cur_y - dash_len + 1;
                        rect.bottom = cur_y + 1;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, mask.and, mask.xor);
                        cur_y -= dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
            }
        }
        pdev->dash_pos = start_pos;
        skip_dash(pdev, bottom - top + 1);
    }
    else
    {
        bres_params clip_params;
        struct line_params line_params;
        INT dx = end->x - start->x, dy = end->y - start->y;
        INT abs_dx = abs(dx), abs_dy = abs(dy);

        clip_params.dx = abs_dx;
        clip_params.dy = abs_dy;
        clip_params.octant = get_octant_mask(dx, dy);
        clip_params.bias   = get_bias( clip_params.octant );

        line_params.bias    = clip_params.bias;
        line_params.x_major = is_xmajor( clip_params.octant );
        line_params.x_inc   = is_x_increasing( clip_params.octant ) ? 1 : -1;
        line_params.y_inc   = is_y_increasing( clip_params.octant ) ? 1 : -1;

        if (line_params.x_major)
        {
            line_params.err_add_1 = 2 * abs_dy - 2 * abs_dx;
            line_params.err_add_2 = 2 * abs_dy;
        }
        else
        {
            line_params.err_add_1 = 2 * abs_dx - 2 * abs_dy;
            line_params.err_add_2 = 2 * abs_dx;
        }

        rect.left   = min( start->x, end->x );
        rect.top    = min( start->y, end->y );
        rect.right  = max( start->x, end->x ) + 1;
        rect.bottom = max( start->y, end->y ) + 1;
        get_clipped_rects( &pdev->dib, &rect, pdev->clip, &clipped_rects );
        for (i = 0; i < clipped_rects.count; i++)
        {
            POINT clipped_start, clipped_end;
            int clip_status;
            clip_status = clip_line(start, end, clipped_rects.rects + i, &clip_params, &clipped_start, &clipped_end);

            if(clip_status)
            {
                int m = abs(clipped_start.x - start->x);
                int n = abs(clipped_start.y - start->y);

                pdev->dash_pos = start_pos;

                if (line_params.x_major)
                {
                    line_params.err_start = 2 * abs_dy - abs_dx + m * 2 * abs_dy - n * 2 * abs_dx;
                    line_params.length = abs( clipped_end.x - clipped_start.x ) + 1;
                    skip_dash(pdev, m);
                }
                else
                {
                    line_params.err_start = 2 * abs_dx - abs_dy + n * 2 * abs_dx - m * 2 * abs_dy;
                    line_params.length = abs( clipped_end.y - clipped_start.y ) + 1;
                    skip_dash(pdev, n);
                }
                if (clipped_end.x == end->x && clipped_end.y == end->y) line_params.length--;

                bres_line_with_bias( &clipped_start, &line_params, dashed_pen_line_callback, pdev );

                if(clip_status == 2) break; /* completely unclipped, so we can finish */
            }
        }
        pdev->dash_pos = start_pos;
        if(line_params.x_major)
            skip_dash(pdev, abs_dx);
        else
            skip_dash(pdev, abs_dy);
    }

    free_clipped_rects( &clipped_rects );
    return TRUE;
}

static BOOL dashed_pen_line_region(dibdrv_physdev *pdev, POINT *start, POINT *end, HRGN region)
{
    int i, dash_len;
    RECT rect;

    rect.left   = start->x;
    rect.top    = start->y;
    rect.right  = start->x + 1;
    rect.bottom = start->y + 1;

    if (start->y == end->y) /* hline */
    {
        if (start->x <= end->x)
        {
            for (i = start->x; i < end->x; i += dash_len)
            {
                dash_len = min( pdev->dash_pos.left_in_dash, end->x - i );
                if (pdev->dash_pos.mark)
                {
                    rect.left = i;
                    rect.right = i + dash_len;
                    add_rect_to_region( region, &rect );
                }
                skip_dash(pdev, dash_len);
            }
        }
        else
        {
            for (i = start->x; i > end->x; i -= dash_len)
            {
                dash_len = min( pdev->dash_pos.left_in_dash, i - end->x );
                if (pdev->dash_pos.mark)
                {
                    rect.left = i - dash_len + 1;
                    rect.right = i + 1;
                    add_rect_to_region( region, &rect );
                }
                skip_dash(pdev, dash_len);
            }
        }
    }
    else if (start->x == end->x) /* vline */
    {
        if (start->y <= end->y)
        {
            for (i = start->y; i < end->y; i += dash_len)
            {
                dash_len = min( pdev->dash_pos.left_in_dash, end->y - i );
                if (pdev->dash_pos.mark)
                {
                    rect.top = i;
                    rect.bottom = i + dash_len;
                    add_rect_to_region( region, &rect );
                }
                skip_dash(pdev, dash_len);
            }
        }
        else
        {
            for (i = start->y; i > end->y; i -= dash_len)
            {
                dash_len = min( pdev->dash_pos.left_in_dash, i - end->y );
                if (pdev->dash_pos.mark)
                {
                    rect.top = i - dash_len + 1;
                    rect.bottom = i + 1;
                    add_rect_to_region( region, &rect );
                }
                skip_dash(pdev, dash_len);
            }
        }
    }
    else
    {
        INT dx = end->x - start->x, dy = end->y - start->y;
        INT abs_dx = abs(dx), abs_dy = abs(dy);
        DWORD octant = get_octant_mask(dx, dy);
        INT bias = get_bias( octant );
        int x_inc = is_x_increasing( octant ) ? 1 : -1;
        int y_inc = is_y_increasing( octant ) ? 1 : -1;

        if (is_xmajor( octant ))
        {
            int err_add_1 = 2 * abs_dy - 2 * abs_dx;
            int err_add_2 = 2 * abs_dy;
            int err = 2 * abs_dy - abs_dx;

            while (abs_dx--)
            {
                if (pdev->dash_pos.mark) add_rect_to_region( region, &rect );
                skip_dash(pdev, 1);
                rect.left += x_inc;
                rect.right += x_inc;
                if (err + bias > 0)
                {
                    rect.top += y_inc;
                    rect.bottom += y_inc;
                    err += err_add_1;
                }
                else err += err_add_2;
            }

        }
        else
        {
            int err_add_1 = 2 * abs_dx - 2 * abs_dy;
            int err_add_2 = 2 * abs_dx;
            int err = 2 * abs_dx - abs_dy;

            while (abs_dy--)
            {
                if (pdev->dash_pos.mark) add_rect_to_region( region, &rect );
                skip_dash(pdev, 1);
                rect.top += y_inc;
                rect.bottom += y_inc;
                if (err + bias > 0)
                {
                    rect.left += x_inc;
                    rect.right += x_inc;
                    err += err_add_1;
                }
                else err += err_add_2;
            }
        }
    }
    return TRUE;
}

static BOOL dashed_pen_lines(dibdrv_physdev *pdev, int num, POINT *pts, BOOL close, HRGN region)
{
    int i;

    assert( num >= 2 );

    if (region)
    {
        for (i = 0; i < num - 1; i++)
            if (!dashed_pen_line_region( pdev, pts + i, pts + i + 1, region ))
                return FALSE;
        if (close) return dashed_pen_line_region( pdev, pts + num - 1, pts, region );
    }
    else
    {
        get_color_masks( pdev, GetROP2(pdev->dev.hdc), pdev->pen_brush.colorref,
                         pdev->pen_is_ext ? TRANSPARENT : GetBkMode(pdev->dev.hdc),
                         &pdev->dash_masks[1], &pdev->dash_masks[0] );

        for (i = 0; i < num - 1; i++)
            if (!dashed_pen_line( pdev, pts + i, pts + i + 1 ))
                return FALSE;
        if (close) return dashed_pen_line( pdev, pts + num - 1, pts );
    }
    return TRUE;
}

static BOOL null_pen_lines(dibdrv_physdev *pdev, int num, POINT *pts, BOOL close, HRGN region)
{
    return TRUE;
}

struct face
{
    POINT start, end;
    int dx, dy;
};

static void add_cap( dibdrv_physdev *pdev, HRGN region, HRGN round_cap, const POINT *pt )
{
    switch (pdev->pen_endcap)
    {
    default: FIXME( "Unknown end cap %x\n", pdev->pen_endcap );
        /* fall through */
    case PS_ENDCAP_ROUND:
        OffsetRgn( round_cap, pt->x, pt->y );
        CombineRgn( region, region, round_cap, RGN_OR );
        OffsetRgn( round_cap, -pt->x, -pt->y );
        return;

    case PS_ENDCAP_SQUARE: /* already been handled */
    case PS_ENDCAP_FLAT:
        return;
    }
}

#define round( f ) (((f) > 0) ? (f) + 0.5 : (f) - 0.5)

/*******************************************************************************
 *                 create_miter_region
 *
 * We need to calculate the intersection of two lines.  We know a point
 * on each line (a face start and the other face end point) and
 * the direction vector of each line eg. (dx_1, dy_1).
 *
 * (x, y) = (x_1, y_1) + u * (dx_1, dy_1) = (x_2, y_2) + v * (dx_2, dy_2)
 * solving (eg using Cramer's rule) gives:
 * u = ((x_2 - x_1) dy_2 - (y_2 - y_1) dx_2) / det
 * with det = dx_1 dy_2 - dx_2 dy_1
 * substituting back in and simplifying gives
 * (x, y) = a (dx_1, dy_1) - b (dx_2, dy_2)
 * with a = (x_2 dy_2 - y_2 dx_2) / det
 * and  b = (x_1 dy_1 - y_1 dx_1) / det
 */
static HRGN create_miter_region( dibdrv_physdev *pdev, const POINT *pt,
                                 const struct face *face_1, const struct face *face_2 )
{
    int det = face_1->dx * face_2->dy - face_1->dy * face_2->dx;
    POINT pt_1, pt_2, pts[5];
    double a, b, x, y;
    FLOAT limit;

    if (det == 0) return 0;

    if (det < 0)
    {
        const struct face *tmp = face_1;
        face_1 = face_2;
        face_2 = tmp;
        det = -det;
    }

    pt_1 = face_1->start;
    pt_2 = face_2->end;

    a = (double)((pt_2.x * face_2->dy - pt_2.y * face_2->dx)) / det;
    b = (double)((pt_1.x * face_1->dy - pt_1.y * face_1->dx)) / det;

    x = a * face_1->dx - b * face_2->dx;
    y = a * face_1->dy - b * face_2->dy;

    GetMiterLimit( pdev->dev.hdc, &limit );

    if (((x - pt->x) * (x - pt->x) + (y - pt->y) * (y - pt->y)) * 4 > limit * limit * pdev->pen_width * pdev->pen_width)
        return 0;

    pts[0] = face_2->start;
    pts[1] = face_1->start;
    pts[2].x = round( x );
    pts[2].y = round( y );
    pts[3] = face_2->end;
    pts[4] = face_1->end;

    return CreatePolygonRgn( pts, 5, ALTERNATE );
}

static void add_join( dibdrv_physdev *pdev, HRGN region, HRGN round_cap, const POINT *pt,
                      const struct face *face_1, const struct face *face_2 )
{
    HRGN join;
    POINT pts[4];

    switch (pdev->pen_join)
    {
    default: FIXME( "Unknown line join %x\n", pdev->pen_join );
        /* fall through */
    case PS_JOIN_ROUND:
        OffsetRgn( round_cap, pt->x, pt->y );
        CombineRgn( region, region, round_cap, RGN_OR );
        OffsetRgn( round_cap, -pt->x, -pt->y );
        return;

    case PS_JOIN_MITER:
        join = create_miter_region( pdev, pt, face_1, face_2 );
        if (join) break;
        /* fall through */
    case PS_JOIN_BEVEL:
        pts[0] = face_1->start;
        pts[1] = face_2->end;
        pts[2] = face_1->end;
        pts[3] = face_2->start;
        join = CreatePolygonRgn( pts, 4, ALTERNATE );
        break;
    }

    CombineRgn( region, region, join, RGN_OR );
    DeleteObject( join );
    return;
}

static void wide_line_segment( dibdrv_physdev *pdev, HRGN total,
                               const POINT *pt_1, const POINT *pt_2, int dx, int dy,
                               BOOL need_cap_1, BOOL need_cap_2, struct face *face_1, struct face *face_2 )
{
    RECT rect;
    BOOL sq_cap_1 = need_cap_1 && (pdev->pen_endcap == PS_ENDCAP_SQUARE);
    BOOL sq_cap_2 = need_cap_2 && (pdev->pen_endcap == PS_ENDCAP_SQUARE);

    if (dx == 0 && dy == 0) return;

    if (dy == 0)
    {
        rect.left = min( pt_1->x, pt_2->x );
        rect.right = max( pt_1->x, pt_2->x );
        rect.top = pt_1->y - pdev->pen_width / 2;
        rect.bottom = rect.top + pdev->pen_width;
        if ((sq_cap_1 && dx > 0) || (sq_cap_2 && dx < 0)) rect.left  -= pdev->pen_width / 2;
        if ((sq_cap_2 && dx > 0) || (sq_cap_1 && dx < 0)) rect.right += pdev->pen_width / 2;
        add_rect_to_region( total, &rect );
        if (dx > 0)
        {
            face_1->start.x = face_1->end.x   = rect.left;
            face_1->start.y = face_2->end.y   = rect.bottom;
            face_1->end.y   = face_2->start.y = rect.top;
            face_2->start.x = face_2->end.x   = rect.right - 1;
        }
        else
        {
            face_1->start.x = face_1->end.x   = rect.right;
            face_1->start.y = face_2->end.y   = rect.top;
            face_1->end.y   = face_2->start.y = rect.bottom;
            face_2->start.x = face_2->end.x   = rect.left + 1;
        }
    }
    else if (dx == 0)
    {
        rect.top = min( pt_1->y, pt_2->y );
        rect.bottom = max( pt_1->y, pt_2->y );
        rect.left = pt_1->x - pdev->pen_width / 2;
        rect.right = rect.left + pdev->pen_width;
        if ((sq_cap_1 && dy > 0) || (sq_cap_2 && dy < 0)) rect.top    -= pdev->pen_width / 2;
        if ((sq_cap_2 && dy > 0) || (sq_cap_1 && dy < 0)) rect.bottom += pdev->pen_width / 2;
        add_rect_to_region( total, &rect );
        if (dy > 0)
        {
            face_1->start.x = face_2->end.x   = rect.left;
            face_1->start.y = face_1->end.y   = rect.top;
            face_1->end.x   = face_2->start.x = rect.right;
            face_2->start.y = face_2->end.y   = rect.bottom - 1;
        }
        else
        {
            face_1->start.x = face_2->end.x   = rect.right;
            face_1->start.y = face_1->end.y   = rect.bottom;
            face_1->end.x   = face_2->start.x = rect.left;
            face_2->start.y = face_2->end.y   = rect.top + 1;
        }
    }
    else
    {
        double len = hypot( dx, dy );
        double width_x, width_y;
        POINT seg_pts[4];
        POINT wide_half, narrow_half;
        HRGN segment;

        width_x = pdev->pen_width * abs( dy ) / len;
        width_y = pdev->pen_width * abs( dx ) / len;

        narrow_half.x = round( width_x / 2 );
        narrow_half.y = round( width_y / 2 );
        wide_half.x   = round( (width_x + 1) / 2 );
        wide_half.y   = round( (width_y + 1) / 2 );

        if (dx < 0)
        {
            wide_half.y   = -wide_half.y;
            narrow_half.y = -narrow_half.y;
        }

        if (dy < 0)
        {
            POINT tmp = narrow_half; narrow_half = wide_half; wide_half = tmp;
            wide_half.x   = -wide_half.x;
            narrow_half.x = -narrow_half.x;
        }

        seg_pts[0].x = pt_1->x - narrow_half.x;
        seg_pts[0].y = pt_1->y + narrow_half.y;
        seg_pts[1].x = pt_1->x + wide_half.x;
        seg_pts[1].y = pt_1->y - wide_half.y;
        seg_pts[2].x = pt_2->x + wide_half.x;
        seg_pts[2].y = pt_2->y - wide_half.y;
        seg_pts[3].x = pt_2->x - narrow_half.x;
        seg_pts[3].y = pt_2->y + narrow_half.y;

        if (sq_cap_1)
        {
            seg_pts[0].x -= narrow_half.y;
            seg_pts[1].x -= narrow_half.y;
            seg_pts[0].y -= narrow_half.x;
            seg_pts[1].y -= narrow_half.x;
        }

        if (sq_cap_2)
        {
            seg_pts[2].x += wide_half.y;
            seg_pts[3].x += wide_half.y;
            seg_pts[2].y += wide_half.x;
            seg_pts[3].y += wide_half.x;
        }

        segment = CreatePolygonRgn( seg_pts, 4, ALTERNATE );
        CombineRgn( total, total, segment, RGN_OR );
        DeleteObject( segment );

        face_1->start = seg_pts[0];
        face_1->end   = seg_pts[1];
        face_2->start = seg_pts[2];
        face_2->end   = seg_pts[3];
    }

    face_1->dx = face_2->dx = dx;
    face_1->dy = face_2->dy = dy;
}

static void wide_line_segments( dibdrv_physdev *pdev, int num, const POINT *pts, BOOL close,
                                int start, int count, const POINT *first_pt, const POINT *last_pt,
                                HRGN round_cap, HRGN total )
{
    int i;
    struct face face_1, face_2, prev_face, first_face;
    const POINT *pt_1, *pt_2;

    if (!close)
    {
        add_cap( pdev, total, round_cap, first_pt );
        add_cap( pdev, total, round_cap, last_pt );
    }

    if (count == 1)
    {
        pt_1 = &pts[start];
        pt_2 = &pts[(start + 1) % num];
        wide_line_segment( pdev, total, first_pt, last_pt, pt_2->x - pt_1->x, pt_2->y - pt_1->y,
                           TRUE, TRUE, &face_1, &face_2 );
        return;
    }

    pt_1 = &pts[start];
    pt_2 = &pts[(start + 1) % num];
    wide_line_segment( pdev, total, first_pt, pt_2, pt_2->x - pt_1->x, pt_2->y - pt_1->y,
                       !close, FALSE, &first_face, &prev_face );


    for (i = 1; i < count - 1; i++)
    {
        pt_1 = &pts[(start + i) % num];
        pt_2 = &pts[(start + i + 1) % num];
        wide_line_segment( pdev, total, pt_1, pt_2, pt_2->x - pt_1->x, pt_2->y - pt_1->y,
                           FALSE, FALSE, &face_1, &face_2 );
        add_join( pdev, total, round_cap, pt_1, &prev_face, &face_1 );
        prev_face = face_2;
    }

    pt_1 = &pts[(start + count - 1) % num];
    pt_2 = &pts[(start + count) % num];
    wide_line_segment( pdev, total, pt_1, last_pt, pt_2->x - pt_1->x, pt_2->y - pt_1->y,
                       FALSE, !close, &face_1, &face_2 );
    add_join( pdev, total, round_cap, pt_1, &prev_face, &face_1 );
    if (close) add_join( pdev, total, round_cap, last_pt, &face_2, &first_face );
}

static BOOL wide_pen_lines(dibdrv_physdev *pdev, int num, POINT *pts, BOOL close, HRGN total)
{
    HRGN round_cap = 0;

    assert( total != 0 );  /* wide pens should always be drawn through a region */
    assert( num >= 2 );

    /* skip empty segments */
    while (num > 2 && pts[0].x == pts[1].x && pts[0].y == pts[1].y) { pts++; num--; }
    while (num > 2 && pts[num - 1].x == pts[num - 2].x && pts[num - 1].y == pts[num - 2].y) num--;

    if (pdev->pen_join == PS_JOIN_ROUND || pdev->pen_endcap == PS_ENDCAP_ROUND)
        round_cap = CreateEllipticRgn( -(pdev->pen_width / 2), -(pdev->pen_width / 2),
                                       (pdev->pen_width + 1) / 2 + 1, (pdev->pen_width + 1) / 2 + 1 );

    if (close)
        wide_line_segments( pdev, num, pts, TRUE, 0, num, &pts[0], &pts[0], round_cap, total );
    else
        wide_line_segments( pdev, num, pts, FALSE, 0, num - 1, &pts[0], &pts[num - 1], round_cap, total );

    if (round_cap) DeleteObject( round_cap );
    return TRUE;
}

static BOOL dashed_wide_pen_lines(dibdrv_physdev *pdev, int num, POINT *pts, BOOL close, HRGN total)
{
    int i, start, cur_len, initial_num = 0;
    POINT initial_point, start_point, end_point;
    HRGN round_cap = 0;

    assert( total != 0 );  /* wide pens should always be drawn through a region */
    assert( num >= 2 );

    /* skip empty segments */
    while (num > 2 && pts[0].x == pts[1].x && pts[0].y == pts[1].y) { pts++; num--; }
    while (num > 2 && pts[num - 1].x == pts[num - 2].x && pts[num - 1].y == pts[num - 2].y) num--;

    if (pdev->pen_join == PS_JOIN_ROUND || pdev->pen_endcap == PS_ENDCAP_ROUND)
        round_cap = CreateEllipticRgn( -(pdev->pen_width / 2), -(pdev->pen_width / 2),
                                       (pdev->pen_width + 1) / 2 + 1, (pdev->pen_width + 1) / 2 + 1);

    start = 0;
    cur_len = 0;
    start_point = pts[0];

    for (i = 0; i < (close ? num : num - 1); i++)
    {
        const POINT *pt_1 = pts + i;
        const POINT *pt_2 = pts + ((close && i == num - 1) ? 0 : i + 1);
        int dx = pt_2->x - pt_1->x;
        int dy = pt_2->y - pt_1->y;

        if (dx == 0 && dy == 0) continue;

        if (dy == 0)
        {
            if (abs( dx ) - cur_len < pdev->dash_pos.left_in_dash)
            {
                skip_dash( pdev, abs( dx ) - cur_len );
                cur_len = 0;
                continue;
            }
            cur_len += pdev->dash_pos.left_in_dash;
            dx = (dx > 0) ? cur_len : -cur_len;
        }
        else if (dx == 0)
        {
            if (abs( dy ) - cur_len < pdev->dash_pos.left_in_dash)
            {
                skip_dash( pdev, abs( dy ) - cur_len );
                cur_len = 0;
                continue;
            }
            cur_len += pdev->dash_pos.left_in_dash;
            dy = (dy > 0) ? cur_len : -cur_len;
        }
        else
        {
            double len = hypot( dx, dy );

            if (len - cur_len < pdev->dash_pos.left_in_dash)
            {
                skip_dash( pdev, len - cur_len );
                cur_len = 0;
                continue;
            }
            cur_len += pdev->dash_pos.left_in_dash;
            dx = dx * cur_len / len;
            dy = dy * cur_len / len;
        }
        end_point.x = pt_1->x + dx;
        end_point.y = pt_1->y + dy;

        if (pdev->dash_pos.mark)
        {
            if (!initial_num && close)  /* this is the first dash, save it for later */
            {
                initial_num = i - start + 1;
                initial_point = end_point;
            }
            else wide_line_segments( pdev, num, pts, FALSE, start, i - start + 1,
                                     &start_point, &end_point, round_cap, total );
        }
        if (!initial_num) initial_num = -1;  /* no need to close it */

        skip_dash( pdev, pdev->dash_pos.left_in_dash );
        start_point = end_point;
        start = i;
        i--;  /* go on with the same segment */
    }

    if (pdev->dash_pos.mark)  /* we have a final dash */
    {
        int count;

        if (initial_num > 0)
        {
            count = num - start + initial_num;
            end_point = initial_point;
        }
        else if (close)
        {
            count = num - start;
            end_point = pts[0];
        }
        else
        {
            count = num - start - 1;
            end_point = pts[num - 1];
        }
        wide_line_segments( pdev, num, pts, FALSE, start, count,
                            &start_point, &end_point, round_cap, total );
    }
    else if (initial_num > 0)  /* initial dash only */
    {
        wide_line_segments( pdev, num, pts, FALSE, 0, initial_num,
                            &pts[0], &initial_point, round_cap, total );
    }

    if (round_cap) DeleteObject( round_cap );
    return TRUE;
}

static const dash_pattern dash_patterns_cosmetic[4] =
{
    {2, {18, 6}, 24},             /* PS_DASH */
    {2, {3,  3}, 6},              /* PS_DOT */
    {4, {9, 6, 3, 6}, 24},        /* PS_DASHDOT */
    {6, {9, 3, 3, 3, 3, 3}, 24}   /* PS_DASHDOTDOT */
};

static const dash_pattern dash_patterns_geometric[4] =
{
    {2, {3, 1}, 4},               /* PS_DASH */
    {2, {1, 1}, 2},               /* PS_DOT */
    {4, {3, 1, 1, 1}, 6},         /* PS_DASHDOT */
    {6, {3, 1, 1, 1, 1, 1}, 8}    /* PS_DASHDOTDOT */
};

static inline void set_dash_pattern( dash_pattern *pattern, DWORD count, DWORD *dashes )
{
    DWORD i;

    pattern->count = count;
    pattern->total_len = 0;
    memcpy( pattern->dashes, dashes, count * sizeof(DWORD) );
    for (i = 0; i < count; i++) pattern->total_len += dashes[i];
    if (pattern->count % 2) pattern->total_len *= 2;
}

static inline void scale_dash_pattern( dash_pattern *pattern, DWORD scale, DWORD endcap )
{
    DWORD i;

    for (i = 0; i < pattern->count; i++) pattern->dashes[i] *= scale;
    pattern->total_len *= scale;

    if (endcap != PS_ENDCAP_FLAT)  /* shrink the dashes to leave room for the caps */
    {
        for (i = 0; i < pattern->count; i += 2)
        {
            pattern->dashes[i] -= scale;
            pattern->dashes[i + 1] += scale;
        }
    }
}

static inline int get_pen_device_width( dibdrv_physdev *pdev, int width )
{
    POINT pts[2];

    if (!width) return 1;
    pts[0].x = pts[0].y = pts[1].y = 0;
    pts[1].x = width;
    LPtoDP( pdev->dev.hdc, pts, 2 );
    width = abs( pts[1].x - pts[0].x );
    return max( width, 1 );
}

/***********************************************************************
 *           dibdrv_SetDCPenColor
 */
COLORREF dibdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_PEN) == GetStockObject( DC_PEN ))
        pdev->pen_brush.colorref = color;

    return color;
}

/**********************************************************************
 *             solid_brush
 *
 * Fill a number of rectangles with the solid brush
 */
static BOOL solid_brush(dibdrv_physdev *pdev, dib_brush *brush, dib_info *dib,
                        int num, const RECT *rects, INT rop)
{
    rop_mask brush_color;
    DWORD color = get_pixel_color( pdev, brush->colorref, TRUE );

    calc_rop_masks( rop, color, &brush_color );
    dib->funcs->solid_rects( dib, num, rects, brush_color.and, brush_color.xor );
    return TRUE;
}

static void free_pattern_brush_bits( dib_brush *brush )
{
    HeapFree(GetProcessHeap(), 0, brush->and_bits);
    HeapFree(GetProcessHeap(), 0, brush->xor_bits);
    brush->and_bits = NULL;
    brush->xor_bits = NULL;
}

void free_pattern_brush( dib_brush *brush )
{
    free_pattern_brush_bits( brush );
    free_dib_info( &brush->dib );
}

static BOOL create_pattern_brush_bits( dib_brush *brush )
{
    DWORD size = brush->dib.height * abs(brush->dib.stride);
    DWORD *brush_bits = brush->dib.bits.ptr;
    DWORD *and_bits, *xor_bits;

    assert(brush->and_bits == NULL);
    assert(brush->xor_bits == NULL);

    assert(brush->dib.stride > 0);

    and_bits = brush->and_bits = HeapAlloc(GetProcessHeap(), 0, size);
    xor_bits = brush->xor_bits = HeapAlloc(GetProcessHeap(), 0, size);

    if(!and_bits || !xor_bits)
    {
        ERR("Failed to create pattern brush bits\n");
        free_pattern_brush_bits( brush );
        return FALSE;
    }

    while(size)
    {
        calc_and_xor_masks(brush->rop, *brush_bits++, and_bits++, xor_bits++);
        size -= 4;
    }

    return TRUE;
}

static const DWORD hatches[6][8] =
{
    { 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00 }, /* HS_HORIZONTAL */
    { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 }, /* HS_VERTICAL   */
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 }, /* HS_FDIAGONAL  */
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }, /* HS_BDIAGONAL  */
    { 0x08, 0x08, 0x08, 0xff, 0x08, 0x08, 0x08, 0x08 }, /* HS_CROSS      */
    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 }  /* HS_DIAGCROSS  */
};

static BOOL create_hatch_brush_bits(dibdrv_physdev *pdev, dib_brush *brush, BOOL *needs_reselect)
{
    dib_info hatch;
    rop_mask fg_mask, bg_mask;
    rop_mask_bits mask_bits;
    DWORD size;
    BOOL ret;

    assert(brush->and_bits == NULL);
    assert(brush->xor_bits == NULL);

    /* Just initialise brush dib with the color / sizing info.  We don't
       need the bits as we'll calculate the rop masks straight from
       the hatch patterns. */

    copy_dib_color_info(&brush->dib, &pdev->dib);
    brush->dib.width  = 8;
    brush->dib.height = 8;
    brush->dib.stride = get_dib_stride( brush->dib.width, brush->dib.bit_count );

    size = brush->dib.height * brush->dib.stride;

    mask_bits.and = brush->and_bits = HeapAlloc(GetProcessHeap(), 0, size);
    mask_bits.xor = brush->xor_bits = HeapAlloc(GetProcessHeap(), 0, size);

    if(!mask_bits.and || !mask_bits.xor)
    {
        ERR("Failed to create pattern brush bits\n");
        free_pattern_brush_bits( brush );
        return FALSE;
    }

    hatch.bit_count = 1;
    hatch.height = hatch.width = 8;
    hatch.stride = 4;
    hatch.bits.ptr = (void *) hatches[brush->hatch];
    hatch.bits.free = hatch.bits.param = NULL;
    hatch.bits.is_copy = FALSE;

    get_color_masks( pdev, brush->rop, brush->colorref, GetBkMode(pdev->dev.hdc),
                     &fg_mask, &bg_mask );

    if (brush->colorref & (1 << 24))  /* PALETTEINDEX */
        *needs_reselect = TRUE;
    if (GetBkMode(pdev->dev.hdc) != TRANSPARENT && (GetBkColor(pdev->dev.hdc) & (1 << 24)))
        *needs_reselect = TRUE;

    ret = brush->dib.funcs->create_rop_masks( &brush->dib, &hatch, &fg_mask, &bg_mask, &mask_bits );
    if(!ret) free_pattern_brush_bits( brush );

    return ret;
}

static BOOL matching_pattern_format( dib_info *dib, dib_info *pattern )
{
    if (dib->bit_count != pattern->bit_count) return FALSE;
    if (dib->stride != pattern->stride) return FALSE;

    switch (dib->bit_count)
    {
    case 1:
    case 4:
    case 8:
        if (dib->color_table_size != pattern->color_table_size) return FALSE;
        return !memcmp( dib->color_table, pattern->color_table, dib->color_table_size * sizeof(RGBQUAD) );
    case 16:
    case 32:
        return (dib->red_mask == pattern->red_mask &&
                dib->green_mask == pattern->green_mask &&
                dib->blue_mask == pattern->blue_mask);
    }
    return TRUE;
}

static BOOL select_pattern_brush( dibdrv_physdev *pdev, dib_brush *brush, BOOL *needs_reselect )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    RGBQUAD color_table[2];
    RECT rect;
    dib_info pattern;

    if (!brush->pattern.info)
    {
        BITMAPOBJ *bmp = GDI_GetObjPtr( brush->pattern.bitmap, OBJ_BITMAP );
        BOOL ret;

        if (!bmp) return FALSE;
        ret = init_dib_info_from_bitmapobj( &pattern, bmp, 0 );
        GDI_ReleaseObj( brush->pattern.bitmap );
        if (!ret) return FALSE;
    }
    else if (brush->pattern.info->bmiHeader.biClrUsed && brush->pattern.usage == DIB_PAL_COLORS)
    {
        copy_bitmapinfo( info, brush->pattern.info );
        fill_color_table_from_pal_colors( info, pdev->dev.hdc );
        init_dib_info_from_bitmapinfo( &pattern, info, brush->pattern.bits.ptr, 0 );
        *needs_reselect = TRUE;
    }
    else
    {
        init_dib_info_from_bitmapinfo( &pattern, brush->pattern.info, brush->pattern.bits.ptr, 0 );
    }

    if (pattern.bit_count == 1 && !pattern.color_table)
    {
        /* monochrome DDB pattern uses DC colors */
        DWORD pixel;
        BOOL got_pixel;
        COLORREF color;

        color = make_rgb_colorref( pdev->dev.hdc, &pdev->dib, GetTextColor( pdev->dev.hdc ),
                                   &got_pixel, &pixel );
        color_table[0].rgbRed      = GetRValue( color );
        color_table[0].rgbGreen    = GetGValue( color );
        color_table[0].rgbBlue     = GetBValue( color );
        color_table[0].rgbReserved = 0;

        color = make_rgb_colorref( pdev->dev.hdc, &pdev->dib, GetBkColor( pdev->dev.hdc ),
                                   &got_pixel, &pixel );
        color_table[1].rgbRed      = GetRValue( color );
        color_table[1].rgbGreen    = GetGValue( color );
        color_table[1].rgbBlue     = GetBValue( color );
        color_table[1].rgbReserved = 0;

        pattern.color_table = color_table;
        pattern.color_table_size = 2;
        *needs_reselect = TRUE;
    }

    copy_dib_color_info(&brush->dib, &pdev->dib);

    brush->dib.height = pattern.height;
    brush->dib.width  = pattern.width;
    brush->dib.stride = get_dib_stride( brush->dib.width, brush->dib.bit_count );

    if (matching_pattern_format( &brush->dib, &pattern ))
    {
        brush->dib.bits.ptr     = pattern.bits.ptr;
        brush->dib.bits.is_copy = FALSE;
        brush->dib.bits.free    = NULL;
    }
    else
    {
        brush->dib.bits.ptr     = HeapAlloc( GetProcessHeap(), 0, brush->dib.height * brush->dib.stride );
        brush->dib.bits.is_copy = TRUE;
        brush->dib.bits.free    = free_heap_bits;

        rect.left = rect.top = 0;
        rect.right = pattern.width;
        rect.bottom = pattern.height;

        brush->dib.funcs->convert_to(&brush->dib, &pattern, &rect);
    }
    return TRUE;
}

/**********************************************************************
 *             pattern_brush
 *
 * Fill a number of rectangles with the pattern brush
 * FIXME: Should we insist l < r && t < b?  Currently we assume this.
 */
static BOOL pattern_brush(dibdrv_physdev *pdev, dib_brush *brush, dib_info *dib,
                          int num, const RECT *rects, INT rop)
{
    POINT origin;
    BOOL needs_reselect = FALSE;

    if (rop != brush->rop)
    {
        free_pattern_brush_bits( brush );
        brush->rop = rop;
    }

    if(brush->and_bits == NULL)
    {
        switch(brush->style)
        {
        case BS_DIBPATTERN:
            if (!brush->dib.bits.ptr && !select_pattern_brush( pdev, brush, &needs_reselect ))
                return FALSE;
            if(!create_pattern_brush_bits( brush ))
                return FALSE;
            break;

        case BS_HATCHED:
            if(!create_hatch_brush_bits(pdev, brush, &needs_reselect))
                return FALSE;
            break;

        default:
            ERR("Unexpected brush style %d\n", brush->style);
            return FALSE;
        }
    }

    GetBrushOrgEx(pdev->dev.hdc, &origin);

    dib->funcs->pattern_rects( dib, num, rects, &origin, &brush->dib, brush->and_bits, brush->xor_bits );

    if (needs_reselect) free_pattern_brush( brush );
    return TRUE;
}

static BOOL null_brush(dibdrv_physdev *pdev, dib_brush *brush, dib_info *dib,
                       int num, const RECT *rects, INT rop)
{
    return TRUE;
}

static void select_brush( dib_brush *brush, const LOGBRUSH *logbrush, const struct brush_pattern *pattern )
{
    free_pattern_brush( brush );

    if (pattern)
    {
        brush->style   = BS_DIBPATTERN;
        brush->pattern = *pattern;  /* brush is actually selected only when it's used */
        brush->rects   = pattern_brush;
    }
    else
    {
        brush->style    = logbrush->lbStyle;
        brush->colorref = logbrush->lbColor;
        brush->hatch    = logbrush->lbHatch;

        switch (logbrush->lbStyle)
        {
        case BS_SOLID:   brush->rects = solid_brush; break;
        case BS_NULL:    brush->rects = null_brush; break;
        case BS_HATCHED: brush->rects = pattern_brush; break;
        }
    }
}

/***********************************************************************
 *           dibdrv_SelectBrush
 */
HBRUSH dibdrv_SelectBrush( PHYSDEV dev, HBRUSH hbrush, const struct brush_pattern *pattern )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGBRUSH logbrush;

    TRACE("(%p, %p)\n", dev, hbrush);

    GetObjectW( hbrush, sizeof(logbrush), &logbrush );

    if (hbrush == GetStockObject( DC_BRUSH ))
        logbrush.lbColor = GetDCBrushColor( dev->hdc );

    select_brush( &pdev->brush, &logbrush, pattern );
    return hbrush;
}

/***********************************************************************
 *           dibdrv_SelectPen
 */
HPEN dibdrv_SelectPen( PHYSDEV dev, HPEN hpen, const struct brush_pattern *pattern )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGPEN logpen;
    LOGBRUSH logbrush;
    EXTLOGPEN *elp = NULL;

    TRACE("(%p, %p)\n", dev, hpen);

    if (!GetObjectW( hpen, sizeof(logpen), &logpen ))
    {
        /* must be an extended pen */
        INT size = GetObjectW( hpen, 0, NULL );

        if (!size) return 0;

        elp = HeapAlloc( GetProcessHeap(), 0, size );

        GetObjectW( hpen, size, elp );
        logpen.lopnStyle = elp->elpPenStyle;
        logpen.lopnWidth.x = elp->elpWidth;
        /* cosmetic ext pens are always 1-pixel wide */
        if (!(logpen.lopnStyle & PS_GEOMETRIC)) logpen.lopnWidth.x = 0;

        logbrush.lbStyle = elp->elpBrushStyle;
        logbrush.lbColor = elp->elpColor;
        logbrush.lbHatch = elp->elpHatch;
    }
    else
    {
        logbrush.lbStyle = BS_SOLID;
        logbrush.lbColor = logpen.lopnColor;
        logbrush.lbHatch = 0;
    }

    pdev->pen_join   = logpen.lopnStyle & PS_JOIN_MASK;
    pdev->pen_endcap = logpen.lopnStyle & PS_ENDCAP_MASK;
    pdev->pen_width  = get_pen_device_width( pdev, logpen.lopnWidth.x );

    if (hpen == GetStockObject( DC_PEN ))
        logbrush.lbColor = GetDCPenColor( dev->hdc );

    set_dash_pattern( &pdev->pen_pattern, 0, NULL );
    select_brush( &pdev->pen_brush, &logbrush, pattern );

    pdev->pen_style = logpen.lopnStyle & PS_STYLE_MASK;

    switch (pdev->pen_style)
    {
    case PS_DASH:
    case PS_DOT:
    case PS_DASHDOT:
    case PS_DASHDOTDOT:
        if (logpen.lopnStyle & PS_GEOMETRIC)
        {
            pdev->pen_pattern = dash_patterns_geometric[pdev->pen_style - 1];
            if (pdev->pen_width > 1)
            {
                scale_dash_pattern( &pdev->pen_pattern, pdev->pen_width, pdev->pen_endcap );
                pdev->pen_lines = dashed_wide_pen_lines;
            }
            else pdev->pen_lines = dashed_pen_lines;
            break;
        }
        if (pdev->pen_width == 1)  /* wide cosmetic pens are not dashed */
        {
            pdev->pen_lines = dashed_pen_lines;
            pdev->pen_pattern = dash_patterns_cosmetic[pdev->pen_style - 1];
            break;
        }
        /* fall through */
    case PS_SOLID:
    case PS_INSIDEFRAME:
        pdev->pen_lines = (pdev->pen_width == 1) ? solid_pen_lines : wide_pen_lines;
        break;

    case PS_NULL:
        pdev->pen_width = 0;
        pdev->pen_lines = null_pen_lines;
        break;

    case PS_ALTERNATE:
        pdev->pen_lines = dashed_pen_lines;
        pdev->pen_pattern = dash_patterns_geometric[PS_DOT - 1];
        break;

    case PS_USERSTYLE:
        pdev->pen_lines = (pdev->pen_width == 1) ? dashed_pen_lines : dashed_wide_pen_lines;
        set_dash_pattern( &pdev->pen_pattern, elp->elpNumEntries, elp->elpStyleEntry );
        if (!(logpen.lopnStyle & PS_GEOMETRIC)) scale_dash_pattern( &pdev->pen_pattern, 3, PS_ENDCAP_FLAT );
        break;
    }

    pdev->pen_uses_region = (logpen.lopnStyle & PS_GEOMETRIC || pdev->pen_width > 1);
    pdev->pen_is_ext = (elp != NULL);
    HeapFree( GetProcessHeap(), 0, elp );
    return hpen;
}

/***********************************************************************
 *           dibdrv_SetDCBrushColor
 */
COLORREF dibdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_BRUSH) == GetStockObject( DC_BRUSH ))
        pdev->brush.colorref = color;

    return color;
}

BOOL brush_rect(dibdrv_physdev *pdev, dib_brush *brush, const RECT *rect, HRGN clip, INT rop)
{
    struct clipped_rects clipped_rects;
    BOOL ret;

    if (!get_clipped_rects( &pdev->dib, rect, clip, &clipped_rects )) return TRUE;
    ret = brush->rects( pdev, brush, &pdev->dib, clipped_rects.count, clipped_rects.rects, rop );
    free_clipped_rects( &clipped_rects );
    return ret;
}

/* paint a region with the brush (note: the region can be modified) */
BOOL brush_region( dibdrv_physdev *pdev, HRGN region )
{
    if (pdev->clip) CombineRgn( region, region, pdev->clip, RGN_AND );
    return brush_rect( pdev, &pdev->brush, NULL, region, GetROP2( pdev->dev.hdc ));
}

/* paint a region with the pen (note: the region can be modified) */
BOOL pen_region( dibdrv_physdev *pdev, HRGN region )
{
    if (pdev->clip) CombineRgn( region, region, pdev->clip, RGN_AND );
    return brush_rect( pdev, &pdev->pen_brush, NULL, region, GetROP2( pdev->dev.hdc ));
}
