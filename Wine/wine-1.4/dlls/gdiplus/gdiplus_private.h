/*
 * Copyright (C) 2007 Google (Evan Stade)
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

#ifndef __WINE_GP_PRIVATE_H_
#define __WINE_GP_PRIVATE_H_

#include <math.h>
#include <stdarg.h>

#include "windef.h"
#include "wingdi.h"
#include "winbase.h"
#include "winuser.h"

#include "objbase.h"
#include "ocidl.h"
#include "wine/list.h"

#include "gdiplus.h"

#define GP_DEFAULT_PENSTYLE (PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_MITER)
#define MAX_ARC_PTS (13)
#define MAX_DASHLEN (16) /* this is a limitation of gdi */
#define INCH_HIMETRIC (2540)

#define VERSION_MAGIC 0xdbc01001
#define TENSION_CONST (0.3)

COLORREF ARGB2COLORREF(ARGB color) DECLSPEC_HIDDEN;
HBITMAP ARGB2BMP(ARGB color) DECLSPEC_HIDDEN;
extern INT arc2polybezier(GpPointF * points, REAL x1, REAL y1, REAL x2, REAL y2,
    REAL startAngle, REAL sweepAngle) DECLSPEC_HIDDEN;
extern REAL gdiplus_atan2(REAL dy, REAL dx) DECLSPEC_HIDDEN;
extern GpStatus hresult_to_status(HRESULT res) DECLSPEC_HIDDEN;
extern REAL convert_unit(REAL logpixels, GpUnit unit) DECLSPEC_HIDDEN;

extern GpStatus graphics_from_image(GpImage *image, GpGraphics **graphics) DECLSPEC_HIDDEN;

extern GpStatus METAFILE_GetGraphicsContext(GpMetafile* metafile, GpGraphics **result) DECLSPEC_HIDDEN;
extern GpStatus METAFILE_GetDC(GpMetafile* metafile, HDC *hdc) DECLSPEC_HIDDEN;
extern GpStatus METAFILE_ReleaseDC(GpMetafile* metafile, HDC hdc) DECLSPEC_HIDDEN;
extern GpStatus METAFILE_GraphicsDeleted(GpMetafile* metafile) DECLSPEC_HIDDEN;

extern void calc_curve_bezier(CONST GpPointF *pts, REAL tension, REAL *x1,
    REAL *y1, REAL *x2, REAL *y2) DECLSPEC_HIDDEN;
extern void calc_curve_bezier_endp(REAL xend, REAL yend, REAL xadj, REAL yadj,
    REAL tension, REAL *x, REAL *y) DECLSPEC_HIDDEN;

extern void free_installed_fonts(void) DECLSPEC_HIDDEN;

extern void get_font_hfont(GpGraphics *graphics, GDIPCONST GpFont *font, HFONT *hfont) DECLSPEC_HIDDEN;

extern BOOL lengthen_path(GpPath *path, INT len) DECLSPEC_HIDDEN;

extern GpStatus trace_path(GpGraphics *graphics, GpPath *path) DECLSPEC_HIDDEN;

typedef struct region_element region_element;
extern void delete_element(region_element *element) DECLSPEC_HIDDEN;

extern GpStatus get_hatch_data(HatchStyle hatchstyle, const char **result) DECLSPEC_HIDDEN;

static inline INT roundr(REAL x)
{
    return (INT) floorf(x + 0.5);
}

static inline INT ceilr(REAL x)
{
    return (INT) ceilf(x);
}

static inline REAL deg2rad(REAL degrees)
{
    return M_PI * degrees / 180.0;
}

static inline ARGB color_over(ARGB bg, ARGB fg)
{
    BYTE b, g, r, a;
    BYTE bg_alpha, fg_alpha;

    fg_alpha = (fg>>24)&0xff;

    if (fg_alpha == 0xff) return fg;

    if (fg_alpha == 0) return bg;

    bg_alpha = (((bg>>24)&0xff) * (0xff-fg_alpha)) / 0xff;

    if (bg_alpha == 0) return fg;

    a = bg_alpha + fg_alpha;
    b = ((bg&0xff)*bg_alpha + (fg&0xff)*fg_alpha)/a;
    g = (((bg>>8)&0xff)*bg_alpha + ((fg>>8)&0xff)*fg_alpha)/a;
    r = (((bg>>16)&0xff)*bg_alpha + ((fg>>16)&0xff)*fg_alpha)/a;

    return (a<<24)|(r<<16)|(g<<8)|b;
}

extern const char *debugstr_rectf(CONST RectF* rc) DECLSPEC_HIDDEN;

extern const char *debugstr_pointf(CONST PointF* pt) DECLSPEC_HIDDEN;

extern void convert_32bppARGB_to_32bppPARGB(UINT width, UINT height,
    BYTE *dst_bits, INT dst_stride, const BYTE *src_bits, INT src_stride) DECLSPEC_HIDDEN;

extern GpStatus convert_pixels(INT width, INT height,
    INT dst_stride, BYTE *dst_bits, PixelFormat dst_format,
    INT src_stride, const BYTE *src_bits, PixelFormat src_format, ARGB *src_palette) DECLSPEC_HIDDEN;

struct GpPen{
    UINT style;
    GpUnit unit;
    REAL width;
    GpLineCap endcap;
    GpLineCap startcap;
    GpDashCap dashcap;
    GpCustomLineCap *customstart;
    GpCustomLineCap *customend;
    GpLineJoin join;
    REAL miterlimit;
    GpDashStyle dash;
    REAL *dashes;
    INT numdashes;
    REAL offset;    /* dash offset */
    GpBrush *brush;
    GpPenAlignment align;
};

struct GpGraphics{
    HDC hdc;
    HWND hwnd;
    BOOL owndc;
    GpImage *image;
    SmoothingMode smoothing;
    CompositingQuality compqual;
    InterpolationMode interpolation;
    PixelOffsetMode pixeloffset;
    CompositingMode compmode;
    TextRenderingHint texthint;
    GpUnit unit;    /* page unit */
    REAL scale;     /* page scale */
    GpMatrix * worldtrans; /* world transform */
    BOOL busy;      /* hdc handle obtained by GdipGetDC */
    GpRegion *clip;
    UINT textcontrast; /* not used yet. get/set only */
    struct list containers;
    GraphicsContainer contid; /* last-issued container ID */
    /* For giving the caller an HDC when we technically can't: */
    HBITMAP temp_hbitmap;
    int temp_hbitmap_width;
    int temp_hbitmap_height;
    BYTE *temp_bits;
    HDC temp_hdc;
};

struct GpBrush{
    HBRUSH gdibrush;
    GpBrushType bt;
    LOGBRUSH lb;
};

struct GpHatch{
    GpBrush brush;
    HatchStyle hatchstyle;
    ARGB forecol;
    ARGB backcol;
};

struct GpSolidFill{
    GpBrush brush;
    ARGB color;
    HBITMAP bmp;
};

struct GpPathGradient{
    GpBrush brush;
    PathData pathdata;
    ARGB centercolor;
    GpWrapMode wrap;
    BOOL gamma;
    GpPointF center;
    GpPointF focus;
    REAL* blendfac;  /* blend factors */
    REAL* blendpos;  /* blend positions */
    INT blendcount;
};

struct GpLineGradient{
    GpBrush brush;
    GpPointF startpoint;
    GpPointF endpoint;
    ARGB startcolor;
    ARGB endcolor;
    RectF rect;
    GpWrapMode wrap;
    BOOL gamma;
    REAL* blendfac;  /* blend factors */
    REAL* blendpos;  /* blend positions */
    INT blendcount;
    ARGB* pblendcolor; /* preset blend colors */
    REAL* pblendpos; /* preset blend positions */
    INT pblendcount;
};

struct GpTexture{
    GpBrush brush;
    GpMatrix *transform;
    GpImage *image;
    GpImageAttributes *imageattributes;
    BYTE *bitmap_bits; /* image bits converted to ARGB and run through imageattributes */
};

struct GpPath{
    GpFillMode fill;
    GpPathData pathdata;
    BOOL newfigure; /* whether the next drawing action starts a new figure */
    INT datalen; /* size of the arrays in pathdata */
};

struct GpMatrix{
    REAL matrix[6];
};

struct GpPathIterator{
    GpPathData pathdata;
    INT subpath_pos;    /* for NextSubpath methods */
    INT marker_pos;     /* for NextMarker methods */
    INT pathtype_pos;   /* for NextPathType methods */
};

struct GpCustomLineCap{
    GpPathData pathdata;
    BOOL fill;      /* TRUE for fill, FALSE for stroke */
    GpLineCap cap;  /* as far as I can tell, this value is ignored */
    REAL inset;     /* how much to adjust the end of the line */
    GpLineJoin join;
    REAL scale;
};

struct GpAdustableArrowCap{
    GpCustomLineCap cap;
};

struct GpImage{
    IPicture* picture;
    ImageType type;
    GUID format;
    UINT flags;
    UINT palette_flags;
    UINT palette_count;
    UINT palette_size;
    ARGB *palette_entries;
    REAL xres, yres;
};

struct GpMetafile{
    GpImage image;
    GpRectF bounds;
    GpUnit unit;
    MetafileType metafile_type;
    HENHMETAFILE hemf;

    /* recording */
    HDC record_dc;
    GpGraphics *record_graphics;
    BYTE *comment_data;
    DWORD comment_data_size;
    DWORD comment_data_length;

    /* playback */
    GpGraphics *playback_graphics;
    HDC playback_dc;
    GpPointF playback_points[3];
    HANDLETABLE *handle_table;
    int handle_count;
};

struct GpBitmap{
    GpImage image;
    INT width;
    INT height;
    PixelFormat format;
    ImageLockMode lockmode;
    INT numlocks;
    BYTE *bitmapbits;   /* pointer to the buffer we passed in BitmapLockBits */
    HBITMAP hbitmap;
    HDC hdc;
    BYTE *bits; /* actual image bits if this is a DIB */
    INT stride; /* stride of bits if this is a DIB */
    BYTE *own_bits; /* image bits that need to be freed with this object */
    INT lockx, locky; /* X and Y coordinates of the rect when a bitmap is locked for writing. */
};

struct GpCachedBitmap{
    GpImage *image;
};

struct color_key{
    BOOL enabled;
    ARGB low;
    ARGB high;
};

struct color_matrix{
    BOOL enabled;
    ColorMatrixFlags flags;
    ColorMatrix colormatrix;
    ColorMatrix graymatrix;
};

struct color_remap_table{
    BOOL enabled;
    INT mapsize;
    GDIPCONST ColorMap *colormap;
};

struct GpImageAttributes{
    WrapMode wrap;
    ARGB outside_color;
    BOOL clamp;
    struct color_key colorkeys[ColorAdjustTypeCount];
    struct color_matrix colormatrices[ColorAdjustTypeCount];
    struct color_remap_table colorremaptables[ColorAdjustTypeCount];
    BOOL gamma_enabled[ColorAdjustTypeCount];
    REAL gamma[ColorAdjustTypeCount];
};

struct GpFont{
    LOGFONTW lfw;
    REAL emSize;
    REAL pixel_size;
    UINT height;
    LONG line_spacing;
    Unit unit;
};

struct GpStringFormat{
    INT attr;
    LANGID lang;
    LANGID digitlang;
    StringAlignment align;
    StringTrimming trimming;
    HotkeyPrefix hkprefix;
    StringAlignment vertalign;
    StringDigitSubstitute digitsub;
    INT tabcount;
    REAL firsttab;
    REAL *tabs;
    CharacterRange *character_ranges;
    INT range_count;
};

struct GpFontCollection{
    GpFontFamily **FontFamilies;
    INT count;
    INT allocated;
};

struct GpFontFamily{
    NEWTEXTMETRICW tmw;
    WCHAR FamilyName[LF_FACESIZE];
};

/* internal use */
typedef enum RegionType
{
    RegionDataRect          = 0x10000000,
    RegionDataPath          = 0x10000001,
    RegionDataEmptyRect     = 0x10000002,
    RegionDataInfiniteRect  = 0x10000003,
} RegionType;

struct region_element
{
    DWORD type; /* Rectangle, Path, SpecialRectangle, or CombineMode */
    union
    {
        GpRectF rect;
        struct
        {
            GpPath* path;
            struct
            {
                DWORD size;
                DWORD magic;
                DWORD count;
                DWORD flags;
            } pathheader;
        } pathdata;
        struct
        {
            struct region_element *left;  /* the original region */
            struct region_element *right; /* what *left was combined with */
        } combine;
    } elementdata;
};

struct GpRegion{
    struct
    {
        DWORD size;
        DWORD checksum;
        DWORD magic;
        DWORD num_children;
    } header;
    region_element node;
};

typedef GpStatus (*gdip_format_string_callback)(HDC hdc,
    GDIPCONST WCHAR *string, INT index, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    INT lineno, const RectF *bounds, void *user_data);

GpStatus gdip_format_string(HDC hdc,
    GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font,
    GDIPCONST RectF *rect, GDIPCONST GpStringFormat *format,
    gdip_format_string_callback callback, void *user_data) DECLSPEC_HIDDEN;

#endif
