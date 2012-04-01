/*
 * X11 graphics driver initialisation functions
 *
 * Copyright 1996 Alexandre Julliard
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
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "x11drv.h"
#include "x11font.h"
#include "ddrawi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

Display *gdi_display;  /* display to use for all GDI functions */

/* a few dynamic device caps */
static int log_pixels_x;  /* pixels per logical inch in x direction */
static int log_pixels_y;  /* pixels per logical inch in y direction */
static int horz_size;     /* horz. size of screen in millimeters */
static int vert_size;     /* vert. size of screen in millimeters */
static int palette_size;
static int device_init_done;
unsigned int text_caps = (TC_OP_CHARACTER | TC_OP_STROKE | TC_CP_STROKE |
                          TC_CR_ANY | TC_SA_DOUBLE | TC_SA_INTEGER |
                          TC_SA_CONTIN | TC_UA_ABLE | TC_SO_ABLE | TC_RA_ABLE);
                          /* X11R6 adds TC_SF_X_YINDEP, Xrender adds TC_VA_ABLE */


static const WCHAR dpi_key_name[] = {'S','o','f','t','w','a','r','e','\\','F','o','n','t','s','\0'};
static const WCHAR dpi_value_name[] = {'L','o','g','P','i','x','e','l','s','\0'};

static const struct gdi_dc_funcs x11drv_funcs;
static const struct gdi_dc_funcs *xrender_funcs;

/******************************************************************************
 *      get_dpi
 *
 * get the dpi from the registry
 */
static DWORD get_dpi( void )
{
    DWORD dpi = 96;
    HKEY hkey;

    if (RegOpenKeyW(HKEY_CURRENT_CONFIG, dpi_key_name, &hkey) == ERROR_SUCCESS)
    {
        DWORD type, size, new_dpi;

        size = sizeof(new_dpi);
        if(RegQueryValueExW(hkey, dpi_value_name, NULL, &type, (void *)&new_dpi, &size) == ERROR_SUCCESS)
        {
            if(type == REG_DWORD && new_dpi != 0)
                dpi = new_dpi;
        }
        RegCloseKey(hkey);
    }
    return dpi;
}

/**********************************************************************
 *	     device_init
 *
 * Perform initializations needed upon creation of the first device.
 */
static void device_init(void)
{
    device_init_done = TRUE;

    /* Initialize XRender */
    xrender_funcs = X11DRV_XRender_Init();

    /* Init Xcursor */
    X11DRV_Xcursor_Init();

    palette_size = X11DRV_PALETTE_Init();

    X11DRV_BITMAP_Init();

    /* Initialize device caps */
    log_pixels_x = log_pixels_y = get_dpi();
    horz_size = MulDiv( screen_width, 254, log_pixels_x * 10 );
    vert_size = MulDiv( screen_height, 254, log_pixels_y * 10 );

    /* Initialize fonts and text caps */
    X11DRV_FONT_Init(log_pixels_x, log_pixels_y);
}

/**********************************************************************
 *	     X11DRV_GDI_Finalize
 */
void X11DRV_GDI_Finalize(void)
{
    X11DRV_PALETTE_Cleanup();
    /* don't bother to close the display, it often triggers X bugs */
    /* XCloseDisplay( gdi_display ); */
}


static X11DRV_PDEVICE *create_x11_physdev( Drawable drawable )
{
    X11DRV_PDEVICE *physDev;

    if (!device_init_done) device_init();

    if (!(physDev = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*physDev) ))) return NULL;

    wine_tsx11_lock();
    physDev->drawable = drawable;
    physDev->gc = XCreateGC( gdi_display, drawable, 0, NULL );
    XSetGraphicsExposures( gdi_display, physDev->gc, False );
    XSetSubwindowMode( gdi_display, physDev->gc, IncludeInferiors );
    XFlush( gdi_display );
    wine_tsx11_unlock();
    return physDev;
}

/**********************************************************************
 *	     X11DRV_CreateDC
 */
static BOOL X11DRV_CreateDC( PHYSDEV *pdev, LPCWSTR driver, LPCWSTR device,
                             LPCWSTR output, const DEVMODEW* initData )
{
    X11DRV_PDEVICE *physDev = create_x11_physdev( root_window );

    if (!physDev) return FALSE;

    physDev->depth         = screen_depth;
    physDev->color_shifts  = &X11DRV_PALETTE_default_shifts;
    physDev->drawable_rect = virtual_screen_rect;
    SetRect( &physDev->dc_rect, 0, 0, virtual_screen_rect.right - virtual_screen_rect.left,
             virtual_screen_rect.bottom - virtual_screen_rect.top );
    push_dc_driver( pdev, &physDev->dev, &x11drv_funcs );
    if (!xrender_funcs) return TRUE;
    return xrender_funcs->pCreateDC( pdev, driver, device, output, initData );
}


/**********************************************************************
 *	     X11DRV_CreateCompatibleDC
 */
static BOOL X11DRV_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *pdev )
{
    X11DRV_PDEVICE *physDev = create_x11_physdev( BITMAP_stock_phys_bitmap.pixmap );

    if (!physDev) return FALSE;

    if (!BITMAP_stock_phys_bitmap.hbitmap)
        BITMAP_stock_phys_bitmap.hbitmap = GetCurrentObject( (*pdev)->hdc, OBJ_BITMAP );

    physDev->bitmap = &BITMAP_stock_phys_bitmap;
    physDev->depth  = 1;
    SetRect( &physDev->drawable_rect, 0, 0, 1, 1 );
    physDev->dc_rect = physDev->drawable_rect;
    push_dc_driver( pdev, &physDev->dev, &x11drv_funcs );
    if (orig) return TRUE;  /* we already went through Xrender if we have an orig device */
    if (!xrender_funcs) return TRUE;
    return xrender_funcs->pCreateCompatibleDC( NULL, pdev );
}


/**********************************************************************
 *	     X11DRV_DeleteDC
 */
static BOOL X11DRV_DeleteDC( PHYSDEV dev )
{
    X11DRV_PDEVICE *physDev = get_x11drv_dev( dev );

    wine_tsx11_lock();
    XFreeGC( gdi_display, physDev->gc );
    wine_tsx11_unlock();
    HeapFree( GetProcessHeap(), 0, physDev );
    return TRUE;
}


/***********************************************************************
 *           GetDeviceCaps    (X11DRV.@)
 */
static INT X11DRV_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    switch(cap)
    {
    case DRIVERVERSION:
        return 0x300;
    case TECHNOLOGY:
        return DT_RASDISPLAY;
    case HORZSIZE:
        return horz_size;
    case VERTSIZE:
        return vert_size;
    case HORZRES:
        return screen_width;
    case VERTRES:
        return screen_height;
    case DESKTOPHORZRES:
        return virtual_screen_rect.right - virtual_screen_rect.left;
    case DESKTOPVERTRES:
        return virtual_screen_rect.bottom - virtual_screen_rect.top;
    case BITSPIXEL:
        return screen_bpp;
    case PLANES:
        return 1;
    case NUMBRUSHES:
        return -1;
    case NUMPENS:
        return -1;
    case NUMMARKERS:
        return 0;
    case NUMFONTS:
        return 0;
    case NUMCOLORS:
        /* MSDN: Number of entries in the device's color table, if the device has
         * a color depth of no more than 8 bits per pixel.For devices with greater
         * color depths, -1 is returned. */
        return (screen_depth > 8) ? -1 : (1 << screen_depth);
    case PDEVICESIZE:
        return sizeof(X11DRV_PDEVICE);
    case CURVECAPS:
        return (CC_CIRCLES | CC_PIE | CC_CHORD | CC_ELLIPSES | CC_WIDE |
                CC_STYLED | CC_WIDESTYLED | CC_INTERIORS | CC_ROUNDRECT);
    case LINECAPS:
        return (LC_POLYLINE | LC_MARKER | LC_POLYMARKER | LC_WIDE |
                LC_STYLED | LC_WIDESTYLED | LC_INTERIORS);
    case POLYGONALCAPS:
        return (PC_POLYGON | PC_RECTANGLE | PC_WINDPOLYGON | PC_SCANLINE |
                PC_WIDE | PC_STYLED | PC_WIDESTYLED | PC_INTERIORS);
    case TEXTCAPS:
        return text_caps;
    case CLIPCAPS:
        return CP_REGION;
    case COLORRES:
        /* The observed correspondence between BITSPIXEL and COLORRES is:
         * BITSPIXEL: 8  -> COLORRES: 18
         * BITSPIXEL: 16 -> COLORRES: 16
         * BITSPIXEL: 24 -> COLORRES: 24
         * BITSPIXEL: 32 -> COLORRES: 24 */
        return (screen_bpp <= 8) ? 18 : min( 24, screen_bpp );
    case RASTERCAPS:
        return (RC_BITBLT | RC_BANDING | RC_SCALING | RC_BITMAP64 | RC_DI_BITMAP |
                RC_DIBTODEV | RC_BIGFONT | RC_STRETCHBLT | RC_STRETCHDIB | RC_DEVBITS |
                (palette_size ? RC_PALETTE : 0));
    case SHADEBLENDCAPS:
        return (SB_GRAD_RECT | SB_GRAD_TRI | SB_CONST_ALPHA | SB_PIXEL_ALPHA);
    case ASPECTX:
    case ASPECTY:
        return 36;
    case ASPECTXY:
        return 51;
    case LOGPIXELSX:
        return log_pixels_x;
    case LOGPIXELSY:
        return log_pixels_y;
    case CAPS1:
        FIXME("(%p): CAPS1 is unimplemented, will return 0\n", dev->hdc );
        /* please see wingdi.h for the possible bit-flag values that need
           to be returned. */
        return 0;
    case SIZEPALETTE:
        return palette_size;
    case NUMRESERVED:
    case PHYSICALWIDTH:
    case PHYSICALHEIGHT:
    case PHYSICALOFFSETX:
    case PHYSICALOFFSETY:
    case SCALINGFACTORX:
    case SCALINGFACTORY:
    case VREFRESH:
    case BLTALIGNMENT:
        return 0;
    default:
        FIXME("(%p): unsupported capability %d, will return 0\n", dev->hdc, cap );
        return 0;
    }
}


/**********************************************************************
 *           ExtEscape  (X11DRV.@)
 */
static INT X11DRV_ExtEscape( PHYSDEV dev, INT escape, INT in_count, LPCVOID in_data,
                      INT out_count, LPVOID out_data )
{
    X11DRV_PDEVICE *physDev = get_x11drv_dev( dev );

    switch(escape)
    {
    case QUERYESCSUPPORT:
        if (in_data)
        {
            switch (*(const INT *)in_data)
            {
            case DCICOMMAND:
                return DD_HAL_VERSION;
            case X11DRV_ESCAPE:
                return TRUE;
            }
        }
        break;

    case X11DRV_ESCAPE:
        if (in_data && in_count >= sizeof(enum x11drv_escape_codes))
        {
            switch(*(const enum x11drv_escape_codes *)in_data)
            {
            case X11DRV_GET_DISPLAY:
                if (out_count >= sizeof(Display *))
                {
                    *(Display **)out_data = gdi_display;
                    return TRUE;
                }
                break;
            case X11DRV_GET_DRAWABLE:
                if (out_count >= sizeof(Drawable))
                {
                    *(Drawable *)out_data = physDev->drawable;
                    return TRUE;
                }
                break;
            case X11DRV_GET_FONT:
                if (out_count >= sizeof(Font))
                {
                    fontObject* pfo = XFONT_GetFontObject( physDev->font );
		    if (pfo == NULL) return FALSE;
                    *(Font *)out_data = pfo->fs->fid;
                    return TRUE;
                }
                break;
            case X11DRV_SET_DRAWABLE:
                if (in_count >= sizeof(struct x11drv_escape_set_drawable))
                {
                    const struct x11drv_escape_set_drawable *data = in_data;
                    physDev->dc_rect = data->dc_rect;
                    physDev->drawable = data->drawable;
                    physDev->drawable_rect = data->drawable_rect;
                    physDev->current_pf = pixelformat_from_fbconfig_id( data->fbconfig_id );
                    physDev->gl_drawable = data->gl_drawable;
                    physDev->pixmap = data->pixmap;
                    physDev->gl_copy = data->gl_copy;
                    wine_tsx11_lock();
                    XSetSubwindowMode( gdi_display, physDev->gc, data->mode );
                    wine_tsx11_unlock();
                    TRACE( "SET_DRAWABLE hdc %p drawable %lx gl_drawable %lx pf %u dc_rect %s drawable_rect %s\n",
                           dev->hdc, physDev->drawable, physDev->gl_drawable, physDev->current_pf,
                           wine_dbgstr_rect(&physDev->dc_rect), wine_dbgstr_rect(&physDev->drawable_rect) );
                    return TRUE;
                }
                break;
            case X11DRV_START_EXPOSURES:
                wine_tsx11_lock();
                XSetGraphicsExposures( gdi_display, physDev->gc, True );
                wine_tsx11_unlock();
                physDev->exposures = 0;
                return TRUE;
            case X11DRV_END_EXPOSURES:
                if (out_count >= sizeof(HRGN))
                {
                    HRGN hrgn = 0, tmp = 0;

                    wine_tsx11_lock();
                    XSetGraphicsExposures( gdi_display, physDev->gc, False );
                    wine_tsx11_unlock();
                    if (physDev->exposures)
                    {
                        for (;;)
                        {
                            XEvent event;

                            wine_tsx11_lock();
                            XWindowEvent( gdi_display, physDev->drawable, ~0, &event );
                            wine_tsx11_unlock();
                            if (event.type == NoExpose) break;
                            if (event.type == GraphicsExpose)
                            {
                                RECT rect;

                                rect.left   = event.xgraphicsexpose.x - physDev->dc_rect.left;
                                rect.top    = event.xgraphicsexpose.y - physDev->dc_rect.top;
                                rect.right  = rect.left + event.xgraphicsexpose.width;
                                rect.bottom = rect.top + event.xgraphicsexpose.height;
                                if (GetLayout( dev->hdc ) & LAYOUT_RTL)
                                    mirror_rect( &physDev->dc_rect, &rect );

                                TRACE( "got %s count %d\n", wine_dbgstr_rect(&rect),
                                       event.xgraphicsexpose.count );

                                if (!tmp) tmp = CreateRectRgnIndirect( &rect );
                                else SetRectRgn( tmp, rect.left, rect.top, rect.right, rect.bottom );
                                if (hrgn) CombineRgn( hrgn, hrgn, tmp, RGN_OR );
                                else
                                {
                                    hrgn = tmp;
                                    tmp = 0;
                                }
                                if (!event.xgraphicsexpose.count) break;
                            }
                            else
                            {
                                ERR( "got unexpected event %d\n", event.type );
                                break;
                            }
                        }
                        if (tmp) DeleteObject( tmp );
                    }
                    *(HRGN *)out_data = hrgn;
                    return TRUE;
                }
                break;
            case X11DRV_GET_DCE:
            case X11DRV_SET_DCE:
            case X11DRV_SYNC_PIXMAP:
                FIXME( "%x escape no longer supported\n", *(const enum x11drv_escape_codes *)in_data );
                break;
            case X11DRV_GET_GLX_DRAWABLE:
                if (out_count >= sizeof(Drawable))
                {
                    *(Drawable *)out_data = get_glxdrawable(physDev);
                    return TRUE;
                }
                break;
            case X11DRV_FLUSH_GL_DRAWABLE:
                flush_gl_drawable(physDev);
                return TRUE;
            }
        }
        break;
    }
    return 0;
}


static const struct gdi_dc_funcs x11drv_funcs =
{
    NULL,                               /* pAbortDoc */
    NULL,                               /* pAbortPath */
    NULL,                               /* pAlphaBlend */
    NULL,                               /* pAngleArc */
    X11DRV_Arc,                         /* pArc */
    NULL,                               /* pArcTo */
    NULL,                               /* pBeginPath */
    NULL,                               /* pBlendImage */
    X11DRV_ChoosePixelFormat,           /* pChoosePixelFormat */
    X11DRV_Chord,                       /* pChord */
    NULL,                               /* pCloseFigure */
    X11DRV_CopyBitmap,                  /* pCopyBitmap */
    X11DRV_CreateBitmap,                /* pCreateBitmap */
    X11DRV_CreateCompatibleDC,          /* pCreateCompatibleDC */
    X11DRV_CreateDC,                    /* pCreateDC */
    X11DRV_DeleteBitmap,                /* pDeleteBitmap */
    X11DRV_DeleteDC,                    /* pDeleteDC */
    NULL,                               /* pDeleteObject */
    X11DRV_DescribePixelFormat,         /* pDescribePixelFormat */
    NULL,                               /* pDeviceCapabilities */
    X11DRV_Ellipse,                     /* pEllipse */
    NULL,                               /* pEndDoc */
    NULL,                               /* pEndPage */
    NULL,                               /* pEndPath */
    X11DRV_EnumFonts,                   /* pEnumFonts */
    X11DRV_EnumICMProfiles,             /* pEnumICMProfiles */
    NULL,                               /* pExcludeClipRect */
    NULL,                               /* pExtDeviceMode */
    X11DRV_ExtEscape,                   /* pExtEscape */
    X11DRV_ExtFloodFill,                /* pExtFloodFill */
    NULL,                               /* pExtSelectClipRgn */
    X11DRV_ExtTextOut,                  /* pExtTextOut */
    NULL,                               /* pFillPath */
    NULL,                               /* pFillRgn */
    NULL,                               /* pFlattenPath */
    NULL,                               /* pFontIsLinked */
    NULL,                               /* pFrameRgn */
    NULL,                               /* pGdiComment */
    NULL,                               /* pGdiRealizationInfo */
    NULL,                               /* pGetCharABCWidths */
    NULL,                               /* pGetCharABCWidthsI */
    X11DRV_GetCharWidth,                /* pGetCharWidth */
    X11DRV_GetDeviceCaps,               /* pGetDeviceCaps */
    X11DRV_GetDeviceGammaRamp,          /* pGetDeviceGammaRamp */
    NULL,                               /* pGetFontData */
    NULL,                               /* pGetFontUnicodeRanges */
    NULL,                               /* pGetGlyphIndices */
    NULL,                               /* pGetGlyphOutline */
    X11DRV_GetICMProfile,               /* pGetICMProfile */
    X11DRV_GetImage,                    /* pGetImage */
    NULL,                               /* pGetKerningPairs */
    X11DRV_GetNearestColor,             /* pGetNearestColor */
    NULL,                               /* pGetOutlineTextMetrics */
    NULL,                               /* pGetPixel */
    X11DRV_GetPixelFormat,              /* pGetPixelFormat */
    X11DRV_GetSystemPaletteEntries,     /* pGetSystemPaletteEntries */
    NULL,                               /* pGetTextCharsetInfo */
    X11DRV_GetTextExtentExPoint,        /* pGetTextExtentExPoint */
    NULL,                               /* pGetTextExtentExPointI */
    NULL,                               /* pGetTextFace */
    X11DRV_GetTextMetrics,              /* pGetTextMetrics */
    X11DRV_GradientFill,                /* pGradientFill */
    NULL,                               /* pIntersectClipRect */
    NULL,                               /* pInvertRgn */
    X11DRV_LineTo,                      /* pLineTo */
    NULL,                               /* pModifyWorldTransform */
    NULL,                               /* pMoveTo */
    NULL,                               /* pOffsetClipRgn */
    NULL,                               /* pOffsetViewportOrg */
    NULL,                               /* pOffsetWindowOrg */
    X11DRV_PaintRgn,                    /* pPaintRgn */
    X11DRV_PatBlt,                      /* pPatBlt */
    X11DRV_Pie,                         /* pPie */
    NULL,                               /* pPolyBezier */
    NULL,                               /* pPolyBezierTo */
    NULL,                               /* pPolyDraw */
    X11DRV_PolyPolygon,                 /* pPolyPolygon */
    X11DRV_PolyPolyline,                /* pPolyPolyline */
    X11DRV_Polygon,                     /* pPolygon */
    X11DRV_Polyline,                    /* pPolyline */
    NULL,                               /* pPolylineTo */
    X11DRV_PutImage,                    /* pPutImage */
    X11DRV_RealizeDefaultPalette,       /* pRealizeDefaultPalette */
    X11DRV_RealizePalette,              /* pRealizePalette */
    X11DRV_Rectangle,                   /* pRectangle */
    NULL,                               /* pResetDC */
    NULL,                               /* pRestoreDC */
    X11DRV_RoundRect,                   /* pRoundRect */
    NULL,                               /* pSaveDC */
    NULL,                               /* pScaleViewportExt */
    NULL,                               /* pScaleWindowExt */
    X11DRV_SelectBitmap,                /* pSelectBitmap */
    X11DRV_SelectBrush,                 /* pSelectBrush */
    NULL,                               /* pSelectClipPath */
    X11DRV_SelectFont,                  /* pSelectFont */
    NULL,                               /* pSelectPalette */
    X11DRV_SelectPen,                   /* pSelectPen */
    NULL,                               /* pSetArcDirection */
    NULL,                               /* pSetBkColor */
    NULL,                               /* pSetBkMode */
    X11DRV_SetDCBrushColor,             /* pSetDCBrushColor */
    X11DRV_SetDCPenColor,               /* pSetDCPenColor */
    NULL,                               /* pSetDIBitsToDevice */
    X11DRV_SetDeviceClipping,           /* pSetDeviceClipping */
    X11DRV_SetDeviceGammaRamp,          /* pSetDeviceGammaRamp */
    NULL,                               /* pSetLayout */
    NULL,                               /* pSetMapMode */
    NULL,                               /* pSetMapperFlags */
    X11DRV_SetPixel,                    /* pSetPixel */
    X11DRV_SetPixelFormat,              /* pSetPixelFormat */
    NULL,                               /* pSetPolyFillMode */
    NULL,                               /* pSetROP2 */
    NULL,                               /* pSetRelAbs */
    NULL,                               /* pSetStretchBltMode */
    NULL,                               /* pSetTextAlign */
    NULL,                               /* pSetTextCharacterExtra */
    NULL,                               /* pSetTextColor */
    NULL,                               /* pSetTextJustification */
    NULL,                               /* pSetViewportExt */
    NULL,                               /* pSetViewportOrg */
    NULL,                               /* pSetWindowExt */
    NULL,                               /* pSetWindowOrg */
    NULL,                               /* pSetWorldTransform */
    NULL,                               /* pStartDoc */
    NULL,                               /* pStartPage */
    X11DRV_StretchBlt,                  /* pStretchBlt */
    NULL,                               /* pStretchDIBits */
    NULL,                               /* pStrokeAndFillPath */
    NULL,                               /* pStrokePath */
    X11DRV_SwapBuffers,                 /* pSwapBuffers */
    X11DRV_UnrealizePalette,            /* pUnrealizePalette */
    NULL,                               /* pWidenPath */
    X11DRV_wglCopyContext,              /* pwglCopyContext */
    X11DRV_wglCreateContext,            /* pwglCreateContext */
    X11DRV_wglCreateContextAttribsARB,  /* pwglCreateContextAttribsARB */
    X11DRV_wglDeleteContext,            /* pwglDeleteContext */
    X11DRV_wglGetPbufferDCARB,          /* pwglGetPbufferDCARB */
    X11DRV_wglGetProcAddress,           /* pwglGetProcAddress */
    X11DRV_wglMakeContextCurrentARB,    /* pwglMakeContextCurrentARB */
    X11DRV_wglMakeCurrent,              /* pwglMakeCurrent */
    X11DRV_wglSetPixelFormatWINE,       /* pwglSetPixelFormatWINE */
    X11DRV_wglShareLists,               /* pwglShareLists */
    X11DRV_wglUseFontBitmapsA,          /* pwglUseFontBitmapsA */
    X11DRV_wglUseFontBitmapsW,          /* pwglUseFontBitmapsW */
};


/******************************************************************************
 *      X11DRV_get_gdi_driver
 */
const struct gdi_dc_funcs * CDECL X11DRV_get_gdi_driver( unsigned int version )
{
    if (version != WINE_GDI_DRIVER_VERSION)
    {
        ERR( "version mismatch, gdi32 wants %u but winex11 has %u\n", version, WINE_GDI_DRIVER_VERSION );
        return NULL;
    }
    return &x11drv_funcs;
}
