/*
 * Copyright 2008 Jacek Caban for CodeWeavers
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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

typedef struct {
    HTMLElement element;

    IHTMLScriptElement IHTMLScriptElement_iface;

    nsIDOMHTMLScriptElement *nsscript;
} HTMLScriptElement;

static inline HTMLScriptElement *impl_from_IHTMLScriptElement(IHTMLScriptElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLScriptElement, IHTMLScriptElement_iface);
}

static HRESULT WINAPI HTMLScriptElement_QueryInterface(IHTMLScriptElement *iface,
        REFIID riid, void **ppv)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLScriptElement_AddRef(IHTMLScriptElement *iface)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLScriptElement_Release(IHTMLScriptElement *iface)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLScriptElement_GetTypeInfoCount(IHTMLScriptElement *iface, UINT *pctinfo)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    return IDispatchEx_GetTypeInfoCount(&This->element.node.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLScriptElement_GetTypeInfo(IHTMLScriptElement *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    return IDispatchEx_GetTypeInfo(&This->element.node.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLScriptElement_GetIDsOfNames(IHTMLScriptElement *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    return IDispatchEx_GetIDsOfNames(&This->element.node.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLScriptElement_Invoke(IHTMLScriptElement *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    return IDispatchEx_Invoke(&This->element.node.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLScriptElement_put_src(IHTMLScriptElement *iface, BSTR v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_get_src(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    nsAString src_str;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsAString_Init(&src_str, NULL);
    nsres = nsIDOMHTMLScriptElement_GetSrc(This->nsscript, &src_str);
    return return_nsstr(nsres, &src_str, p);
}

static HRESULT WINAPI HTMLScriptElement_put_htmlFor(IHTMLScriptElement *iface, BSTR v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_get_htmlFor(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_put_event(IHTMLScriptElement *iface, BSTR v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_get_event(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_put_text(IHTMLScriptElement *iface, BSTR v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_get_text(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_put_defer(IHTMLScriptElement *iface, VARIANT_BOOL v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    HRESULT hr = S_OK;
    nsresult nsres;

    TRACE("(%p)->(%x)\n", This, v);

    nsres = nsIDOMHTMLScriptElement_SetDefer(This->nsscript, v != VARIANT_FALSE);
    if(NS_FAILED(nsres))
    {
        hr = E_FAIL;
    }

    return hr;
}

static HRESULT WINAPI HTMLScriptElement_get_defer(IHTMLScriptElement *iface, VARIANT_BOOL *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    PRBool defer = FALSE;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_INVALIDARG;

    nsres = nsIDOMHTMLScriptElement_GetDefer(This->nsscript, &defer);
    if(NS_FAILED(nsres)) {
        ERR("GetSrc failed: %08x\n", nsres);
    }

    *p = defer ? VARIANT_TRUE : VARIANT_FALSE;

    TRACE("*p = %d\n", *p);
    return S_OK;
}

static HRESULT WINAPI HTMLScriptElement_get_readyState(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_put_onerror(IHTMLScriptElement *iface, VARIANT v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(v(%d))\n", This, V_VT(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_get_onerror(IHTMLScriptElement *iface, VARIANT *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScriptElement_put_type(IHTMLScriptElement *iface, BSTR v)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    nsAString nstype_str;
    nsresult nsres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(v));

    nsAString_Init(&nstype_str, v);
    nsres = nsIDOMHTMLScriptElement_SetType(This->nsscript, &nstype_str);
    if (NS_FAILED(nsres))
        ERR("SetType failed: %08x\n", nsres);
    nsAString_Finish (&nstype_str);

    return S_OK;
}

static HRESULT WINAPI HTMLScriptElement_get_type(IHTMLScriptElement *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_IHTMLScriptElement(iface);
    const PRUnichar *nstype;
    nsAString nstype_str;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsAString_Init(&nstype_str, NULL);
    nsres = nsIDOMHTMLScriptElement_GetType(This->nsscript, &nstype_str);
    if(NS_FAILED(nsres))
        ERR("GetType failed: %08x\n", nsres);

    nsAString_GetData(&nstype_str, &nstype);
    *p = *nstype ? SysAllocString(nstype) : NULL;
    nsAString_Finish(&nstype_str);

    return S_OK;
}

static const IHTMLScriptElementVtbl HTMLScriptElementVtbl = {
    HTMLScriptElement_QueryInterface,
    HTMLScriptElement_AddRef,
    HTMLScriptElement_Release,
    HTMLScriptElement_GetTypeInfoCount,
    HTMLScriptElement_GetTypeInfo,
    HTMLScriptElement_GetIDsOfNames,
    HTMLScriptElement_Invoke,
    HTMLScriptElement_put_src,
    HTMLScriptElement_get_src,
    HTMLScriptElement_put_htmlFor,
    HTMLScriptElement_get_htmlFor,
    HTMLScriptElement_put_event,
    HTMLScriptElement_get_event,
    HTMLScriptElement_put_text,
    HTMLScriptElement_get_text,
    HTMLScriptElement_put_defer,
    HTMLScriptElement_get_defer,
    HTMLScriptElement_get_readyState,
    HTMLScriptElement_put_onerror,
    HTMLScriptElement_get_onerror,
    HTMLScriptElement_put_type,
    HTMLScriptElement_get_type
};

static inline HTMLScriptElement *impl_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLScriptElement, element.node);
}

static HRESULT HTMLScriptElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLScriptElement *This = impl_from_HTMLDOMNode(iface);

    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IHTMLScriptElement_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IHTMLScriptElement_iface;
    }else if(IsEqualGUID(&IID_IHTMLScriptElement, riid)) {
        TRACE("(%p)->(IID_IHTMLScriptElement %p)\n", This, ppv);
        *ppv = &This->IHTMLScriptElement_iface;
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    return HTMLElement_QI(&This->element.node, riid, ppv);
}

static void HTMLScriptElement_destructor(HTMLDOMNode *iface)
{
    HTMLScriptElement *This = impl_from_HTMLDOMNode(iface);
    HTMLElement_destructor(&This->element.node);
}

static HRESULT HTMLScriptElement_get_readystate(HTMLDOMNode *iface, BSTR *p)
{
    HTMLScriptElement *This = impl_from_HTMLDOMNode(iface);

    return IHTMLScriptElement_get_readyState(&This->IHTMLScriptElement_iface, p);
}

static const NodeImplVtbl HTMLScriptElementImplVtbl = {
    HTMLScriptElement_QI,
    HTMLScriptElement_destructor,
    HTMLElement_clone,
    HTMLElement_get_attr_col,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    HTMLScriptElement_get_readystate
};

static const tid_t HTMLScriptElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLScriptElement_tid,
    0
};

static dispex_static_data_t HTMLScriptElement_dispex = {
    NULL,
    DispHTMLScriptElement_tid,
    NULL,
    HTMLScriptElement_iface_tids
};

HRESULT HTMLScriptElement_Create(HTMLDocumentNode *doc, nsIDOMHTMLElement *nselem, HTMLElement **elem)
{
    HTMLScriptElement *ret;
    nsresult nsres;

    ret = heap_alloc_zero(sizeof(HTMLScriptElement));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLScriptElement_iface.lpVtbl = &HTMLScriptElementVtbl;
    ret->element.node.vtbl = &HTMLScriptElementImplVtbl;

    nsres = nsIDOMHTMLElement_QueryInterface(nselem, &IID_nsIDOMHTMLScriptElement, (void**)&ret->nsscript);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDOMHTMLScriptElement: %08x\n", nsres);
        heap_free(ret);
        return E_FAIL;
    }

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLScriptElement_dispex);

    *elem = &ret->element;
    return S_OK;
}
