/*
 * Copyright 2009 Vincent Povirk for CodeWeavers
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
#include "wine/port.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#ifdef SONAME_LIBJPEG
/* This is a hack, so jpeglib.h does not redefine INT32 and the like*/
#define XMD_H
#define UINT8 JPEG_UINT8
#define UINT16 JPEG_UINT16
#define boolean jpeg_boolean
#undef HAVE_STDLIB_H
# include <jpeglib.h>
#undef HAVE_STDLIB_H
#define HAVE_STDLIB_H 1
#undef UINT8
#undef UINT16
#undef boolean
#endif

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "wincodec.h"

#include "wincodecs_private.h"

#include "wine/debug.h"
#include "wine/library.h"

WINE_DEFAULT_DEBUG_CHANNEL(wincodecs);

#ifdef SONAME_LIBJPEG
WINE_DECLARE_DEBUG_CHANNEL(jpeg);

static void *libjpeg_handle;

#define MAKE_FUNCPTR(f) static typeof(f) * p##f
MAKE_FUNCPTR(jpeg_CreateDecompress);
MAKE_FUNCPTR(jpeg_destroy_decompress);
MAKE_FUNCPTR(jpeg_read_header);
MAKE_FUNCPTR(jpeg_read_scanlines);
MAKE_FUNCPTR(jpeg_resync_to_restart);
MAKE_FUNCPTR(jpeg_start_decompress);
MAKE_FUNCPTR(jpeg_std_error);
#undef MAKE_FUNCPTR

static void *load_libjpeg(void)
{
    if((libjpeg_handle = wine_dlopen(SONAME_LIBJPEG, RTLD_NOW, NULL, 0)) != NULL) {

#define LOAD_FUNCPTR(f) \
    if((p##f = wine_dlsym(libjpeg_handle, #f, NULL, 0)) == NULL) { \
        libjpeg_handle = NULL; \
        return NULL; \
    }

        LOAD_FUNCPTR(jpeg_CreateDecompress);
        LOAD_FUNCPTR(jpeg_destroy_decompress);
        LOAD_FUNCPTR(jpeg_read_header);
        LOAD_FUNCPTR(jpeg_read_scanlines);
        LOAD_FUNCPTR(jpeg_resync_to_restart);
        LOAD_FUNCPTR(jpeg_start_decompress);
        LOAD_FUNCPTR(jpeg_std_error);
#undef LOAD_FUNCPTR
    }
    return libjpeg_handle;
}

static void error_exit_fn(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX];
    if (ERR_ON(jpeg))
    {
        cinfo->err->format_message(cinfo, message);
        ERR_(jpeg)("%s\n", message);
    }
    longjmp(*(jmp_buf*)cinfo->client_data, 1);
}

static void emit_message_fn(j_common_ptr cinfo, int msg_level)
{
    char message[JMSG_LENGTH_MAX];

    if (msg_level < 0 && ERR_ON(jpeg))
    {
        cinfo->err->format_message(cinfo, message);
        ERR_(jpeg)("%s\n", message);
    }
    else if (msg_level == 0 && WARN_ON(jpeg))
    {
        cinfo->err->format_message(cinfo, message);
        WARN_(jpeg)("%s\n", message);
    }
    else if (msg_level > 0 && TRACE_ON(jpeg))
    {
        cinfo->err->format_message(cinfo, message);
        TRACE_(jpeg)("%s\n", message);
    }
}

typedef struct {
    IWICBitmapDecoder IWICBitmapDecoder_iface;
    IWICBitmapFrameDecode IWICBitmapFrameDecode_iface;
    LONG ref;
    BOOL initialized;
    BOOL cinfo_initialized;
    IStream *stream;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_source_mgr source_mgr;
    BYTE source_buffer[1024];
    BYTE *image_data;
    CRITICAL_SECTION lock;
} JpegDecoder;

static inline JpegDecoder *impl_from_IWICBitmapDecoder(IWICBitmapDecoder *iface)
{
    return CONTAINING_RECORD(iface, JpegDecoder, IWICBitmapDecoder_iface);
}

static inline JpegDecoder *impl_from_IWICBitmapFrameDecode(IWICBitmapFrameDecode *iface)
{
    return CONTAINING_RECORD(iface, JpegDecoder, IWICBitmapFrameDecode_iface);
}

static inline JpegDecoder *decoder_from_decompress(j_decompress_ptr decompress)
{
    return CONTAINING_RECORD(decompress, JpegDecoder, cinfo);
}

static HRESULT WINAPI JpegDecoder_QueryInterface(IWICBitmapDecoder *iface, REFIID iid,
    void **ppv)
{
    JpegDecoder *This = impl_from_IWICBitmapDecoder(iface);
    TRACE("(%p,%s,%p)\n", iface, debugstr_guid(iid), ppv);

    if (!ppv) return E_INVALIDARG;

    if (IsEqualIID(&IID_IUnknown, iid) || IsEqualIID(&IID_IWICBitmapDecoder, iid))
    {
        *ppv = This;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI JpegDecoder_AddRef(IWICBitmapDecoder *iface)
{
    JpegDecoder *This = impl_from_IWICBitmapDecoder(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) refcount=%u\n", iface, ref);

    return ref;
}

static ULONG WINAPI JpegDecoder_Release(IWICBitmapDecoder *iface)
{
    JpegDecoder *This = impl_from_IWICBitmapDecoder(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) refcount=%u\n", iface, ref);

    if (ref == 0)
    {
        This->lock.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&This->lock);
        if (This->cinfo_initialized) pjpeg_destroy_decompress(&This->cinfo);
        if (This->stream) IStream_Release(This->stream);
        HeapFree(GetProcessHeap(), 0, This->image_data);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI JpegDecoder_QueryCapability(IWICBitmapDecoder *iface, IStream *pIStream,
    DWORD *pdwCapability)
{
    FIXME("(%p,%p,%p): stub\n", iface, pIStream, pdwCapability);
    return E_NOTIMPL;
}

static void source_mgr_init_source(j_decompress_ptr cinfo)
{
}

static jpeg_boolean source_mgr_fill_input_buffer(j_decompress_ptr cinfo)
{
    JpegDecoder *This = decoder_from_decompress(cinfo);
    HRESULT hr;
    ULONG bytesread;

    hr = IStream_Read(This->stream, This->source_buffer, 1024, &bytesread);

    if (hr != S_OK || bytesread == 0)
    {
        return FALSE;
    }
    else
    {
        This->source_mgr.next_input_byte = This->source_buffer;
        This->source_mgr.bytes_in_buffer = bytesread;
        return TRUE;
    }
}

static void source_mgr_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    JpegDecoder *This = decoder_from_decompress(cinfo);
    LARGE_INTEGER seek;

    if (num_bytes > This->source_mgr.bytes_in_buffer)
    {
        seek.QuadPart = num_bytes - This->source_mgr.bytes_in_buffer;
        IStream_Seek(This->stream, seek, STREAM_SEEK_CUR, NULL);
        This->source_mgr.bytes_in_buffer = 0;
    }
    else if (num_bytes > 0)
    {
        This->source_mgr.next_input_byte += num_bytes;
        This->source_mgr.bytes_in_buffer -= num_bytes;
    }
}

static void source_mgr_term_source(j_decompress_ptr cinfo)
{
}

static HRESULT WINAPI JpegDecoder_Initialize(IWICBitmapDecoder *iface, IStream *pIStream,
    WICDecodeOptions cacheOptions)
{
    JpegDecoder *This = impl_from_IWICBitmapDecoder(iface);
    int ret;
    LARGE_INTEGER seek;
    jmp_buf jmpbuf;
    TRACE("(%p,%p,%u)\n", iface, pIStream, cacheOptions);

    EnterCriticalSection(&This->lock);

    if (This->cinfo_initialized)
    {
        LeaveCriticalSection(&This->lock);
        return WINCODEC_ERR_WRONGSTATE;
    }

    pjpeg_std_error(&This->jerr);

    This->jerr.error_exit = error_exit_fn;
    This->jerr.emit_message = emit_message_fn;

    This->cinfo.err = &This->jerr;

    This->cinfo.client_data = jmpbuf;

    if (setjmp(jmpbuf))
    {
        LeaveCriticalSection(&This->lock);
        return E_FAIL;
    }

    pjpeg_CreateDecompress(&This->cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));

    This->cinfo_initialized = TRUE;

    This->stream = pIStream;
    IStream_AddRef(pIStream);

    seek.QuadPart = 0;
    IStream_Seek(This->stream, seek, STREAM_SEEK_SET, NULL);

    This->source_mgr.bytes_in_buffer = 0;
    This->source_mgr.init_source = source_mgr_init_source;
    This->source_mgr.fill_input_buffer = source_mgr_fill_input_buffer;
    This->source_mgr.skip_input_data = source_mgr_skip_input_data;
    This->source_mgr.resync_to_restart = pjpeg_resync_to_restart;
    This->source_mgr.term_source = source_mgr_term_source;

    This->cinfo.src = &This->source_mgr;

    ret = pjpeg_read_header(&This->cinfo, TRUE);

    if (ret != JPEG_HEADER_OK) {
        WARN("Jpeg image in stream has bad format, read header returned %d.\n",ret);
        LeaveCriticalSection(&This->lock);
        return E_FAIL;
    }

    switch (This->cinfo.jpeg_color_space)
    {
    case JCS_GRAYSCALE:
        This->cinfo.out_color_space = JCS_GRAYSCALE;
        break;
    case JCS_RGB:
    case JCS_YCbCr:
        This->cinfo.out_color_space = JCS_RGB;
        break;
    case JCS_CMYK:
    case JCS_YCCK:
        This->cinfo.out_color_space = JCS_CMYK;
        break;
    default:
        ERR("Unknown JPEG color space %i\n", This->cinfo.jpeg_color_space);
        LeaveCriticalSection(&This->lock);
        return E_FAIL;
    }

    if (!pjpeg_start_decompress(&This->cinfo))
    {
        ERR("jpeg_start_decompress failed\n");
        LeaveCriticalSection(&This->lock);
        return E_FAIL;
    }

    This->initialized = TRUE;

    LeaveCriticalSection(&This->lock);

    return S_OK;
}

static HRESULT WINAPI JpegDecoder_GetContainerFormat(IWICBitmapDecoder *iface,
    GUID *pguidContainerFormat)
{
    memcpy(pguidContainerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID));
    return S_OK;
}

static HRESULT WINAPI JpegDecoder_GetDecoderInfo(IWICBitmapDecoder *iface,
    IWICBitmapDecoderInfo **ppIDecoderInfo)
{
    HRESULT hr;
    IWICComponentInfo *compinfo;

    TRACE("(%p,%p)\n", iface, ppIDecoderInfo);

    hr = CreateComponentInfo(&CLSID_WICJpegDecoder, &compinfo);
    if (FAILED(hr)) return hr;

    hr = IWICComponentInfo_QueryInterface(compinfo, &IID_IWICBitmapDecoderInfo,
        (void**)ppIDecoderInfo);

    IWICComponentInfo_Release(compinfo);

    return hr;
}

static HRESULT WINAPI JpegDecoder_CopyPalette(IWICBitmapDecoder *iface,
    IWICPalette *pIPalette)
{
    TRACE("(%p,%p)\n", iface, pIPalette);

    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

static HRESULT WINAPI JpegDecoder_GetMetadataQueryReader(IWICBitmapDecoder *iface,
    IWICMetadataQueryReader **ppIMetadataQueryReader)
{
    FIXME("(%p,%p): stub\n", iface, ppIMetadataQueryReader);
    return E_NOTIMPL;
}

static HRESULT WINAPI JpegDecoder_GetPreview(IWICBitmapDecoder *iface,
    IWICBitmapSource **ppIBitmapSource)
{
    FIXME("(%p,%p): stub\n", iface, ppIBitmapSource);
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

static HRESULT WINAPI JpegDecoder_GetColorContexts(IWICBitmapDecoder *iface,
    UINT cCount, IWICColorContext **ppIColorContexts, UINT *pcActualCount)
{
    FIXME("(%p,%u,%p,%p): stub\n", iface, cCount, ppIColorContexts, pcActualCount);
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

static HRESULT WINAPI JpegDecoder_GetThumbnail(IWICBitmapDecoder *iface,
    IWICBitmapSource **ppIThumbnail)
{
    FIXME("(%p,%p): stub\n", iface, ppIThumbnail);
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

static HRESULT WINAPI JpegDecoder_GetFrameCount(IWICBitmapDecoder *iface,
    UINT *pCount)
{
    *pCount = 1;
    return S_OK;
}

static HRESULT WINAPI JpegDecoder_GetFrame(IWICBitmapDecoder *iface,
    UINT index, IWICBitmapFrameDecode **ppIBitmapFrame)
{
    JpegDecoder *This = impl_from_IWICBitmapDecoder(iface);
    TRACE("(%p,%u,%p)\n", iface, index, ppIBitmapFrame);

    if (!This->initialized) return WINCODEC_ERR_NOTINITIALIZED;

    if (index != 0) return E_INVALIDARG;

    IWICBitmapDecoder_AddRef(iface);
    *ppIBitmapFrame = &This->IWICBitmapFrameDecode_iface;

    return S_OK;
}

static const IWICBitmapDecoderVtbl JpegDecoder_Vtbl = {
    JpegDecoder_QueryInterface,
    JpegDecoder_AddRef,
    JpegDecoder_Release,
    JpegDecoder_QueryCapability,
    JpegDecoder_Initialize,
    JpegDecoder_GetContainerFormat,
    JpegDecoder_GetDecoderInfo,
    JpegDecoder_CopyPalette,
    JpegDecoder_GetMetadataQueryReader,
    JpegDecoder_GetPreview,
    JpegDecoder_GetColorContexts,
    JpegDecoder_GetThumbnail,
    JpegDecoder_GetFrameCount,
    JpegDecoder_GetFrame
};

static HRESULT WINAPI JpegDecoder_Frame_QueryInterface(IWICBitmapFrameDecode *iface, REFIID iid,
    void **ppv)
{
    TRACE("(%p,%s,%p)\n", iface, debugstr_guid(iid), ppv);

    if (!ppv) return E_INVALIDARG;

    if (IsEqualIID(&IID_IUnknown, iid) ||
        IsEqualIID(&IID_IWICBitmapSource, iid) ||
        IsEqualIID(&IID_IWICBitmapFrameDecode, iid))
    {
        *ppv = iface;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI JpegDecoder_Frame_AddRef(IWICBitmapFrameDecode *iface)
{
    JpegDecoder *This = impl_from_IWICBitmapFrameDecode(iface);
    return IUnknown_AddRef((IUnknown*)This);
}

static ULONG WINAPI JpegDecoder_Frame_Release(IWICBitmapFrameDecode *iface)
{
    JpegDecoder *This = impl_from_IWICBitmapFrameDecode(iface);
    return IUnknown_Release((IUnknown*)This);
}

static HRESULT WINAPI JpegDecoder_Frame_GetSize(IWICBitmapFrameDecode *iface,
    UINT *puiWidth, UINT *puiHeight)
{
    JpegDecoder *This = impl_from_IWICBitmapFrameDecode(iface);
    *puiWidth = This->cinfo.output_width;
    *puiHeight = This->cinfo.output_height;
    TRACE("(%p)->(%u,%u)\n", iface, *puiWidth, *puiHeight);
    return S_OK;
}

static HRESULT WINAPI JpegDecoder_Frame_GetPixelFormat(IWICBitmapFrameDecode *iface,
    WICPixelFormatGUID *pPixelFormat)
{
    JpegDecoder *This = impl_from_IWICBitmapFrameDecode(iface);
    TRACE("(%p,%p)\n", iface, pPixelFormat);
    if (This->cinfo.out_color_space == JCS_RGB)
        memcpy(pPixelFormat, &GUID_WICPixelFormat24bppBGR, sizeof(GUID));
    else if (This->cinfo.out_color_space == JCS_CMYK)
        memcpy(pPixelFormat, &GUID_WICPixelFormat32bppCMYK, sizeof(GUID));
    else /* This->cinfo.out_color_space == JCS_GRAYSCALE */
        memcpy(pPixelFormat, &GUID_WICPixelFormat8bppGray, sizeof(GUID));
    return S_OK;
}

static HRESULT WINAPI JpegDecoder_Frame_GetResolution(IWICBitmapFrameDecode *iface,
    double *pDpiX, double *pDpiY)
{
    FIXME("(%p,%p,%p): stub\n", iface, pDpiX, pDpiY);
    return E_NOTIMPL;
}

static HRESULT WINAPI JpegDecoder_Frame_CopyPalette(IWICBitmapFrameDecode *iface,
    IWICPalette *pIPalette)
{
    FIXME("(%p,%p): stub\n", iface, pIPalette);
    return E_NOTIMPL;
}

static HRESULT WINAPI JpegDecoder_Frame_CopyPixels(IWICBitmapFrameDecode *iface,
    const WICRect *prc, UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer)
{
    JpegDecoder *This = impl_from_IWICBitmapFrameDecode(iface);
    UINT bpp;
    UINT stride;
    UINT data_size;
    UINT max_row_needed;
    jmp_buf jmpbuf;
    WICRect rect;
    TRACE("(%p,%p,%u,%u,%p)\n", iface, prc, cbStride, cbBufferSize, pbBuffer);

    if (!prc)
    {
        rect.X = 0;
        rect.Y = 0;
        rect.Width = This->cinfo.output_width;
        rect.Height = This->cinfo.output_height;
        prc = &rect;
    }
    else
    {
        if (prc->X < 0 || prc->Y < 0 || prc->X+prc->Width > This->cinfo.output_width ||
            prc->Y+prc->Height > This->cinfo.output_height)
            return E_INVALIDARG;
    }

    if (This->cinfo.out_color_space == JCS_GRAYSCALE) bpp = 8;
    else if (This->cinfo.out_color_space == JCS_CMYK) bpp = 32;
    else bpp = 24;

    stride = bpp * This->cinfo.output_width;
    data_size = stride * This->cinfo.output_height;

    max_row_needed = prc->Y + prc->Height;
    if (max_row_needed > This->cinfo.output_height) return E_INVALIDARG;

    EnterCriticalSection(&This->lock);

    if (!This->image_data)
    {
        This->image_data = HeapAlloc(GetProcessHeap(), 0, data_size);
        if (!This->image_data)
        {
            LeaveCriticalSection(&This->lock);
            return E_OUTOFMEMORY;
        }
    }

    This->cinfo.client_data = jmpbuf;

    if (setjmp(jmpbuf))
    {
        LeaveCriticalSection(&This->lock);
        return E_FAIL;
    }

    while (max_row_needed > This->cinfo.output_scanline)
    {
        UINT first_scanline = This->cinfo.output_scanline;
        UINT max_rows;
        JSAMPROW out_rows[4];
        UINT i;
        JDIMENSION ret;

        max_rows = min(This->cinfo.output_height-first_scanline, 4);
        for (i=0; i<max_rows; i++)
            out_rows[i] = This->image_data + stride * (first_scanline+i);

        ret = pjpeg_read_scanlines(&This->cinfo, out_rows, max_rows);

        if (ret == 0)
        {
            ERR("read_scanlines failed\n");
            LeaveCriticalSection(&This->lock);
            return E_FAIL;
        }

        if (bpp == 24)
        {
            /* libjpeg gives us RGB data and we want BGR, so byteswap the data */
            reverse_bgr8(3, This->image_data + stride * first_scanline,
                This->cinfo.output_width, This->cinfo.output_scanline - first_scanline,
                stride);
        }

        if (This->cinfo.out_color_space == JCS_CMYK && This->cinfo.saw_Adobe_marker)
            /* Adobe JPEG's have inverted CMYK data. */
            for (i=0; i<data_size; i++)
                This->image_data[i] ^= 0xff;
    }

    LeaveCriticalSection(&This->lock);

    return copy_pixels(bpp, This->image_data,
        This->cinfo.output_width, This->cinfo.output_height, stride,
        prc, cbStride, cbBufferSize, pbBuffer);
}

static HRESULT WINAPI JpegDecoder_Frame_GetMetadataQueryReader(IWICBitmapFrameDecode *iface,
    IWICMetadataQueryReader **ppIMetadataQueryReader)
{
    FIXME("(%p,%p): stub\n", iface, ppIMetadataQueryReader);
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

static HRESULT WINAPI JpegDecoder_Frame_GetColorContexts(IWICBitmapFrameDecode *iface,
    UINT cCount, IWICColorContext **ppIColorContexts, UINT *pcActualCount)
{
    FIXME("(%p,%u,%p,%p): stub\n", iface, cCount, ppIColorContexts, pcActualCount);
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

static HRESULT WINAPI JpegDecoder_Frame_GetThumbnail(IWICBitmapFrameDecode *iface,
    IWICBitmapSource **ppIThumbnail)
{
    FIXME("(%p,%p): stub\n", iface, ppIThumbnail);
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

static const IWICBitmapFrameDecodeVtbl JpegDecoder_Frame_Vtbl = {
    JpegDecoder_Frame_QueryInterface,
    JpegDecoder_Frame_AddRef,
    JpegDecoder_Frame_Release,
    JpegDecoder_Frame_GetSize,
    JpegDecoder_Frame_GetPixelFormat,
    JpegDecoder_Frame_GetResolution,
    JpegDecoder_Frame_CopyPalette,
    JpegDecoder_Frame_CopyPixels,
    JpegDecoder_Frame_GetMetadataQueryReader,
    JpegDecoder_Frame_GetColorContexts,
    JpegDecoder_Frame_GetThumbnail
};

HRESULT JpegDecoder_CreateInstance(IUnknown *pUnkOuter, REFIID iid, void** ppv)
{
    JpegDecoder *This;
    HRESULT ret;

    TRACE("(%p,%s,%p)\n", pUnkOuter, debugstr_guid(iid), ppv);

    if (!libjpeg_handle && !load_libjpeg())
    {
        ERR("Failed reading JPEG because unable to find %s\n", SONAME_LIBJPEG);
        return E_FAIL;
    }

    *ppv = NULL;

    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    This = HeapAlloc(GetProcessHeap(), 0, sizeof(JpegDecoder));
    if (!This) return E_OUTOFMEMORY;

    This->IWICBitmapDecoder_iface.lpVtbl = &JpegDecoder_Vtbl;
    This->IWICBitmapFrameDecode_iface.lpVtbl = &JpegDecoder_Frame_Vtbl;
    This->ref = 1;
    This->initialized = FALSE;
    This->cinfo_initialized = FALSE;
    This->stream = NULL;
    This->image_data = NULL;
    InitializeCriticalSection(&This->lock);
    This->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": JpegDecoder.lock");

    ret = IUnknown_QueryInterface((IUnknown*)This, iid, ppv);
    IUnknown_Release((IUnknown*)This);

    return ret;
}

#else /* !defined(SONAME_LIBJPEG) */

HRESULT JpegDecoder_CreateInstance(IUnknown *pUnkOuter, REFIID iid, void** ppv)
{
    ERR("Trying to load JPEG picture, but JPEG support is not compiled in.\n");
    return E_FAIL;
}

#endif
