/*
 *    IXMLHTTPRequest implementation
 *
 * Copyright 2008 Alistair Leslie-Hughes
 * Copyright 2010-2012 Nikolay Sivov for CodeWeavers
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

#define COBJMACROS
#define NONAMELESSUNION

#include "config.h"

#include <stdarg.h>
#ifdef HAVE_LIBXML2
# include <libxml/parser.h>
# include <libxml/xmlerror.h>
# include <libxml/encoding.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wininet.h"
#include "winreg.h"
#include "winuser.h"
#include "ole2.h"
#include "mshtml.h"
#include "msxml6.h"
#include "objsafe.h"
#include "docobj.h"
#include "shlwapi.h"

#include "msxml_private.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(msxml);

#ifdef HAVE_LIBXML2

static const WCHAR colspaceW[] = {':',' ',0};
static const WCHAR crlfW[] = {'\r','\n',0};

typedef struct BindStatusCallback BindStatusCallback;

struct httpheader
{
    struct list entry;
    BSTR header;
    BSTR value;
};

typedef struct
{
    IXMLHTTPRequest IXMLHTTPRequest_iface;
    IObjectWithSite IObjectWithSite_iface;
    IObjectSafety   IObjectSafety_iface;
    LONG ref;

    READYSTATE state;
    IDispatch *sink;

    /* request */
    BINDVERB verb;
    BSTR custom;
    BSTR siteurl;
    BSTR url;
    BOOL async;
    struct list reqheaders;
    /* cached resulting custom request headers string length in WCHARs */
    LONG reqheader_size;
    /* use UTF-8 content type */
    BOOL use_utf8_content;

    /* response headers */
    struct list respheaders;
    BSTR raw_respheaders;

    /* credentials */
    BSTR user;
    BSTR password;

    /* bind callback */
    BindStatusCallback *bsc;
    LONG status;
    BSTR status_text;

    /* IObjectWithSite*/
    IUnknown *site;

    /* IObjectSafety */
    DWORD safeopt;
} httprequest;

static inline httprequest *impl_from_IXMLHTTPRequest( IXMLHTTPRequest *iface )
{
    return CONTAINING_RECORD(iface, httprequest, IXMLHTTPRequest_iface);
}

static inline httprequest *impl_from_IObjectWithSite(IObjectWithSite *iface)
{
    return CONTAINING_RECORD(iface, httprequest, IObjectWithSite_iface);
}

static inline httprequest *impl_from_IObjectSafety(IObjectSafety *iface)
{
    return CONTAINING_RECORD(iface, httprequest, IObjectSafety_iface);
}

static void httprequest_setreadystate(httprequest *This, READYSTATE state)
{
    READYSTATE last = This->state;

    This->state = state;

    if (This->sink && last != state)
    {
        DISPPARAMS params;

        memset(&params, 0, sizeof(params));
        IDispatch_Invoke(This->sink, 0, &IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, &params, 0, 0, 0);
    }
}

static void free_response_headers(httprequest *This)
{
    struct httpheader *header, *header2;

    LIST_FOR_EACH_ENTRY_SAFE(header, header2, &This->respheaders, struct httpheader, entry)
    {
        list_remove(&header->entry);
        SysFreeString(header->header);
        SysFreeString(header->value);
        heap_free(header);
    }

    SysFreeString(This->raw_respheaders);
    This->raw_respheaders = NULL;
}

struct BindStatusCallback
{
    IBindStatusCallback IBindStatusCallback_iface;
    IHttpNegotiate      IHttpNegotiate_iface;
    IAuthenticate       IAuthenticate_iface;
    LONG ref;

    IBinding *binding;
    httprequest *request;

    /* response data */
    IStream *stream;

    /* request body data */
    HGLOBAL body;
};

static inline BindStatusCallback *impl_from_IBindStatusCallback( IBindStatusCallback *iface )
{
    return CONTAINING_RECORD(iface, BindStatusCallback, IBindStatusCallback_iface);
}

static inline BindStatusCallback *impl_from_IHttpNegotiate( IHttpNegotiate *iface )
{
    return CONTAINING_RECORD(iface, BindStatusCallback, IHttpNegotiate_iface);
}

static inline BindStatusCallback *impl_from_IAuthenticate( IAuthenticate *iface )
{
    return CONTAINING_RECORD(iface, BindStatusCallback, IAuthenticate_iface);
}

static void BindStatusCallback_Detach(BindStatusCallback *bsc)
{
    if (bsc)
    {
        if (bsc->binding) IBinding_Abort(bsc->binding);
        bsc->request = NULL;
        IBindStatusCallback_Release(&bsc->IBindStatusCallback_iface);
    }
}

static HRESULT WINAPI BindStatusCallback_QueryInterface(IBindStatusCallback *iface,
        REFIID riid, void **ppv)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    *ppv = NULL;

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(&IID_IUnknown, riid) ||
        IsEqualGUID(&IID_IBindStatusCallback, riid))
    {
        *ppv = &This->IBindStatusCallback_iface;
    }
    else if (IsEqualGUID(&IID_IHttpNegotiate, riid))
    {
        *ppv = &This->IHttpNegotiate_iface;
    }
    else if (IsEqualGUID(&IID_IAuthenticate, riid))
    {
        *ppv = &This->IAuthenticate_iface;
    }
    else if (IsEqualGUID(&IID_IServiceProvider, riid) ||
             IsEqualGUID(&IID_IBindStatusCallbackEx, riid) ||
             IsEqualGUID(&IID_IInternetProtocol, riid) ||
             IsEqualGUID(&IID_IHttpNegotiate2, riid))
    {
        return E_NOINTERFACE;
    }

    if (*ppv)
    {
        IBindStatusCallback_AddRef(iface);
        return S_OK;
    }

    FIXME("Unsupported riid = %s\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI BindStatusCallback_AddRef(IBindStatusCallback *iface)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %d\n", This, ref);

    return ref;
}

static ULONG WINAPI BindStatusCallback_Release(IBindStatusCallback *iface)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %d\n", This, ref);

    if (!ref)
    {
        if (This->binding) IBinding_Release(This->binding);
        if (This->stream) IStream_Release(This->stream);
        if (This->body) GlobalFree(This->body);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI BindStatusCallback_OnStartBinding(IBindStatusCallback *iface,
        DWORD reserved, IBinding *pbind)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%d %p)\n", This, reserved, pbind);

    if (!pbind) return E_INVALIDARG;

    This->binding = pbind;
    IBinding_AddRef(pbind);

    httprequest_setreadystate(This->request, READYSTATE_LOADED);

    return CreateStreamOnHGlobal(NULL, TRUE, &This->stream);
}

static HRESULT WINAPI BindStatusCallback_GetPriority(IBindStatusCallback *iface, LONG *pPriority)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%p)\n", This, pPriority);

    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnLowResource(IBindStatusCallback *iface, DWORD reserved)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%d)\n", This, reserved);

    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnProgress(IBindStatusCallback *iface, ULONG ulProgress,
        ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%u %u %u %s)\n", This, ulProgress, ulProgressMax, ulStatusCode,
            debugstr_w(szStatusText));

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnStopBinding(IBindStatusCallback *iface,
        HRESULT hr, LPCWSTR error)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(0x%08x %s)\n", This, hr, debugstr_w(error));

    if (This->binding)
    {
        IBinding_Release(This->binding);
        This->binding = NULL;
    }

    if (hr == S_OK)
        httprequest_setreadystate(This->request, READYSTATE_COMPLETE);

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_GetBindInfo(IBindStatusCallback *iface,
        DWORD *bind_flags, BINDINFO *pbindinfo)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%p %p)\n", This, bind_flags, pbindinfo);

    *bind_flags = 0;
    if (This->request->async) *bind_flags |= BINDF_ASYNCHRONOUS;

    if (This->request->verb != BINDVERB_GET && This->body)
    {
        pbindinfo->stgmedData.tymed = TYMED_HGLOBAL;
        pbindinfo->stgmedData.u.hGlobal = This->body;
        pbindinfo->cbstgmedData = GlobalSize(This->body);
        /* callback owns passed body pointer */
        IBindStatusCallback_QueryInterface(iface, &IID_IUnknown, (void**)&pbindinfo->stgmedData.pUnkForRelease);
    }

    pbindinfo->dwBindVerb = This->request->verb;
    if (This->request->verb == BINDVERB_CUSTOM)
    {
        pbindinfo->szCustomVerb = CoTaskMemAlloc(SysStringByteLen(This->request->custom));
        strcpyW(pbindinfo->szCustomVerb, This->request->custom);
    }

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnDataAvailable(IBindStatusCallback *iface,
        DWORD flags, DWORD size, FORMATETC *format, STGMEDIUM *stgmed)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);
    DWORD read, written;
    BYTE buf[4096];
    HRESULT hr;

    TRACE("(%p)->(%08x %d %p %p)\n", This, flags, size, format, stgmed);

    do
    {
        hr = IStream_Read(stgmed->u.pstm, buf, sizeof(buf), &read);
        if (hr != S_OK) break;

        hr = IStream_Write(This->stream, buf, read, &written);
    } while((hr == S_OK) && written != 0 && read != 0);

    httprequest_setreadystate(This->request, READYSTATE_INTERACTIVE);

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnObjectAvailable(IBindStatusCallback *iface,
        REFIID riid, IUnknown *punk)
{
    BindStatusCallback *This = impl_from_IBindStatusCallback(iface);

    FIXME("(%p)->(%s %p): stub\n", This, debugstr_guid(riid), punk);

    return E_NOTIMPL;
}

static const IBindStatusCallbackVtbl BindStatusCallbackVtbl = {
    BindStatusCallback_QueryInterface,
    BindStatusCallback_AddRef,
    BindStatusCallback_Release,
    BindStatusCallback_OnStartBinding,
    BindStatusCallback_GetPriority,
    BindStatusCallback_OnLowResource,
    BindStatusCallback_OnProgress,
    BindStatusCallback_OnStopBinding,
    BindStatusCallback_GetBindInfo,
    BindStatusCallback_OnDataAvailable,
    BindStatusCallback_OnObjectAvailable
};

static HRESULT WINAPI BSCHttpNegotiate_QueryInterface(IHttpNegotiate *iface,
        REFIID riid, void **ppv)
{
    BindStatusCallback *This = impl_from_IHttpNegotiate(iface);
    return IBindStatusCallback_QueryInterface(&This->IBindStatusCallback_iface, riid, ppv);
}

static ULONG WINAPI BSCHttpNegotiate_AddRef(IHttpNegotiate *iface)
{
    BindStatusCallback *This = impl_from_IHttpNegotiate(iface);
    return IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
}

static ULONG WINAPI BSCHttpNegotiate_Release(IHttpNegotiate *iface)
{
    BindStatusCallback *This = impl_from_IHttpNegotiate(iface);
    return IBindStatusCallback_Release(&This->IBindStatusCallback_iface);
}

static HRESULT WINAPI BSCHttpNegotiate_BeginningTransaction(IHttpNegotiate *iface,
        LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR *add_headers)
{
    static const WCHAR content_type_utf8W[] = {'C','o','n','t','e','n','t','-','T','y','p','e',':',' ',
        't','e','x','t','/','p','l','a','i','n',';','c','h','a','r','s','e','t','=','u','t','f','-','8','\r','\n',0};

    BindStatusCallback *This = impl_from_IHttpNegotiate(iface);
    const struct httpheader *entry;
    WCHAR *buff, *ptr;
    int size = 0;

    TRACE("(%p)->(%s %s %d %p)\n", This, debugstr_w(url), debugstr_w(headers), reserved, add_headers);

    *add_headers = NULL;

    if (This->request->use_utf8_content)
        size = sizeof(content_type_utf8W);

    if (!list_empty(&This->request->reqheaders))
        size += This->request->reqheader_size*sizeof(WCHAR);

    if (!size) return S_OK;

    buff = CoTaskMemAlloc(size);
    if (!buff) return E_OUTOFMEMORY;

    ptr = buff;
    if (This->request->use_utf8_content)
    {
        lstrcpyW(ptr, content_type_utf8W);
        ptr += sizeof(content_type_utf8W)/sizeof(WCHAR)-1;
    }

    /* user headers */
    LIST_FOR_EACH_ENTRY(entry, &This->request->reqheaders, struct httpheader, entry)
    {
        lstrcpyW(ptr, entry->header);
        ptr += SysStringLen(entry->header);

        lstrcpyW(ptr, colspaceW);
        ptr += sizeof(colspaceW)/sizeof(WCHAR)-1;

        lstrcpyW(ptr, entry->value);
        ptr += SysStringLen(entry->value);

        lstrcpyW(ptr, crlfW);
        ptr += sizeof(crlfW)/sizeof(WCHAR)-1;
    }

    *add_headers = buff;

    return S_OK;
}

static void add_response_header(httprequest *This, const WCHAR *data, int len)
{
    struct httpheader *entry;
    const WCHAR *ptr = data;
    BSTR header, value;

    while (*ptr)
    {
        if (*ptr == ':')
        {
            header = SysAllocStringLen(data, ptr-data);
            /* skip leading spaces for a value */
            while (*++ptr == ' ')
                ;
            value = SysAllocStringLen(ptr, len-(ptr-data));
            break;
        }
        ptr++;
    }

    if (!*ptr) return;

    /* new header */
    TRACE("got header %s:%s\n", debugstr_w(header), debugstr_w(value));

    entry = heap_alloc(sizeof(*entry));
    entry->header = header;
    entry->value  = value;
    list_add_head(&This->respheaders, &entry->entry);
}

static HRESULT WINAPI BSCHttpNegotiate_OnResponse(IHttpNegotiate *iface, DWORD code,
        LPCWSTR resp_headers, LPCWSTR req_headers, LPWSTR *add_reqheaders)
{
    BindStatusCallback *This = impl_from_IHttpNegotiate(iface);

    TRACE("(%p)->(%d %s %s %p)\n", This, code, debugstr_w(resp_headers),
          debugstr_w(req_headers), add_reqheaders);

    This->request->status = code;
    /* store headers and status text */
    free_response_headers(This->request);
    SysFreeString(This->request->status_text);
    This->request->status_text = NULL;
    if (resp_headers)
    {
        const WCHAR *ptr, *line;

        ptr = line = resp_headers;

        /* skip status line */
        while (*ptr)
        {
            if (*ptr == '\r' && *(ptr+1) == '\n')
            {
                const WCHAR *end = ptr-1;
                line = ptr + 2;
                /* scan back to get status phrase */
                while (ptr > resp_headers)
                {
                     if (*ptr == ' ')
                     {
                         This->request->status_text = SysAllocStringLen(ptr+1, end-ptr);
                         TRACE("status text %s\n", debugstr_w(This->request->status_text));
                         break;
                     }
                     ptr--;
                }
                break;
            }
            ptr++;
        }

        /* store as unparsed string for now */
        This->request->raw_respheaders = SysAllocString(line);
    }

    return S_OK;
}

static const IHttpNegotiateVtbl BSCHttpNegotiateVtbl = {
    BSCHttpNegotiate_QueryInterface,
    BSCHttpNegotiate_AddRef,
    BSCHttpNegotiate_Release,
    BSCHttpNegotiate_BeginningTransaction,
    BSCHttpNegotiate_OnResponse
};

static HRESULT WINAPI Authenticate_QueryInterface(IAuthenticate *iface,
        REFIID riid, void **ppv)
{
    BindStatusCallback *This = impl_from_IAuthenticate(iface);
    return IBindStatusCallback_QueryInterface(&This->IBindStatusCallback_iface, riid, ppv);
}

static ULONG WINAPI Authenticate_AddRef(IAuthenticate *iface)
{
    BindStatusCallback *This = impl_from_IAuthenticate(iface);
    return IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
}

static ULONG WINAPI Authenticate_Release(IAuthenticate *iface)
{
    BindStatusCallback *This = impl_from_IAuthenticate(iface);
    return IBindStatusCallback_Release(&This->IBindStatusCallback_iface);
}

static HRESULT WINAPI Authenticate_Authenticate(IAuthenticate *iface,
    HWND *hwnd, LPWSTR *username, LPWSTR *password)
{
    BindStatusCallback *This = impl_from_IAuthenticate(iface);
    FIXME("(%p)->(%p %p %p)\n", This, hwnd, username, password);
    return E_NOTIMPL;
}

static const IAuthenticateVtbl AuthenticateVtbl = {
    Authenticate_QueryInterface,
    Authenticate_AddRef,
    Authenticate_Release,
    Authenticate_Authenticate
};

static HRESULT BindStatusCallback_create(httprequest* This, BindStatusCallback **obj, const VARIANT *body)
{
    BindStatusCallback *bsc;
    IBindCtx *pbc;
    HRESULT hr;
    int size;

    hr = CreateBindCtx(0, &pbc);
    if (hr != S_OK) return hr;

    bsc = heap_alloc(sizeof(*bsc));
    if (!bsc)
    {
        IBindCtx_Release(pbc);
        return E_OUTOFMEMORY;
    }

    bsc->IBindStatusCallback_iface.lpVtbl = &BindStatusCallbackVtbl;
    bsc->IHttpNegotiate_iface.lpVtbl = &BSCHttpNegotiateVtbl;
    bsc->IAuthenticate_iface.lpVtbl = &AuthenticateVtbl;
    bsc->ref = 1;
    bsc->request = This;
    bsc->binding = NULL;
    bsc->stream = NULL;
    bsc->body = NULL;

    TRACE("(%p)->(%p)\n", This, bsc);

    This->use_utf8_content = FALSE;

    if (This->verb != BINDVERB_GET)
    {
        void *send_data, *ptr;
        SAFEARRAY *sa = NULL;

        if (V_VT(body) == (VT_VARIANT|VT_BYREF))
            body = V_VARIANTREF(body);

        switch (V_VT(body))
        {
        case VT_BSTR:
        {
            int len = SysStringLen(V_BSTR(body));
            const WCHAR *str = V_BSTR(body);
            UINT i, cp = CP_ACP;

            for (i = 0; i < len; i++)
            {
                if (str[i] > 127)
                {
                    cp = CP_UTF8;
                    break;
                }
            }

            size = WideCharToMultiByte(cp, 0, str, len, NULL, 0, NULL, NULL);
            if (!(ptr = heap_alloc(size)))
            {
                heap_free(bsc);
                return E_OUTOFMEMORY;
            }
            WideCharToMultiByte(cp, 0, str, len, ptr, size, NULL, NULL);
            if (cp == CP_UTF8) This->use_utf8_content = TRUE;
            break;
        }
        case VT_ARRAY|VT_UI1:
        {
            sa = V_ARRAY(body);
            if ((hr = SafeArrayAccessData(sa, (void **)&ptr)) != S_OK) return hr;
            if ((hr = SafeArrayGetUBound(sa, 1, &size) != S_OK))
            {
                SafeArrayUnaccessData(sa);
                return hr;
            }
            size++;
            break;
        }
        case VT_EMPTY:
            ptr = NULL;
            size = 0;
            break;
        default:
            FIXME("unsupported body data type %d\n", V_VT(body));
            break;
        }

        bsc->body = GlobalAlloc(GMEM_FIXED, size);
        if (!bsc->body)
        {
            if (V_VT(body) == VT_BSTR)
                heap_free(ptr);
            else if (V_VT(body) == (VT_ARRAY|VT_UI1))
                SafeArrayUnaccessData(sa);

            heap_free(bsc);
            return E_OUTOFMEMORY;
        }

        send_data = GlobalLock(bsc->body);
        memcpy(send_data, ptr, size);
        GlobalUnlock(bsc->body);

        if (V_VT(body) == VT_BSTR)
            heap_free(ptr);
        else if (V_VT(body) == (VT_ARRAY|VT_UI1))
            SafeArrayUnaccessData(sa);
    }

    hr = RegisterBindStatusCallback(pbc, &bsc->IBindStatusCallback_iface, NULL, 0);
    if (hr == S_OK)
    {
        IMoniker *moniker;

        hr = CreateURLMoniker(NULL, This->url, &moniker);
        if (hr == S_OK)
        {
            IStream *stream;

            hr = IMoniker_BindToStorage(moniker, pbc, NULL, &IID_IStream, (void**)&stream);
            IMoniker_Release(moniker);
            if (stream) IStream_Release(stream);
        }
        IBindCtx_Release(pbc);
    }

    if (FAILED(hr))
    {
        IBindStatusCallback_Release(&bsc->IBindStatusCallback_iface);
        bsc = NULL;
    }

    *obj = bsc;
    return hr;
}

static HRESULT WINAPI httprequest_QueryInterface(IXMLHTTPRequest *iface, REFIID riid, void **ppvObject)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if ( IsEqualGUID( riid, &IID_IXMLHTTPRequest) ||
         IsEqualGUID( riid, &IID_IDispatch) ||
         IsEqualGUID( riid, &IID_IUnknown) )
    {
        *ppvObject = iface;
    }
    else if (IsEqualGUID(&IID_IObjectWithSite, riid))
    {
        *ppvObject = &This->IObjectWithSite_iface;
    }
    else if (IsEqualGUID(&IID_IObjectSafety, riid))
    {
        *ppvObject = &This->IObjectSafety_iface;
    }
    else
    {
        TRACE("Unsupported interface %s\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IXMLHTTPRequest_AddRef( iface );

    return S_OK;
}

static ULONG WINAPI httprequest_AddRef(IXMLHTTPRequest *iface)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    ULONG ref = InterlockedIncrement( &This->ref );
    TRACE("(%p)->(%u)\n", This, ref );
    return ref;
}

static ULONG WINAPI httprequest_Release(IXMLHTTPRequest *iface)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE("(%p)->(%u)\n", This, ref );

    if ( ref == 0 )
    {
        struct httpheader *header, *header2;

        if (This->site)
            IUnknown_Release( This->site );

        SysFreeString(This->custom);
        SysFreeString(This->siteurl);
        SysFreeString(This->url);
        SysFreeString(This->user);
        SysFreeString(This->password);

        /* request headers */
        LIST_FOR_EACH_ENTRY_SAFE(header, header2, &This->reqheaders, struct httpheader, entry)
        {
            list_remove(&header->entry);
            SysFreeString(header->header);
            SysFreeString(header->value);
            heap_free(header);
        }
        /* response headers */
        free_response_headers(This);
        SysFreeString(This->status_text);

        /* detach callback object */
        BindStatusCallback_Detach(This->bsc);

        if (This->sink) IDispatch_Release(This->sink);

        heap_free( This );
    }

    return ref;
}

static HRESULT WINAPI httprequest_GetTypeInfoCount(IXMLHTTPRequest *iface, UINT *pctinfo)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;

    return S_OK;
}

static HRESULT WINAPI httprequest_GetTypeInfo(IXMLHTTPRequest *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    return get_typeinfo(IXMLHTTPRequest_tid, ppTInfo);
}

static HRESULT WINAPI httprequest_GetIDsOfNames(IXMLHTTPRequest *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);

    if(!rgszNames || cNames == 0 || !rgDispId)
        return E_INVALIDARG;

    hr = get_typeinfo(IXMLHTTPRequest_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI httprequest_Invoke(IXMLHTTPRequest *iface, DISPID dispIdMember, REFIID riid,
        LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IXMLHTTPRequest_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &This->IXMLHTTPRequest_iface, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI httprequest_open(IXMLHTTPRequest *iface, BSTR method, BSTR url,
        VARIANT async, VARIANT user, VARIANT password)
{
    static const WCHAR MethodGetW[] = {'G','E','T',0};
    static const WCHAR MethodPutW[] = {'P','U','T',0};
    static const WCHAR MethodPostW[] = {'P','O','S','T',0};
    static const WCHAR MethodDeleteW[] = {'D','E','L','E','T','E',0};

    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    VARIANT str, is_async;
    HRESULT hr;

    TRACE("(%p)->(%s %s %s)\n", This, debugstr_w(method), debugstr_w(url),
        debugstr_variant(&async));

    if (!method || !url) return E_INVALIDARG;

    /* free previously set data */
    SysFreeString(This->url);
    SysFreeString(This->user);
    SysFreeString(This->password);
    This->url = This->user = This->password = NULL;

    if (!strcmpiW(method, MethodGetW))
    {
        This->verb = BINDVERB_GET;
    }
    else if (!strcmpiW(method, MethodPutW))
    {
        This->verb = BINDVERB_PUT;
    }
    else if (!strcmpiW(method, MethodPostW))
    {
        This->verb = BINDVERB_POST;
    }
    else if (!strcmpiW(method, MethodDeleteW))
    {
        This->verb = BINDVERB_CUSTOM;
        SysReAllocString(&This->custom, method);
    }
    else
    {
        FIXME("unsupported request type %s\n", debugstr_w(method));
        This->verb = -1;
        return E_FAIL;
    }

    /* try to combine with site url */
    if (This->siteurl && PathIsRelativeW(url))
    {
        DWORD len = INTERNET_MAX_URL_LENGTH;
        WCHAR *fullW = heap_alloc(len*sizeof(WCHAR));

        hr = UrlCombineW(This->siteurl, url, fullW, &len, 0);
        if (hr == S_OK)
        {
            TRACE("combined url %s\n", debugstr_w(fullW));
            This->url = SysAllocString(fullW);
        }
        heap_free(fullW);
    }
    else
        This->url = SysAllocString(url);

    VariantInit(&is_async);
    hr = VariantChangeType(&is_async, &async, 0, VT_BOOL);
    This->async = hr == S_OK && V_BOOL(&is_async) == VARIANT_TRUE;

    VariantInit(&str);
    hr = VariantChangeType(&str, &user, 0, VT_BSTR);
    if (hr == S_OK)
        This->user = V_BSTR(&str);

    VariantInit(&str);
    hr = VariantChangeType(&str, &password, 0, VT_BSTR);
    if (hr == S_OK)
        This->password = V_BSTR(&str);

    httprequest_setreadystate(This, READYSTATE_LOADING);

    return S_OK;
}

static HRESULT WINAPI httprequest_setRequestHeader(IXMLHTTPRequest *iface, BSTR header, BSTR value)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    struct httpheader *entry;

    TRACE("(%p)->(%s %s)\n", This, debugstr_w(header), debugstr_w(value));

    if (!header || !*header) return E_INVALIDARG;
    if (This->state != READYSTATE_LOADING) return E_FAIL;
    if (!value) return E_INVALIDARG;

    /* replace existing header value if already added */
    LIST_FOR_EACH_ENTRY(entry, &This->reqheaders, struct httpheader, entry)
    {
        if (lstrcmpW(entry->header, header) == 0)
        {
            LONG length = SysStringLen(entry->value);
            HRESULT hr;

            hr = SysReAllocString(&entry->value, value) ? S_OK : E_OUTOFMEMORY;

            if (hr == S_OK)
                This->reqheader_size += (SysStringLen(entry->value) - length);

            return hr;
        }
    }

    entry = heap_alloc(sizeof(*entry));
    if (!entry) return E_OUTOFMEMORY;

    /* new header */
    entry->header = SysAllocString(header);
    entry->value  = SysAllocString(value);

    /* header length including null terminator */
    This->reqheader_size += SysStringLen(entry->header) + sizeof(colspaceW)/sizeof(WCHAR) +
                            SysStringLen(entry->value)  + sizeof(crlfW)/sizeof(WCHAR) - 1;

    list_add_head(&This->reqheaders, &entry->entry);

    return S_OK;
}

static HRESULT WINAPI httprequest_getResponseHeader(IXMLHTTPRequest *iface, BSTR header, BSTR *value)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    struct httpheader *entry;

    TRACE("(%p)->(%s %p)\n", This, debugstr_w(header), value);

    if (!header || !value) return E_INVALIDARG;

    if (This->raw_respheaders && list_empty(&This->respheaders))
    {
        WCHAR *ptr, *line;

        ptr = line = This->raw_respheaders;
        while (*ptr)
        {
            if (*ptr == '\r' && *(ptr+1) == '\n')
            {
                add_response_header(This, line, ptr-line);
                ptr++; line = ++ptr;
                continue;
            }
            ptr++;
        }
    }

    LIST_FOR_EACH_ENTRY(entry, &This->respheaders, struct httpheader, entry)
    {
        if (!strcmpiW(entry->header, header))
        {
            *value = SysAllocString(entry->value);
            TRACE("header value %s\n", debugstr_w(*value));
            return S_OK;
        }
    }

    return S_FALSE;
}

static HRESULT WINAPI httprequest_getAllResponseHeaders(IXMLHTTPRequest *iface, BSTR *respheaders)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, respheaders);

    if (!respheaders) return E_INVALIDARG;

    *respheaders = SysAllocString(This->raw_respheaders);

    return S_OK;
}

static HRESULT WINAPI httprequest_send(IXMLHTTPRequest *iface, VARIANT body)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    BindStatusCallback *bsc = NULL;
    HRESULT hr;

    TRACE("(%p)->(%s)\n", This, debugstr_variant(&body));

    if (This->state != READYSTATE_LOADING) return E_FAIL;

    hr = BindStatusCallback_create(This, &bsc, &body);
    if (FAILED(hr)) return hr;

    BindStatusCallback_Detach(This->bsc);
    This->bsc = bsc;

    return hr;
}

static HRESULT WINAPI httprequest_abort(IXMLHTTPRequest *iface)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)\n", This);

    BindStatusCallback_Detach(This->bsc);
    This->bsc = NULL;

    httprequest_setreadystate(This, READYSTATE_UNINITIALIZED);

    return S_OK;
}

static HRESULT WINAPI httprequest_get_status(IXMLHTTPRequest *iface, LONG *status)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, status);

    if (!status) return E_INVALIDARG;
    if (This->state != READYSTATE_COMPLETE) return E_FAIL;

    *status = This->status;

    return S_OK;
}

static HRESULT WINAPI httprequest_get_statusText(IXMLHTTPRequest *iface, BSTR *status)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, status);

    if (!status) return E_INVALIDARG;
    if (This->state != READYSTATE_COMPLETE) return E_FAIL;

    *status = SysAllocString(This->status_text);

    return S_OK;
}

static HRESULT WINAPI httprequest_get_responseXML(IXMLHTTPRequest *iface, IDispatch **body)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    IXMLDOMDocument3 *doc;
    HRESULT hr;
    BSTR str;

    TRACE("(%p)->(%p)\n", This, body);

    if (!body) return E_INVALIDARG;
    if (This->state != READYSTATE_COMPLETE) return E_FAIL;

    hr = DOMDocument_create(MSXML_DEFAULT, NULL, (void**)&doc);
    if (hr != S_OK) return hr;

    hr = IXMLHTTPRequest_get_responseText(iface, &str);
    if (hr == S_OK)
    {
        VARIANT_BOOL ok;

        hr = IXMLDOMDocument3_loadXML(doc, str, &ok);
        SysFreeString(str);
    }

    IXMLDOMDocument3_QueryInterface(doc, &IID_IDispatch, (void**)body);
    IXMLDOMDocument3_Release(doc);

    return hr;
}

static HRESULT WINAPI httprequest_get_responseText(IXMLHTTPRequest *iface, BSTR *body)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    HGLOBAL hglobal;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", This, body);

    if (!body) return E_INVALIDARG;
    if (This->state != READYSTATE_COMPLETE) return E_FAIL;

    hr = GetHGlobalFromStream(This->bsc->stream, &hglobal);
    if (hr == S_OK)
    {
        xmlChar *ptr = GlobalLock(hglobal);
        DWORD size = GlobalSize(hglobal);
        xmlCharEncoding encoding = XML_CHAR_ENCODING_UTF8;

        /* try to determine data encoding */
        if (size >= 4)
        {
            encoding = xmlDetectCharEncoding(ptr, 4);
            TRACE("detected encoding: %s\n", debugstr_a(xmlGetCharEncodingName(encoding)));
            if ( encoding != XML_CHAR_ENCODING_UTF8 &&
                 encoding != XML_CHAR_ENCODING_UTF16LE &&
                 encoding != XML_CHAR_ENCODING_NONE )
            {
                FIXME("unsupported encoding: %s\n", debugstr_a(xmlGetCharEncodingName(encoding)));
                GlobalUnlock(hglobal);
                return E_FAIL;
            }
        }

        /* without BOM assume UTF-8 */
        if (encoding == XML_CHAR_ENCODING_UTF8 ||
            encoding == XML_CHAR_ENCODING_NONE )
        {
            DWORD length = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)ptr, size, NULL, 0);

            *body = SysAllocStringLen(NULL, length);
            if (*body)
                MultiByteToWideChar( CP_UTF8, 0, (LPCSTR)ptr, size, *body, length);
        }
        else
            *body = SysAllocStringByteLen((LPCSTR)ptr, size);

        if (!*body) hr = E_OUTOFMEMORY;
        GlobalUnlock(hglobal);
    }

    return hr;
}

static HRESULT WINAPI httprequest_get_responseBody(IXMLHTTPRequest *iface, VARIANT *body)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    HGLOBAL hglobal;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", This, body);

    if (!body) return E_INVALIDARG;
    V_VT(body) = VT_EMPTY;

    if (This->state != READYSTATE_COMPLETE) return E_PENDING;

    hr = GetHGlobalFromStream(This->bsc->stream, &hglobal);
    if (hr == S_OK)
    {
        void *ptr = GlobalLock(hglobal);
        DWORD size = GlobalSize(hglobal);

        SAFEARRAYBOUND bound;
        SAFEARRAY *array;

        bound.lLbound = 0;
        bound.cElements = size;
        array = SafeArrayCreate(VT_UI1, 1, &bound);

        if (array)
        {
            void *dest;

            V_VT(body) = VT_ARRAY | VT_UI1;
            V_ARRAY(body) = array;

            hr = SafeArrayAccessData(array, &dest);
            if (hr == S_OK)
            {
                memcpy(dest, ptr, size);
                SafeArrayUnaccessData(array);
            }
            else
            {
                VariantClear(body);
            }
        }
        else
            hr = E_FAIL;

        GlobalUnlock(hglobal);
    }

    return hr;
}

static HRESULT WINAPI httprequest_get_responseStream(IXMLHTTPRequest *iface, VARIANT *body)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );
    LARGE_INTEGER move;
    IStream *stream;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", This, body);

    if (!body) return E_INVALIDARG;
    V_VT(body) = VT_EMPTY;

    if (This->state != READYSTATE_COMPLETE) return E_PENDING;

    hr = IStream_Clone(This->bsc->stream, &stream);

    move.QuadPart = 0;
    IStream_Seek(stream, move, STREAM_SEEK_SET, NULL);

    V_VT(body) = VT_UNKNOWN;
    V_UNKNOWN(body) = (IUnknown*)stream;

    return hr;
}

static HRESULT WINAPI httprequest_get_readyState(IXMLHTTPRequest *iface, LONG *state)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, state);

    if (!state) return E_INVALIDARG;

    *state = This->state;
    return S_OK;
}

static HRESULT WINAPI httprequest_put_onreadystatechange(IXMLHTTPRequest *iface, IDispatch *sink)
{
    httprequest *This = impl_from_IXMLHTTPRequest( iface );

    TRACE("(%p)->(%p)\n", This, sink);

    if (This->sink) IDispatch_Release(This->sink);
    if ((This->sink = sink)) IDispatch_AddRef(This->sink);

    return S_OK;
}

static const struct IXMLHTTPRequestVtbl XMLHTTPRequestVtbl =
{
    httprequest_QueryInterface,
    httprequest_AddRef,
    httprequest_Release,
    httprequest_GetTypeInfoCount,
    httprequest_GetTypeInfo,
    httprequest_GetIDsOfNames,
    httprequest_Invoke,
    httprequest_open,
    httprequest_setRequestHeader,
    httprequest_getResponseHeader,
    httprequest_getAllResponseHeaders,
    httprequest_send,
    httprequest_abort,
    httprequest_get_status,
    httprequest_get_statusText,
    httprequest_get_responseXML,
    httprequest_get_responseText,
    httprequest_get_responseBody,
    httprequest_get_responseStream,
    httprequest_get_readyState,
    httprequest_put_onreadystatechange
};

/* IObjectWithSite */
static HRESULT WINAPI
httprequest_ObjectWithSite_QueryInterface( IObjectWithSite* iface, REFIID riid, void** ppvObject )
{
    httprequest *This = impl_from_IObjectWithSite(iface);
    return IXMLHTTPRequest_QueryInterface( (IXMLHTTPRequest *)This, riid, ppvObject );
}

static ULONG WINAPI httprequest_ObjectWithSite_AddRef( IObjectWithSite* iface )
{
    httprequest *This = impl_from_IObjectWithSite(iface);
    return IXMLHTTPRequest_AddRef((IXMLHTTPRequest *)This);
}

static ULONG WINAPI httprequest_ObjectWithSite_Release( IObjectWithSite* iface )
{
    httprequest *This = impl_from_IObjectWithSite(iface);
    return IXMLHTTPRequest_Release((IXMLHTTPRequest *)This);
}

static HRESULT WINAPI httprequest_ObjectWithSite_GetSite( IObjectWithSite *iface, REFIID iid, void **ppvSite )
{
    httprequest *This = impl_from_IObjectWithSite(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid( iid ), ppvSite );

    if ( !This->site )
        return E_FAIL;

    return IUnknown_QueryInterface( This->site, iid, ppvSite );
}

static HRESULT WINAPI httprequest_ObjectWithSite_SetSite( IObjectWithSite *iface, IUnknown *punk )
{
    httprequest *This = impl_from_IObjectWithSite(iface);
    IServiceProvider *provider;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", iface, punk);

    if (punk)
        IUnknown_AddRef( punk );

    if(This->site)
        IUnknown_Release( This->site );

    This->site = punk;

    hr = IUnknown_QueryInterface(This->site, &IID_IServiceProvider, (void**)&provider);
    if (hr == S_OK)
    {
        IHTMLDocument2 *doc;

        hr = IServiceProvider_QueryService(provider, &SID_SContainerDispatch, &IID_IHTMLDocument2, (void**)&doc);
        if (hr == S_OK)
        {
            SysFreeString(This->siteurl);

            hr = IHTMLDocument2_get_URL(doc, &This->siteurl);
            IHTMLDocument2_Release(doc);
            TRACE("host url %s, 0x%08x\n", debugstr_w(This->siteurl), hr);
        }
        IServiceProvider_Release(provider);
    }

    return S_OK;
}

static const IObjectWithSiteVtbl ObjectWithSiteVtbl =
{
    httprequest_ObjectWithSite_QueryInterface,
    httprequest_ObjectWithSite_AddRef,
    httprequest_ObjectWithSite_Release,
    httprequest_ObjectWithSite_SetSite,
    httprequest_ObjectWithSite_GetSite
};

/* IObjectSafety */
static HRESULT WINAPI httprequest_Safety_QueryInterface(IObjectSafety *iface, REFIID riid, void **ppv)
{
    httprequest *This = impl_from_IObjectSafety(iface);
    return IXMLHTTPRequest_QueryInterface( (IXMLHTTPRequest *)This, riid, ppv );
}

static ULONG WINAPI httprequest_Safety_AddRef(IObjectSafety *iface)
{
    httprequest *This = impl_from_IObjectSafety(iface);
    return IXMLHTTPRequest_AddRef((IXMLHTTPRequest *)This);
}

static ULONG WINAPI httprequest_Safety_Release(IObjectSafety *iface)
{
    httprequest *This = impl_from_IObjectSafety(iface);
    return IXMLHTTPRequest_Release((IXMLHTTPRequest *)This);
}

#define SAFETY_SUPPORTED_OPTIONS (INTERFACESAFE_FOR_UNTRUSTED_CALLER|INTERFACESAFE_FOR_UNTRUSTED_DATA|INTERFACE_USES_SECURITY_MANAGER)

static HRESULT WINAPI httprequest_Safety_GetInterfaceSafetyOptions(IObjectSafety *iface, REFIID riid,
        DWORD *supported, DWORD *enabled)
{
    httprequest *This = impl_from_IObjectSafety(iface);

    TRACE("(%p)->(%s %p %p)\n", This, debugstr_guid(riid), supported, enabled);

    if(!supported || !enabled) return E_POINTER;

    *supported = SAFETY_SUPPORTED_OPTIONS;
    *enabled = This->safeopt;

    return S_OK;
}

static HRESULT WINAPI httprequest_Safety_SetInterfaceSafetyOptions(IObjectSafety *iface, REFIID riid,
        DWORD mask, DWORD enabled)
{
    httprequest *This = impl_from_IObjectSafety(iface);
    TRACE("(%p)->(%s %x %x)\n", This, debugstr_guid(riid), mask, enabled);

    if ((mask & ~SAFETY_SUPPORTED_OPTIONS) != 0)
        return E_FAIL;

    This->safeopt = (This->safeopt & ~mask) | (mask & enabled);

    return S_OK;
}

#undef SAFETY_SUPPORTED_OPTIONS

static const IObjectSafetyVtbl ObjectSafetyVtbl = {
    httprequest_Safety_QueryInterface,
    httprequest_Safety_AddRef,
    httprequest_Safety_Release,
    httprequest_Safety_GetInterfaceSafetyOptions,
    httprequest_Safety_SetInterfaceSafetyOptions
};

HRESULT XMLHTTPRequest_create(IUnknown *pUnkOuter, void **ppObj)
{
    httprequest *req;
    HRESULT hr = S_OK;

    TRACE("(%p,%p)\n", pUnkOuter, ppObj);

    req = heap_alloc( sizeof (*req) );
    if( !req )
        return E_OUTOFMEMORY;

    req->IXMLHTTPRequest_iface.lpVtbl = &XMLHTTPRequestVtbl;
    req->IObjectWithSite_iface.lpVtbl = &ObjectWithSiteVtbl;
    req->IObjectSafety_iface.lpVtbl = &ObjectSafetyVtbl;
    req->ref = 1;

    req->async = FALSE;
    req->verb = -1;
    req->custom = NULL;
    req->url = req->siteurl = req->user = req->password = NULL;

    req->state = READYSTATE_UNINITIALIZED;
    req->sink = NULL;

    req->bsc = NULL;
    req->status = 0;
    req->status_text = NULL;
    req->reqheader_size = 0;
    req->raw_respheaders = NULL;
    req->use_utf8_content = FALSE;

    list_init(&req->reqheaders);
    list_init(&req->respheaders);

    req->site = NULL;
    req->safeopt = 0;

    *ppObj = &req->IXMLHTTPRequest_iface;

    TRACE("returning iface %p\n", *ppObj);

    return hr;
}

#else

HRESULT XMLHTTPRequest_create(IUnknown *pUnkOuter, void **ppObj)
{
    MESSAGE("This program tried to use a XMLHTTPRequest object, but\n"
            "libxml2 support was not present at compile time.\n");
    return E_NOTIMPL;
}

#endif
