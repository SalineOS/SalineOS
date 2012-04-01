/*
 * X11 graphics driver text functions
 *
 * Copyright 1993,1994 Alexandre Julliard
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

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

#include "windef.h"
#include "winbase.h"
#include "x11font.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(text);

#define IROUND(x) (int)((x)>0? (x)+0.5 : (x) - 0.5)


/***********************************************************************
 *           X11DRV_ExtTextOut
 */
BOOL X11DRV_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags,
                        const RECT *lprect, LPCWSTR wstr, UINT count, const INT *lpDx )
{
    X11DRV_PDEVICE *physDev = get_x11drv_dev( dev );
    BOOL restore_region = FALSE;
    unsigned int i;
    int pixel;
    fontObject*		pfo = XFONT_GetFontObject( physDev->font );
    XFontStruct*	font;
    BOOL		rotated = FALSE;
    XChar2b		*str2b = NULL;
    BOOL                result = TRUE;

    if (!pfo)
    {
        dev = GET_NEXT_PHYSDEV( dev, pExtTextOut );
        return dev->funcs->pExtTextOut( dev, x, y, flags, lprect, wstr, count, lpDx );
    }

    if (!X11DRV_SetupGCForText( physDev )) return TRUE;

    font = pfo->fs;

    if (pfo->lf.lfEscapement && pfo->lpX11Trans)
        rotated = TRUE;

    TRACE("hdc=%p df=%04x %d,%d rc %s %s, %d  flags=%d lpDx=%p\n",
          dev->hdc, (UINT16)physDev->font, x, y, wine_dbgstr_rect(lprect),
	  debugstr_wn (wstr, count), count, flags, lpDx);

      /* Draw the rectangle */

    if (flags & ETO_OPAQUE)
    {
	pixel = X11DRV_PALETTE_ToPhysical( physDev, GetBkColor(physDev->dev.hdc) );
        wine_tsx11_lock();
        XSetForeground( gdi_display, physDev->gc, pixel );
        XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                        physDev->dc_rect.left + lprect->left, physDev->dc_rect.top + lprect->top,
                        lprect->right - lprect->left, lprect->bottom - lprect->top );
        wine_tsx11_unlock();
    }
    if (!count) goto END;  /* Nothing more to do */


      /* Set the clip region */

    if (flags & ETO_CLIPPED)
    {
        HRGN clip_region = CreateRectRgnIndirect( lprect );
        restore_region = add_extra_clipping_region( physDev, clip_region );
        DeleteObject( clip_region );
    }


    /* Draw the text (count > 0 verified) */
    if (!(str2b = X11DRV_cptable[pfo->fi->cptable].punicode_to_char2b( pfo, wstr, count )))
    {
        result = FALSE;
        goto END;
    }

    pixel = X11DRV_PALETTE_ToPhysical( physDev, GetTextColor(physDev->dev.hdc) );
    wine_tsx11_lock();
    XSetForeground( gdi_display, physDev->gc, pixel );
    wine_tsx11_unlock();
    if(!rotated)
    {
        if (!lpDx)
        {
            X11DRV_cptable[pfo->fi->cptable].pDrawString(
                           pfo, gdi_display, physDev->drawable, physDev->gc,
                           physDev->dc_rect.left + x, physDev->dc_rect.top + y, str2b, count );
        }
        else
        {
            XTextItem16 *items;

            items = HeapAlloc( GetProcessHeap(), 0, count * sizeof(XTextItem16) );
            if(items == NULL)
            {
                result = FALSE;
                goto END;
            }
            items[0].chars = str2b;
            items[0].delta = 0;
            items[0].nchars = 1;
            items[0].font = None;
            for(i = 1; i < count; i++)
            {
                items[i].chars  = str2b + i;
                items[i].delta  = (flags & ETO_PDY) ? lpDx[(i - 1) * 2] : lpDx[i - 1];
                items[i].delta -= X11DRV_cptable[pfo->fi->cptable].pTextWidth( pfo, str2b + i - 1, 1 );
                items[i].nchars = 1;
                items[i].font   = None;
            }
            X11DRV_cptable[pfo->fi->cptable].pDrawText( pfo, gdi_display,
                                  physDev->drawable, physDev->gc,
                                  physDev->dc_rect.left + x, physDev->dc_rect.top + y, items, count );
            HeapFree( GetProcessHeap(), 0, items );
        }
    }
    else /* rotated */
    {
        /* have to render character by character. */
        double offset = 0.0;
        UINT i;

        for (i=0; i<count; i++)
        {
            int char_metric_offset = str2b[i].byte2 + (str2b[i].byte1 << 8)
                - font->min_char_or_byte2;
            int x_i = IROUND((double) (physDev->dc_rect.left + x) + offset *
                             pfo->lpX11Trans->a / pfo->lpX11Trans->pixelsize );
            int y_i = IROUND((double) (physDev->dc_rect.top + y) - offset *
                             pfo->lpX11Trans->b / pfo->lpX11Trans->pixelsize );

            X11DRV_cptable[pfo->fi->cptable].pDrawString(
                                    pfo, gdi_display, physDev->drawable, physDev->gc,
                                    x_i, y_i, &str2b[i], 1);
            if (lpDx)
            {
                offset += (flags & ETO_PDY) ? lpDx[i * 2] : lpDx[i];
            }
            else
            {
                offset += (double) (font->per_char ?
                                    font->per_char[char_metric_offset].attributes:
                                    font->min_bounds.attributes)
                    * pfo->lpX11Trans->pixelsize / 1000.0;
            }
        }
    }

END:
    HeapFree( GetProcessHeap(), 0, str2b );
    if (restore_region) restore_clipping_region( physDev );
    return result;
}


/***********************************************************************
 *           X11DRV_GetTextExtentExPoint
 */
BOOL X11DRV_GetTextExtentExPoint( PHYSDEV dev, LPCWSTR str, INT count,
                                  INT maxExt, LPINT lpnFit, LPINT alpDx, LPSIZE size )
{
    X11DRV_PDEVICE *physDev = get_x11drv_dev( dev );
    fontObject* pfo = XFONT_GetFontObject( physDev->font );

    TRACE("%s %d\n", debugstr_wn(str,count), count);
    if( pfo ) {
	XChar2b *p = X11DRV_cptable[pfo->fi->cptable].punicode_to_char2b( pfo, str, count );
	if (!p) return FALSE;
        if( !pfo->lpX11Trans ) {
	    int dir, ascent, descent;
	    int info_width;
	    X11DRV_cptable[pfo->fi->cptable].pTextExtents( pfo, p,
				count, &dir, &ascent, &descent, &info_width,
				maxExt, lpnFit, alpDx );

          size->cx = info_width;
          size->cy = pfo->fs->ascent + pfo->fs->descent;
	} else {
	    INT i;
	    INT nfit = 0;
	    float x = 0.0, y = 0.0;
	    float scaled_x = 0.0, pixsize = pfo->lpX11Trans->pixelsize;
	    /* FIXME: Deal with *_char_or_byte2 != 0 situations */
	    for(i = 0; i < count; i++) {
	        x += pfo->fs->per_char ?
	   pfo->fs->per_char[p[i].byte2 - pfo->fs->min_char_or_byte2].attributes :
	   pfo->fs->min_bounds.attributes;
	        scaled_x = x * pixsize / 1000.0;
	        if (alpDx)
	            alpDx[i] = scaled_x;
	        if (scaled_x <= maxExt)
	            ++nfit;
	    }
	    y = pfo->lpX11Trans->RAW_ASCENT + pfo->lpX11Trans->RAW_DESCENT;
	    TRACE("x = %f y = %f\n", x, y);
	    size->cx = x * pfo->lpX11Trans->pixelsize / 1000.0;
	    size->cy = y * pfo->lpX11Trans->pixelsize / 1000.0;
	    if (lpnFit)
	        *lpnFit = nfit;
	}
	size->cx *= pfo->rescale;
	size->cy *= pfo->rescale;
	HeapFree( GetProcessHeap(), 0, p );
	return TRUE;
    }

    dev = GET_NEXT_PHYSDEV( dev, pGetTextExtentExPoint );
    return dev->funcs->pGetTextExtentExPoint( dev, str, count, maxExt, lpnFit, alpDx, size );
}
