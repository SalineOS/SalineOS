/*
 * GDI bitmap objects
 *
 * Copyright 1993 Alexandre Julliard
 *           1998 Huw D M Davies
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(bitmap);


static HGDIOBJ BITMAP_SelectObject( HGDIOBJ handle, HDC hdc );
static INT BITMAP_GetObject( HGDIOBJ handle, INT count, LPVOID buffer );
static BOOL BITMAP_DeleteObject( HGDIOBJ handle );

static const struct gdi_obj_funcs bitmap_funcs =
{
    BITMAP_SelectObject,  /* pSelectObject */
    BITMAP_GetObject,     /* pGetObjectA */
    BITMAP_GetObject,     /* pGetObjectW */
    NULL,                 /* pUnrealizeObject */
    BITMAP_DeleteObject   /* pDeleteObject */
};


/***********************************************************************
 *           null driver fallback implementations
 */

DWORD nulldrv_GetImage( PHYSDEV dev, HBITMAP hbitmap, BITMAPINFO *info,
                        struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    if (!hbitmap) return ERROR_NOT_SUPPORTED;
    return dib_driver.pGetImage( 0, hbitmap, info, bits, src );
}

DWORD nulldrv_PutImage( PHYSDEV dev, HBITMAP hbitmap, HRGN clip, BITMAPINFO *info,
                        const struct gdi_image_bits *bits, struct bitblt_coords *src,
                        struct bitblt_coords *dst, DWORD rop )
{
    if (!hbitmap) return ERROR_SUCCESS;
    return dib_driver.pPutImage( NULL, hbitmap, clip, info, bits, src, dst, rop );
}

BOOL nulldrv_CopyBitmap( HBITMAP src, HBITMAP dst )
{
    BOOL ret = TRUE;
    BITMAPOBJ *src_bmp = GDI_GetObjPtr( src, OBJ_BITMAP );

    if (!src_bmp) return FALSE;
    if (src_bmp->dib.dsBm.bmBits)
    {
        BITMAPOBJ *dst_bmp = GDI_GetObjPtr( dst, OBJ_BITMAP );
        int stride = get_dib_stride( dst_bmp->dib.dsBm.bmWidth, dst_bmp->dib.dsBm.bmBitsPixel );
        dst_bmp->dib.dsBm.bmBits = HeapAlloc( GetProcessHeap(), 0, dst_bmp->dib.dsBm.bmHeight * stride );
        if (dst_bmp->dib.dsBm.bmBits)
            memcpy( dst_bmp->dib.dsBm.bmBits, src_bmp->dib.dsBm.bmBits, dst_bmp->dib.dsBm.bmHeight * stride );
        else
            ret = FALSE;
        GDI_ReleaseObj( dst );
    }
    GDI_ReleaseObj( src );
    return ret;
}


/******************************************************************************
 * CreateBitmap [GDI32.@]
 *
 * Creates a bitmap with the specified info.
 *
 * PARAMS
 *    width  [I] bitmap width
 *    height [I] bitmap height
 *    planes [I] Number of color planes
 *    bpp    [I] Number of bits to identify a color
 *    bits   [I] Pointer to array containing color data
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: 0
 */
HBITMAP WINAPI CreateBitmap( INT width, INT height, UINT planes,
                             UINT bpp, LPCVOID bits )
{
    BITMAP bm;

    bm.bmType = 0;
    bm.bmWidth = width;
    bm.bmHeight = height;
    bm.bmWidthBytes = get_bitmap_stride( width, bpp );
    bm.bmPlanes = planes;
    bm.bmBitsPixel = bpp;
    bm.bmBits = (LPVOID)bits;

    return CreateBitmapIndirect( &bm );
}

/******************************************************************************
 * CreateCompatibleBitmap [GDI32.@]
 *
 * Creates a bitmap compatible with the DC.
 *
 * PARAMS
 *    hdc    [I] Handle to device context
 *    width  [I] Width of bitmap
 *    height [I] Height of bitmap
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: 0
 */
HBITMAP WINAPI CreateCompatibleBitmap( HDC hdc, INT width, INT height)
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *bi = (BITMAPINFO *)buffer;
    DIBSECTION dib;

    TRACE("(%p,%d,%d)\n", hdc, width, height);

    if (GetObjectType( hdc ) != OBJ_MEMDC)
        return CreateBitmap( width, height,
                             GetDeviceCaps(hdc, PLANES), GetDeviceCaps(hdc, BITSPIXEL), NULL );

    switch (GetObjectW( GetCurrentObject( hdc, OBJ_BITMAP ), sizeof(dib), &dib ))
    {
    case sizeof(BITMAP): /* A device-dependent bitmap is selected in the DC */
        return CreateBitmap( width, height, dib.dsBm.bmPlanes, dib.dsBm.bmBitsPixel, NULL );

    case sizeof(DIBSECTION): /* A DIB section is selected in the DC */
        bi->bmiHeader = dib.dsBmih;
        bi->bmiHeader.biWidth  = width;
        bi->bmiHeader.biHeight = height;
        if (dib.dsBmih.biCompression == BI_BITFIELDS)  /* copy the color masks */
            memcpy(bi->bmiColors, dib.dsBitfields, sizeof(dib.dsBitfields));
        else if (dib.dsBmih.biBitCount <= 8)  /* copy the color table */
            GetDIBColorTable(hdc, 0, 256, bi->bmiColors);
        return CreateDIBSection( hdc, bi, DIB_RGB_COLORS, NULL, NULL, 0 );

    default:
        return 0;
    }
}


/******************************************************************************
 * CreateBitmapIndirect [GDI32.@]
 *
 * Creates a bitmap with the specified info.
 *
 * PARAMS
 *  bmp [I] Pointer to the bitmap info describing the bitmap
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: NULL. Use GetLastError() to determine the cause.
 *
 * NOTES
 *  If a width or height of 0 are given, a 1x1 monochrome bitmap is returned.
 */
HBITMAP WINAPI CreateBitmapIndirect( const BITMAP *bmp )
{
    BITMAP bm;
    BITMAPOBJ *bmpobj;
    HBITMAP hbitmap;

    if (!bmp || bmp->bmType)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return NULL;
    }

    if (bmp->bmWidth > 0x7ffffff || bmp->bmHeight > 0x7ffffff)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    bm = *bmp;

    if (!bm.bmWidth || !bm.bmHeight)
    {
        return GetStockObject( DEFAULT_BITMAP );
    }
    else
    {
        if (bm.bmHeight < 0)
            bm.bmHeight = -bm.bmHeight;
        if (bm.bmWidth < 0)
            bm.bmWidth = -bm.bmWidth;
    }

    if (bm.bmPlanes != 1)
    {
        FIXME("planes = %d\n", bm.bmPlanes);
        SetLastError( ERROR_INVALID_PARAMETER );
        return NULL;
    }

    /* Windows only uses 1, 4, 8, 16, 24 and 32 bpp */
    if(bm.bmBitsPixel == 1)         bm.bmBitsPixel = 1;
    else if(bm.bmBitsPixel <= 4)    bm.bmBitsPixel = 4;
    else if(bm.bmBitsPixel <= 8)    bm.bmBitsPixel = 8;
    else if(bm.bmBitsPixel <= 16)   bm.bmBitsPixel = 16;
    else if(bm.bmBitsPixel <= 24)   bm.bmBitsPixel = 24;
    else if(bm.bmBitsPixel <= 32)   bm.bmBitsPixel = 32;
    else {
        WARN("Invalid bmBitsPixel %d, returning ERROR_INVALID_PARAMETER\n", bm.bmBitsPixel);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* Windows ignores the provided bm.bmWidthBytes */
    bm.bmWidthBytes = get_bitmap_stride( bm.bmWidth, bm.bmBitsPixel );
    /* XP doesn't allow to create bitmaps larger than 128 Mb */
    if (bm.bmHeight > 128 * 1024 * 1024 / bm.bmWidthBytes)
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }

    /* Create the BITMAPOBJ */
    if (!(bmpobj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*bmpobj) )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }

    bmpobj->dib.dsBm = bm;
    bmpobj->dib.dsBm.bmBits = NULL;
    bmpobj->funcs = &null_driver;

    if (!(hbitmap = alloc_gdi_handle( &bmpobj->header, OBJ_BITMAP, &bitmap_funcs )))
    {
        HeapFree( GetProcessHeap(), 0, bmpobj );
        return 0;
    }

    if (bm.bmBits)
        SetBitmapBits( hbitmap, bm.bmHeight * bm.bmWidthBytes, bm.bmBits );

    TRACE("%dx%d, bpp %d planes %d: returning %p\n", bm.bmWidth, bm.bmHeight,
          bm.bmBitsPixel, bm.bmPlanes, hbitmap);

    return hbitmap;
}


/***********************************************************************
 * GetBitmapBits [GDI32.@]
 *
 * Copies bitmap bits of bitmap to buffer.
 *
 * RETURNS
 *    Success: Number of bytes copied
 *    Failure: 0
 */
LONG WINAPI GetBitmapBits(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    LONG count,        /* [in]  Number of bytes to copy */
    LPVOID bits)       /* [out] Pointer to buffer to receive bits */
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct gdi_image_bits src_bits;
    struct bitblt_coords src;
    int dst_stride, max, ret;
    BITMAPOBJ *bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );

    if (!bmp) return 0;

    dst_stride = get_bitmap_stride( bmp->dib.dsBm.bmWidth, bmp->dib.dsBm.bmBitsPixel );
    ret = max = dst_stride * bmp->dib.dsBm.bmHeight;
    if (!bits) goto done;
    if (count > max) count = max;
    ret = count;

    src.visrect.left = 0;
    src.visrect.right = bmp->dib.dsBm.bmWidth;
    src.visrect.top = 0;
    src.visrect.bottom = (count + dst_stride - 1) / dst_stride;
    src.x = src.y = 0;
    src.width = src.visrect.right - src.visrect.left;
    src.height = src.visrect.bottom - src.visrect.top;

    if (!bmp->funcs->pGetImage( NULL, hbitmap, info, &src_bits, &src ))
    {
        const char *src_ptr = src_bits.ptr;
        int src_stride = get_dib_stride( info->bmiHeader.biWidth, info->bmiHeader.biBitCount );

        /* GetBitmapBits returns 16-bit aligned data */

        if (info->bmiHeader.biHeight > 0)
        {
            src_ptr += (info->bmiHeader.biHeight - 1) * src_stride;
            src_stride = -src_stride;
        }
        src_ptr += src.visrect.top * src_stride;

        if (src_stride == dst_stride) memcpy( bits, src_ptr, count );
        else while (count > 0)
        {
            memcpy( bits, src_ptr, min( count, dst_stride ) );
            src_ptr += src_stride;
            bits = (char *)bits + dst_stride;
            count -= dst_stride;
        }
        if (src_bits.free) src_bits.free( &src_bits );
    }
    else ret = 0;

done:
    GDI_ReleaseObj( hbitmap );
    return ret;
}


/******************************************************************************
 * SetBitmapBits [GDI32.@]
 *
 * Sets bits of color data for a bitmap.
 *
 * RETURNS
 *    Success: Number of bytes used in setting the bitmap bits
 *    Failure: 0
 */
LONG WINAPI SetBitmapBits(
    HBITMAP hbitmap, /* [in] Handle to bitmap */
    LONG count,        /* [in] Number of bytes in bitmap array */
    LPCVOID bits)      /* [in] Address of array with bitmap bits */
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAPOBJ *bmp;
    DWORD err;
    int i, src_stride, dst_stride;
    struct bitblt_coords src, dst;
    struct gdi_image_bits src_bits;
    HRGN clip = NULL;

    if (!bits) return 0;

    bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );
    if (!bmp) return 0;

    if (count < 0) {
	WARN("(%d): Negative number of bytes passed???\n", count );
	count = -count;
    }

    src_stride = get_bitmap_stride( bmp->dib.dsBm.bmWidth, bmp->dib.dsBm.bmBitsPixel );
    count = min( count, src_stride * bmp->dib.dsBm.bmHeight );

    dst_stride = get_dib_stride( bmp->dib.dsBm.bmWidth, bmp->dib.dsBm.bmBitsPixel );

    src.visrect.left   = src.x = 0;
    src.visrect.top    = src.y = 0;
    src.visrect.right  = src.width = bmp->dib.dsBm.bmWidth;
    src.visrect.bottom = src.height = (count + src_stride - 1 ) / src_stride;
    dst = src;

    if (count % src_stride)
    {
        HRGN last_row;
        int extra_pixels = ((count % src_stride) << 3) / bmp->dib.dsBm.bmBitsPixel;

        if ((count % src_stride << 3) % bmp->dib.dsBm.bmBitsPixel)
            FIXME( "Unhandled partial pixel\n" );
        clip = CreateRectRgn( src.visrect.left, src.visrect.top,
                              src.visrect.right, src.visrect.bottom - 1 );
        last_row = CreateRectRgn( src.visrect.left, src.visrect.bottom - 1,
                                  src.visrect.left + extra_pixels, src.visrect.bottom );
        CombineRgn( clip, clip, last_row, RGN_OR );
        DeleteObject( last_row );
    }

    TRACE("(%p, %d, %p) %dx%d %d bpp fetched height: %d\n",
          hbitmap, count, bits, bmp->dib.dsBm.bmWidth, bmp->dib.dsBm.bmHeight,
          bmp->dib.dsBm.bmBitsPixel, src.height );

    if (src_stride == dst_stride)
    {
        src_bits.ptr = (void *)bits;
        src_bits.is_copy = FALSE;
        src_bits.free = NULL;
    }
    else
    {
        if (!(src_bits.ptr = HeapAlloc( GetProcessHeap(), 0, dst.height * dst_stride )))
        {
            GDI_ReleaseObj( hbitmap );
            return 0;
        }
        src_bits.is_copy = TRUE;
        src_bits.free = free_heap_bits;
        for (i = 0; i < count / src_stride; i++)
            memcpy( (char *)src_bits.ptr + i * dst_stride, (char *)bits + i * src_stride, src_stride );
        if (count % src_stride)
            memcpy( (char *)src_bits.ptr + i * dst_stride, (char *)bits + i * src_stride, count % src_stride );
    }

    /* query the color info */
    info->bmiHeader.biSize          = sizeof(info->bmiHeader);
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = bmp->dib.dsBm.bmBitsPixel;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed       = 0;
    info->bmiHeader.biClrImportant  = 0;
    info->bmiHeader.biWidth         = 0;
    info->bmiHeader.biHeight        = 0;
    info->bmiHeader.biSizeImage     = 0;
    err = bmp->funcs->pPutImage( NULL, hbitmap, 0, info, NULL, NULL, NULL, SRCCOPY );

    if (!err || err == ERROR_BAD_FORMAT)
    {
        info->bmiHeader.biWidth     = bmp->dib.dsBm.bmWidth;
        info->bmiHeader.biHeight    = -dst.height;
        info->bmiHeader.biSizeImage = dst.height * dst_stride;
        err = bmp->funcs->pPutImage( NULL, hbitmap, clip, info, &src_bits, &src, &dst, SRCCOPY );
    }
    if (err) count = 0;

    if (clip) DeleteObject( clip );
    if (src_bits.free) src_bits.free( &src_bits );
    GDI_ReleaseObj( hbitmap );
    return count;
}


static void set_initial_bitmap_bits( HBITMAP hbitmap, BITMAPOBJ *bmp )
{
    char src_bmibuf[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *src_info = (BITMAPINFO *)src_bmibuf;
    char dst_bmibuf[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *dst_info = (BITMAPINFO *)dst_bmibuf;
    DWORD err;
    struct gdi_image_bits bits;
    struct bitblt_coords src, dst;

    if (!bmp->dib.dsBm.bmBits) return;
    if (bmp->funcs->pPutImage == nulldrv_PutImage) return;

    get_ddb_bitmapinfo( bmp, src_info );

    bits.ptr = bmp->dib.dsBm.bmBits;
    bits.is_copy = FALSE;
    bits.free = NULL;
    bits.param = NULL;

    src.x      = 0;
    src.y      = 0;
    src.width  = bmp->dib.dsBm.bmWidth;
    src.height = bmp->dib.dsBm.bmHeight;
    src.visrect.left   = 0;
    src.visrect.top    = 0;
    src.visrect.right  = bmp->dib.dsBm.bmWidth;
    src.visrect.bottom = bmp->dib.dsBm.bmHeight;
    dst = src;

    copy_bitmapinfo( dst_info, src_info );

    err = bmp->funcs->pPutImage( NULL, hbitmap, 0, dst_info, &bits, &src, &dst, 0 );
    if (err == ERROR_BAD_FORMAT)
    {
        err = convert_bits( src_info, &src, dst_info, &bits, FALSE );
        if (!err) err = bmp->funcs->pPutImage( NULL, hbitmap, 0, dst_info, &bits, &src, &dst, 0 );
        if (bits.free) bits.free( &bits );
    }
}

/***********************************************************************
 *           BITMAP_SetOwnerDC
 *
 * Set the type of DC that owns the bitmap. This is used when the
 * bitmap is selected into a device to initialize the bitmap function
 * table.
 */
static BOOL BITMAP_SetOwnerDC( HBITMAP hbitmap, PHYSDEV physdev )
{
    BITMAPOBJ *bitmap;
    BOOL ret = TRUE;

    /* never set the owner of the stock bitmap since it can be selected in multiple DCs */
    if (hbitmap == GetStockObject(DEFAULT_BITMAP)) return TRUE;

    if (!(bitmap = GDI_GetObjPtr( hbitmap, OBJ_BITMAP ))) return FALSE;

    if (bitmap->funcs != physdev->funcs)
    {
        /* we can only change from the null driver to some other driver */
        if (bitmap->funcs == &null_driver)
        {
            if (physdev->funcs->pCreateBitmap)
            {
                ret = physdev->funcs->pCreateBitmap( physdev, hbitmap );
                if (ret)
                {
                    bitmap->funcs = physdev->funcs;
                    set_initial_bitmap_bits( hbitmap, bitmap );
                }
            }
            else bitmap->funcs = &dib_driver;  /* use the DIB driver to emulate DDB support */
        }
        else
        {
            FIXME( "Trying to select bitmap %p in different DC type\n", hbitmap );
            ret = FALSE;
        }
    }
    GDI_ReleaseObj( hbitmap );
    return ret;
}


/***********************************************************************
 *           BITMAP_SelectObject
 */
static HGDIOBJ BITMAP_SelectObject( HGDIOBJ handle, HDC hdc )
{
    HGDIOBJ ret;
    BITMAPOBJ *bitmap;
    DC *dc;
    PHYSDEV physdev = NULL, old_physdev = NULL, pathdev = NULL;

    if (!(dc = get_dc_ptr( hdc ))) return 0;

    if (GetObjectType( hdc ) != OBJ_MEMDC)
    {
        ret = 0;
        goto done;
    }
    ret = dc->hBitmap;
    if (handle == dc->hBitmap) goto done;  /* nothing to do */

    if (!(bitmap = GDI_GetObjPtr( handle, OBJ_BITMAP )))
    {
        ret = 0;
        goto done;
    }

    if (bitmap->header.selcount && (handle != GetStockObject(DEFAULT_BITMAP)))
    {
        WARN( "Bitmap already selected in another DC\n" );
        GDI_ReleaseObj( handle );
        ret = 0;
        goto done;
    }

    if (dc->physDev->funcs == &path_driver) pathdev = pop_dc_driver( &dc->physDev );

    old_physdev = GET_DC_PHYSDEV( dc, pSelectBitmap );
    if(old_physdev == dc->dibdrv)
        old_physdev = pop_dc_driver( &dc->physDev );

    physdev = GET_DC_PHYSDEV( dc, pSelectBitmap );
    if (physdev->funcs == &null_driver)
    {
        physdev = dc->dibdrv;
        if (physdev) push_dc_driver( &dc->physDev, physdev, physdev->funcs );
        else
        {
            if (!dib_driver.pCreateDC( &dc->physDev, NULL, NULL, NULL, NULL )) goto done;
            dc->dibdrv = physdev = dc->physDev;
        }
    }

    if (!BITMAP_SetOwnerDC( handle, physdev ))
    {
        GDI_ReleaseObj( handle );
        ret = 0;
        goto done;
    }
    if (!physdev->funcs->pSelectBitmap( physdev, handle ))
    {
        GDI_ReleaseObj( handle );
        ret = 0;
    }
    else
    {
        dc->hBitmap = handle;
        GDI_inc_ref_count( handle );
        dc->dirty = 0;
        dc->vis_rect.left   = 0;
        dc->vis_rect.top    = 0;
        dc->vis_rect.right  = bitmap->dib.dsBm.bmWidth;
        dc->vis_rect.bottom = bitmap->dib.dsBm.bmHeight;
        GDI_ReleaseObj( handle );
        DC_InitDC( dc );
        GDI_dec_ref_count( ret );
    }

 done:
    if(!ret)
    {
        if (physdev && physdev == dc->dibdrv)
            pop_dc_driver( &dc->physDev );
        if (old_physdev && old_physdev == dc->dibdrv)
            push_dc_driver( &dc->physDev, old_physdev, old_physdev->funcs );
    }
    if (pathdev) push_dc_driver( &dc->physDev, pathdev, pathdev->funcs );
    release_dc_ptr( dc );
    return ret;
}


/***********************************************************************
 *           BITMAP_DeleteObject
 */
static BOOL BITMAP_DeleteObject( HGDIOBJ handle )
{
    const struct gdi_dc_funcs *funcs;
    BITMAPOBJ *bmp = GDI_GetObjPtr( handle, OBJ_BITMAP );

    if (!bmp) return FALSE;
    funcs = bmp->funcs;
    GDI_ReleaseObj( handle );

    funcs->pDeleteBitmap( handle );

    if (!(bmp = free_gdi_handle( handle ))) return FALSE;

    HeapFree( GetProcessHeap(), 0, bmp->dib.dsBm.bmBits );
    return HeapFree( GetProcessHeap(), 0, bmp );
}


/***********************************************************************
 *           BITMAP_GetObject
 */
static INT BITMAP_GetObject( HGDIOBJ handle, INT count, LPVOID buffer )
{
    INT ret = 0;
    BITMAPOBJ *bmp = GDI_GetObjPtr( handle, OBJ_BITMAP );

    if (!bmp) return 0;

    if (!buffer) ret = sizeof(BITMAP);
    else if (count >= sizeof(BITMAP))
    {
        BITMAP *bitmap = buffer;
        *bitmap = bmp->dib.dsBm;
        bitmap->bmBits = NULL;
        ret = sizeof(BITMAP);
    }
    GDI_ReleaseObj( handle );
    return ret;
}


/******************************************************************************
 * CreateDiscardableBitmap [GDI32.@]
 *
 * Creates a discardable bitmap.
 *
 * RETURNS
 *    Success: Handle to bitmap
 *    Failure: NULL
 */
HBITMAP WINAPI CreateDiscardableBitmap(
    HDC hdc,    /* [in] Handle to device context */
    INT width,  /* [in] Bitmap width */
    INT height) /* [in] Bitmap height */
{
    return CreateCompatibleBitmap( hdc, width, height );
}


/******************************************************************************
 * GetBitmapDimensionEx [GDI32.@]
 *
 * Retrieves dimensions of a bitmap.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetBitmapDimensionEx(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    LPSIZE size)     /* [out] Address of struct receiving dimensions */
{
    BITMAPOBJ * bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );
    if (!bmp) return FALSE;
    *size = bmp->size;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}


/******************************************************************************
 * SetBitmapDimensionEx [GDI32.@]
 *
 * Assigns dimensions to a bitmap.
 * MSDN says that this function will fail if hbitmap is a handle created by
 * CreateDIBSection, but that's not true on Windows 2000.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetBitmapDimensionEx(
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    INT x,           /* [in]  Bitmap width */
    INT y,           /* [in]  Bitmap height */
    LPSIZE prevSize) /* [out] Address of structure for orig dims */
{
    BITMAPOBJ * bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );
    if (!bmp) return FALSE;
    if (prevSize) *prevSize = bmp->size;
    bmp->size.cx = x;
    bmp->size.cy = y;
    GDI_ReleaseObj( hbitmap );
    return TRUE;
}
