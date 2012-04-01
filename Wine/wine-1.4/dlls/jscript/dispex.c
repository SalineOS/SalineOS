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

#include <assert.h>

#include "jscript.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

/*
 * This IID is used to get jsdisp_t objecto from interface.
 * We might consider using private insteface instead.
 */
static const IID IID_IDispatchJS =
        {0x719c3050,0xf9d3,0x11cf,{0xa4,0x93,0x00,0x40,0x05,0x23,0xa8,0xa6}};

#define FDEX_VERSION_MASK 0xf0000000
#define GOLDEN_RATIO 0x9E3779B9U

typedef enum {
    PROP_VARIANT,
    PROP_BUILTIN,
    PROP_PROTREF,
    PROP_DELETED
} prop_type_t;

struct _dispex_prop_t {
    WCHAR *name;
    unsigned hash;
    prop_type_t type;
    DWORD flags;

    union {
        VARIANT var;
        const builtin_prop_t *p;
        DWORD ref;
    } u;

    int bucket_head;
    int bucket_next;
};

static inline DISPID prop_to_id(jsdisp_t *This, dispex_prop_t *prop)
{
    return prop - This->props;
}

static inline dispex_prop_t *get_prop(jsdisp_t *This, DISPID id)
{
    if(id < 0 || id >= This->prop_cnt || This->props[id].type == PROP_DELETED)
        return NULL;

    return This->props+id;
}

static DWORD get_flags(jsdisp_t *This, dispex_prop_t *prop)
{
    if(prop->type == PROP_PROTREF) {
        dispex_prop_t *parent = get_prop(This->prototype, prop->u.ref);
        if(!parent) {
            prop->type = PROP_DELETED;
            return 0;
        }

        return get_flags(This->prototype, parent);
    }

    return prop->flags;
}

static const builtin_prop_t *find_builtin_prop(jsdisp_t *This, const WCHAR *name)
{
    int min = 0, max, i, r;

    max = This->builtin_info->props_cnt-1;
    while(min <= max) {
        i = (min+max)/2;

        r = strcmpW(name, This->builtin_info->props[i].name);
        if(!r)
            return This->builtin_info->props + i;

        if(r < 0)
            max = i-1;
        else
            min = i+1;
    }

    return NULL;
}

static inline unsigned string_hash(const WCHAR *name)
{
    unsigned h = 0;
    for(; *name; name++)
        h = (h>>(sizeof(unsigned)*8-4)) ^ (h<<4) ^ tolowerW(*name);
    return h;
}

static inline unsigned get_props_idx(jsdisp_t *This, unsigned hash)
{
    return (hash*GOLDEN_RATIO) & (This->buf_size-1);
}

static inline HRESULT resize_props(jsdisp_t *This)
{
    dispex_prop_t *props;
    int i, bucket;

    if(This->buf_size != This->prop_cnt)
        return S_FALSE;

    props = heap_realloc(This->props, sizeof(dispex_prop_t)*This->buf_size*2);
    if(!props)
        return E_OUTOFMEMORY;
    This->buf_size *= 2;
    This->props = props;

    for(i=0; i<This->buf_size; i++) {
        This->props[i].bucket_head = 0;
        This->props[i].bucket_next = 0;
    }

    for(i=1; i<This->prop_cnt; i++) {
        props = This->props+i;

        bucket = get_props_idx(This, props->hash);
        props->bucket_next = This->props[bucket].bucket_head;
        This->props[bucket].bucket_head = i;
    }

    return S_OK;
}

static inline dispex_prop_t* alloc_prop(jsdisp_t *This, const WCHAR *name, prop_type_t type, DWORD flags)
{
    dispex_prop_t *prop;
    unsigned bucket;

    if(FAILED(resize_props(This)))
        return NULL;

    prop = &This->props[This->prop_cnt];
    prop->name = heap_strdupW(name);
    if(!prop->name)
        return NULL;
    prop->type = type;
    prop->flags = flags;
    prop->hash = string_hash(name);

    bucket = get_props_idx(This, prop->hash);
    prop->bucket_next = This->props[bucket].bucket_head;
    This->props[bucket].bucket_head = This->prop_cnt++;
    return prop;
}

static dispex_prop_t *alloc_protref(jsdisp_t *This, const WCHAR *name, DWORD ref)
{
    dispex_prop_t *ret;

    ret = alloc_prop(This, name, PROP_PROTREF, 0);
    if(!ret)
        return NULL;

    ret->u.ref = ref;
    return ret;
}

static HRESULT find_prop_name(jsdisp_t *This, unsigned hash, const WCHAR *name, dispex_prop_t **ret)
{
    const builtin_prop_t *builtin;
    unsigned bucket, pos, prev = 0;
    dispex_prop_t *prop;

    bucket = get_props_idx(This, hash);
    pos = This->props[bucket].bucket_head;
    while(pos != 0) {
        if(!strcmpW(name, This->props[pos].name)) {
            if(prev != 0) {
                This->props[prev].bucket_next = This->props[pos].bucket_next;
                This->props[pos].bucket_next = This->props[bucket].bucket_head;
                This->props[bucket].bucket_head = pos;
            }

            *ret = &This->props[pos];
            return S_OK;
        }

        prev = pos;
        pos = This->props[pos].bucket_next;
    }

    builtin = find_builtin_prop(This, name);
    if(builtin) {
        prop = alloc_prop(This, name, PROP_BUILTIN, builtin->flags);
        if(!prop)
            return E_OUTOFMEMORY;

        prop->u.p = builtin;
        *ret = prop;
        return S_OK;
    }

    *ret = NULL;
    return S_OK;
}

static HRESULT find_prop_name_prot(jsdisp_t *This, unsigned hash, const WCHAR *name, dispex_prop_t **ret)
{
    dispex_prop_t *prop, *del=NULL;
    HRESULT hres;

    hres = find_prop_name(This, hash, name, &prop);
    if(FAILED(hres))
        return hres;
    if(prop && prop->type==PROP_DELETED) {
        del = prop;
    } else if(prop) {
        *ret = prop;
        return S_OK;
    }

    if(This->prototype) {
        hres = find_prop_name_prot(This->prototype, hash, name, &prop);
        if(FAILED(hres))
            return hres;
        if(prop) {
            if(del) {
                del->type = PROP_PROTREF;
                del->flags = 0;
                del->u.ref = prop - This->prototype->props;
                prop = del;
            }else {
                prop = alloc_protref(This, prop->name, prop - This->prototype->props);
                if(!prop)
                    return E_OUTOFMEMORY;
            }

            *ret = prop;
            return S_OK;
        }
    }

    *ret = del;
    return S_OK;
}

static HRESULT ensure_prop_name(jsdisp_t *This, const WCHAR *name, BOOL search_prot, DWORD create_flags, dispex_prop_t **ret)
{
    dispex_prop_t *prop;
    HRESULT hres;

    if(search_prot)
        hres = find_prop_name_prot(This, string_hash(name), name, &prop);
    else
        hres = find_prop_name(This, string_hash(name), name, &prop);
    if(SUCCEEDED(hres) && (!prop || prop->type==PROP_DELETED)) {
        TRACE("creating prop %s\n", debugstr_w(name));

        if(prop) {
            prop->type = PROP_VARIANT;
            prop->flags = create_flags;
        }else {
            prop = alloc_prop(This, name, PROP_VARIANT, create_flags);
            if(!prop)
                return E_OUTOFMEMORY;
        }

        VariantInit(&prop->u.var);
    }

    *ret = prop;
    return hres;
}

static HRESULT set_this(DISPPARAMS *dp, DISPPARAMS *olddp, IDispatch *jsthis)
{
    VARIANTARG *oldargs;
    int i;

    static DISPID this_id = DISPID_THIS;

    *dp = *olddp;

    for(i = 0; i < dp->cNamedArgs; i++) {
        if(dp->rgdispidNamedArgs[i] == DISPID_THIS)
            return S_OK;
    }

    oldargs = dp->rgvarg;
    dp->rgvarg = heap_alloc((dp->cArgs+1) * sizeof(VARIANTARG));
    if(!dp->rgvarg)
        return E_OUTOFMEMORY;
    memcpy(dp->rgvarg+1, oldargs, dp->cArgs*sizeof(VARIANTARG));
    V_VT(dp->rgvarg) = VT_DISPATCH;
    V_DISPATCH(dp->rgvarg) = jsthis;
    dp->cArgs++;

    if(dp->cNamedArgs) {
        DISPID *old = dp->rgdispidNamedArgs;
        dp->rgdispidNamedArgs = heap_alloc((dp->cNamedArgs+1)*sizeof(DISPID));
        if(!dp->rgdispidNamedArgs) {
            heap_free(dp->rgvarg);
            return E_OUTOFMEMORY;
        }

        memcpy(dp->rgdispidNamedArgs+1, old, dp->cNamedArgs*sizeof(DISPID));
        dp->rgdispidNamedArgs[0] = DISPID_THIS;
        dp->cNamedArgs++;
    }else {
        dp->rgdispidNamedArgs = &this_id;
        dp->cNamedArgs = 1;
    }

    return S_OK;
}

static HRESULT invoke_prop_func(jsdisp_t *This, jsdisp_t *jsthis, dispex_prop_t *prop, WORD flags,
        DISPPARAMS *dp, VARIANT *retv, jsexcept_t *ei, IServiceProvider *caller)
{
    HRESULT hres;

    switch(prop->type) {
    case PROP_BUILTIN: {
        vdisp_t vthis;

        if(flags == DISPATCH_CONSTRUCT && (prop->flags & PROPF_METHOD)) {
            WARN("%s is not a constructor\n", debugstr_w(prop->name));
            return E_INVALIDARG;
        }

        set_jsdisp(&vthis, jsthis);
        hres = prop->u.p->invoke(This->ctx, &vthis, flags, dp, retv, ei, caller);
        vdisp_release(&vthis);
        return hres;
    }
    case PROP_PROTREF:
        return invoke_prop_func(This->prototype, jsthis, This->prototype->props+prop->u.ref, flags, dp, retv, ei, caller);
    case PROP_VARIANT: {
        DISPPARAMS new_dp;

        if(V_VT(&prop->u.var) != VT_DISPATCH) {
            FIXME("invoke vt %d\n", V_VT(&prop->u.var));
            return E_FAIL;
        }

        TRACE("call %s %p\n", debugstr_w(prop->name), V_DISPATCH(&prop->u.var));

        hres = set_this(&new_dp, dp, to_disp(jsthis));
        if(FAILED(hres))
            return hres;

        hres = disp_call(This->ctx, V_DISPATCH(&prop->u.var), DISPID_VALUE, flags, &new_dp, retv, ei, caller);

        if(new_dp.rgvarg != dp->rgvarg) {
            heap_free(new_dp.rgvarg);
            if(new_dp.cNamedArgs > 1)
                heap_free(new_dp.rgdispidNamedArgs);
        }

        return hres;
    }
    default:
        ERR("type %d\n", prop->type);
    }

    return E_FAIL;
}

static HRESULT prop_get(jsdisp_t *This, dispex_prop_t *prop, DISPPARAMS *dp,
        VARIANT *retv, jsexcept_t *ei, IServiceProvider *caller)
{
    HRESULT hres;

    switch(prop->type) {
    case PROP_BUILTIN:
        if(prop->u.p->flags & PROPF_METHOD) {
            jsdisp_t *obj;
            hres = create_builtin_function(This->ctx, prop->u.p->invoke, prop->u.p->name, NULL,
                    prop->u.p->flags, NULL, &obj);
            if(FAILED(hres))
                break;

            prop->type = PROP_VARIANT;
            var_set_jsdisp(&prop->u.var, obj);
            hres = VariantCopy(retv, &prop->u.var);
        }else {
            vdisp_t vthis;

            set_jsdisp(&vthis, This);
            hres = prop->u.p->invoke(This->ctx, &vthis, DISPATCH_PROPERTYGET, dp, retv, ei, caller);
            vdisp_release(&vthis);
        }
        break;
    case PROP_PROTREF:
        hres = prop_get(This->prototype, This->prototype->props+prop->u.ref, dp, retv, ei, caller);
        break;
    case PROP_VARIANT:
        hres = VariantCopy(retv, &prop->u.var);
        break;
    default:
        ERR("type %d\n", prop->type);
        return E_FAIL;
    }

    if(FAILED(hres)) {
        TRACE("fail %08x\n", hres);
        return hres;
    }

    TRACE("%s ret %s\n", debugstr_w(prop->name), debugstr_variant(retv));
    return hres;
}

static HRESULT prop_put(jsdisp_t *This, dispex_prop_t *prop, VARIANT *val,
        jsexcept_t *ei, IServiceProvider *caller)
{
    HRESULT hres;

    if(prop->flags & PROPF_CONST)
        return S_OK;

    switch(prop->type) {
    case PROP_BUILTIN:
        if(!(prop->flags & PROPF_METHOD)) {
            DISPPARAMS dp = {val, NULL, 1, 0};
            vdisp_t vthis;

            set_jsdisp(&vthis, This);
            hres = prop->u.p->invoke(This->ctx, &vthis, DISPATCH_PROPERTYPUT, &dp, NULL, ei, caller);
            vdisp_release(&vthis);
            return hres;
        }
    case PROP_PROTREF:
        prop->type = PROP_VARIANT;
        prop->flags = PROPF_ENUM;
        V_VT(&prop->u.var) = VT_EMPTY;
        break;
    case PROP_VARIANT:
        VariantClear(&prop->u.var);
        break;
    default:
        ERR("type %d\n", prop->type);
        return E_FAIL;
    }

    hres = VariantCopy(&prop->u.var, val);
    if(FAILED(hres))
        return hres;

    if(This->builtin_info->on_put)
        This->builtin_info->on_put(This, prop->name);

    TRACE("%s = %s\n", debugstr_w(prop->name), debugstr_variant(val));
    return S_OK;
}

static HRESULT fill_protrefs(jsdisp_t *This)
{
    dispex_prop_t *iter, *prop;
    HRESULT hres;

    if(!This->prototype)
        return S_OK;

    fill_protrefs(This->prototype);

    for(iter = This->prototype->props; iter < This->prototype->props+This->prototype->prop_cnt; iter++) {
        if(!iter->name)
            continue;
        hres = find_prop_name(This, iter->hash, iter->name, &prop);
        if(FAILED(hres))
            return hres;
        if(!prop || prop->type==PROP_DELETED) {
            if(prop) {
                prop->type = PROP_PROTREF;
                prop->flags = 0;
                prop->u.ref = iter - This->prototype->props;
            }else {
                prop = alloc_protref(This, iter->name, iter - This->prototype->props);
                if(!prop)
                    return E_OUTOFMEMORY;
            }
        }
    }

    return S_OK;
}

static inline jsdisp_t *impl_from_IDispatchEx(IDispatchEx *iface)
{
    return CONTAINING_RECORD(iface, jsdisp_t, IDispatchEx_iface);
}

static HRESULT WINAPI DispatchEx_QueryInterface(IDispatchEx *iface, REFIID riid, void **ppv)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchEx, riid)) {
        TRACE("(%p)->(IID_IDispatchEx %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchJS, riid)) {
        TRACE("(%p)->(IID_IDispatchJS %p)\n", This, ppv);
        jsdisp_addref(This);
        *ppv = This;
        return S_OK;
    }else {
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI DispatchEx_AddRef(IDispatchEx *iface)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI DispatchEx_Release(IDispatchEx *iface)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        dispex_prop_t *prop;

        for(prop = This->props; prop < This->props+This->prop_cnt; prop++) {
            if(prop->type == PROP_VARIANT)
                VariantClear(&prop->u.var);
            heap_free(prop->name);
        }
        heap_free(This->props);
        script_release(This->ctx);
        if(This->prototype)
            jsdisp_release(This->prototype);

        if(This->builtin_info->destructor)
            This->builtin_info->destructor(This);
        else
            heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI DispatchEx_GetTypeInfoCount(IDispatchEx *iface, UINT *pctinfo)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetTypeInfo(IDispatchEx *iface, UINT iTInfo, LCID lcid,
                                              ITypeInfo **ppTInfo)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetIDsOfNames(IDispatchEx *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                                                DISPID *rgDispId)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    UINT i;
    HRESULT hres;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);

    for(i=0; i < cNames; i++) {
        hres = IDispatchEx_GetDispID(&This->IDispatchEx_iface, rgszNames[i], 0, rgDispId+i);
        if(FAILED(hres))
            return hres;
    }

    return S_OK;
}

static HRESULT WINAPI DispatchEx_Invoke(IDispatchEx *iface, DISPID dispIdMember,
                                        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    return IDispatchEx_InvokeEx(&This->IDispatchEx_iface, dispIdMember, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, NULL);
}

static HRESULT WINAPI DispatchEx_GetDispID(IDispatchEx *iface, BSTR bstrName, DWORD grfdex, DISPID *pid)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%s %x %p)\n", This, debugstr_w(bstrName), grfdex, pid);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK)) {
        FIXME("Unsupported grfdex %x\n", grfdex);
        return E_NOTIMPL;
    }

    return jsdisp_get_id(This, bstrName, grfdex, pid);
}

static HRESULT WINAPI DispatchEx_InvokeEx(IDispatchEx *iface, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *pvarRes, EXCEPINFO *pei, IServiceProvider *pspCaller)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;
    jsexcept_t jsexcept;
    HRESULT hres;

    TRACE("(%p)->(%x %x %x %p %p %p %p)\n", This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    if(pvarRes)
        V_VT(pvarRes) = VT_EMPTY;

    prop = get_prop(This, id);
    if(!prop || prop->type == PROP_DELETED) {
        TRACE("invalid id\n");
        return DISP_E_MEMBERNOTFOUND;
    }

    memset(&jsexcept, 0, sizeof(jsexcept));

    switch(wFlags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        wFlags = DISPATCH_METHOD;
        /* fall through */
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT:
        hres = invoke_prop_func(This, This, prop, wFlags, pdp, pvarRes, &jsexcept, pspCaller);
        break;
    case DISPATCH_PROPERTYGET:
        hres = prop_get(This, prop, pdp, pvarRes, &jsexcept, pspCaller);
        break;
    case DISPATCH_PROPERTYPUT: {
        DWORD i;

        for(i=0; i < pdp->cNamedArgs; i++) {
            if(pdp->rgdispidNamedArgs[i] == DISPID_PROPERTYPUT)
                break;
        }

        if(i == pdp->cNamedArgs) {
            TRACE("no value to set\n");
            return DISP_E_PARAMNOTOPTIONAL;
        }

        hres = prop_put(This, prop, pdp->rgvarg+i, &jsexcept, pspCaller);
        break;
    }
    default:
        FIXME("Unimplemented flags %x\n", wFlags);
        return E_INVALIDARG;
    }

    if(pei)
        *pei = jsexcept.ei;

    return hres;
}

static HRESULT delete_prop(dispex_prop_t *prop)
{
    if(prop->type == PROP_VARIANT) {
        VariantClear(&prop->u.var);
        prop->type = PROP_DELETED;
    }
    return S_OK;
}

static HRESULT WINAPI DispatchEx_DeleteMemberByName(IDispatchEx *iface, BSTR bstrName, DWORD grfdex)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;
    HRESULT hres;

    TRACE("(%p)->(%s %x)\n", This, debugstr_w(bstrName), grfdex);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK))
        FIXME("Unsupported grfdex %x\n", grfdex);

    hres = find_prop_name(This, string_hash(bstrName), bstrName, &prop);
    if(FAILED(hres))
        return hres;
    if(!prop) {
        TRACE("not found\n");
        return S_OK;
    }

    return delete_prop(prop);
}

static HRESULT WINAPI DispatchEx_DeleteMemberByDispID(IDispatchEx *iface, DISPID id)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;

    TRACE("(%p)->(%x)\n", This, id);

    prop = get_prop(This, id);
    if(!prop) {
        WARN("invalid id\n");
        return DISP_E_MEMBERNOTFOUND;
    }

    return delete_prop(prop);
}

static HRESULT WINAPI DispatchEx_GetMemberProperties(IDispatchEx *iface, DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %x %p)\n", This, id, grfdexFetch, pgrfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberName(IDispatchEx *iface, DISPID id, BSTR *pbstrName)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;

    TRACE("(%p)->(%x %p)\n", This, id, pbstrName);

    prop = get_prop(This, id);
    if(!prop || !prop->name || prop->type == PROP_DELETED)
        return DISP_E_MEMBERNOTFOUND;

    *pbstrName = SysAllocString(prop->name);
    if(!*pbstrName)
        return E_OUTOFMEMORY;

    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetNextDispID(IDispatchEx *iface, DWORD grfdex, DISPID id, DISPID *pid)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *iter;
    HRESULT hres;

    TRACE("(%p)->(%x %x %p)\n", This, grfdex, id, pid);

    if(id == DISPID_STARTENUM) {
        hres = fill_protrefs(This);
        if(FAILED(hres))
            return hres;
    }

    if(id+1>=0 && id+1<This->prop_cnt) {
        iter = &This->props[id+1];
    }else {
        *pid = DISPID_STARTENUM;
        return S_FALSE;
    }

    while(iter < This->props + This->prop_cnt) {
        if(iter->name && (get_flags(This, iter) & PROPF_ENUM) && iter->type!=PROP_DELETED) {
            *pid = prop_to_id(This, iter);
            return S_OK;
        }
        iter++;
    }

    *pid = DISPID_STARTENUM;
    return S_FALSE;
}

static HRESULT WINAPI DispatchEx_GetNameSpaceParent(IDispatchEx *iface, IUnknown **ppunk)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%p)\n", This, ppunk);
    return E_NOTIMPL;
}

static IDispatchExVtbl DispatchExVtbl = {
    DispatchEx_QueryInterface,
    DispatchEx_AddRef,
    DispatchEx_Release,
    DispatchEx_GetTypeInfoCount,
    DispatchEx_GetTypeInfo,
    DispatchEx_GetIDsOfNames,
    DispatchEx_Invoke,
    DispatchEx_GetDispID,
    DispatchEx_InvokeEx,
    DispatchEx_DeleteMemberByName,
    DispatchEx_DeleteMemberByDispID,
    DispatchEx_GetMemberProperties,
    DispatchEx_GetMemberName,
    DispatchEx_GetNextDispID,
    DispatchEx_GetNameSpaceParent
};

jsdisp_t *as_jsdisp(IDispatch *disp)
{
    assert(disp->lpVtbl == (IDispatchVtbl*)&DispatchExVtbl);
    return impl_from_IDispatchEx((IDispatchEx*)disp);
}

jsdisp_t *to_jsdisp(IDispatch *disp)
{
    return disp->lpVtbl == (IDispatchVtbl*)&DispatchExVtbl ? impl_from_IDispatchEx((IDispatchEx*)disp) : NULL;
}

HRESULT init_dispex(jsdisp_t *dispex, script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *prototype)
{
    TRACE("%p (%p)\n", dispex, prototype);

    dispex->IDispatchEx_iface.lpVtbl = &DispatchExVtbl;
    dispex->ref = 1;
    dispex->builtin_info = builtin_info;

    dispex->props = heap_alloc_zero(sizeof(dispex_prop_t)*(dispex->buf_size=4));
    if(!dispex->props)
        return E_OUTOFMEMORY;

    dispex->prototype = prototype;
    if(prototype)
        jsdisp_addref(prototype);

    dispex->prop_cnt = 1;
    if(builtin_info->value_prop.invoke) {
        dispex->props[0].type = PROP_BUILTIN;
        dispex->props[0].u.p = &builtin_info->value_prop;
    }else {
        dispex->props[0].type = PROP_DELETED;
    }

    script_addref(ctx);
    dispex->ctx = ctx;

    return S_OK;
}

static const builtin_info_t dispex_info = {
    JSCLASS_NONE,
    {NULL, NULL, 0},
    0, NULL,
    NULL,
    NULL
};

HRESULT create_dispex(script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *prototype, jsdisp_t **dispex)
{
    jsdisp_t *ret;
    HRESULT hres;

    ret = heap_alloc_zero(sizeof(jsdisp_t));
    if(!ret)
        return E_OUTOFMEMORY;

    hres = init_dispex(ret, ctx, builtin_info ? builtin_info : &dispex_info, prototype);
    if(FAILED(hres))
        return hres;

    *dispex = ret;
    return S_OK;
}

HRESULT init_dispex_from_constr(jsdisp_t *dispex, script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *constr)
{
    jsdisp_t *prot = NULL;
    dispex_prop_t *prop;
    HRESULT hres;

    static const WCHAR constructorW[] = {'c','o','n','s','t','r','u','c','t','o','r',0};
    static const WCHAR prototypeW[] = {'p','r','o','t','o','t','y','p','e',0};

    hres = find_prop_name_prot(constr, string_hash(prototypeW), prototypeW, &prop);
    if(SUCCEEDED(hres) && prop && prop->type!=PROP_DELETED) {
        jsexcept_t jsexcept;
        VARIANT var;

        V_VT(&var) = VT_EMPTY;
        memset(&jsexcept, 0, sizeof(jsexcept));
        hres = prop_get(constr, prop, NULL, &var, &jsexcept, NULL/*FIXME*/);
        if(FAILED(hres)) {
            ERR("Could not get prototype\n");
            return hres;
        }

        if(V_VT(&var) == VT_DISPATCH)
            prot = iface_to_jsdisp((IUnknown*)V_DISPATCH(&var));
        VariantClear(&var);
    }

    hres = init_dispex(dispex, ctx, builtin_info, prot);

    if(prot)
        jsdisp_release(prot);
    if(FAILED(hres))
        return hres;

    hres = ensure_prop_name(dispex, constructorW, FALSE, 0, &prop);
    if(SUCCEEDED(hres)) {
        jsexcept_t jsexcept;
        VARIANT var;

        var_set_jsdisp(&var, constr);
        memset(&jsexcept, 0, sizeof(jsexcept));
        hres = prop_put(dispex, prop, &var, &jsexcept, NULL/*FIXME*/);
    }
    if(FAILED(hres))
        jsdisp_release(dispex);

    return hres;
}

jsdisp_t *iface_to_jsdisp(IUnknown *iface)
{
    jsdisp_t *ret;
    HRESULT hres;

    hres = IUnknown_QueryInterface(iface, &IID_IDispatchJS, (void**)&ret);
    if(FAILED(hres))
        return NULL;

    return ret;
}

HRESULT jsdisp_get_id(jsdisp_t *jsdisp, const WCHAR *name, DWORD flags, DISPID *id)
{
    dispex_prop_t *prop;
    HRESULT hres;

    if(flags & fdexNameEnsure)
        hres = ensure_prop_name(jsdisp, name, TRUE, PROPF_ENUM, &prop);
    else
        hres = find_prop_name_prot(jsdisp, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(prop && prop->type!=PROP_DELETED) {
        *id = prop_to_id(jsdisp, prop);
        return S_OK;
    }

    TRACE("not found %s\n", debugstr_w(name));
    return DISP_E_UNKNOWNNAME;
}

HRESULT jsdisp_call_value(jsdisp_t *jsthis, WORD flags, DISPPARAMS *dp, VARIANT *retv,
        jsexcept_t *ei, IServiceProvider *caller)
{
    vdisp_t vdisp;
    HRESULT hres;

    set_jsdisp(&vdisp, jsthis);
    hres = jsthis->builtin_info->value_prop.invoke(jsthis->ctx, &vdisp, flags, dp, retv, ei, caller);
    vdisp_release(&vdisp);
    return hres;
}

HRESULT jsdisp_call(jsdisp_t *disp, DISPID id, WORD flags, DISPPARAMS *dp, VARIANT *retv,
        jsexcept_t *ei, IServiceProvider *caller)
{
    dispex_prop_t *prop;

    memset(ei, 0, sizeof(*ei));
    if(retv)
        V_VT(retv) = VT_EMPTY;

    prop = get_prop(disp, id);
    if(!prop)
        return DISP_E_MEMBERNOTFOUND;

    return invoke_prop_func(disp, disp, prop, flags, dp, retv, ei, caller);
}

HRESULT jsdisp_call_name(jsdisp_t *disp, const WCHAR *name, WORD flags, DISPPARAMS *dp, VARIANT *retv,
        jsexcept_t *ei, IServiceProvider *caller)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(disp, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    memset(ei, 0, sizeof(*ei));
    if(retv)
        V_VT(retv) = VT_EMPTY;

    return invoke_prop_func(disp, disp, prop, flags, dp, retv, ei, caller);
}

HRESULT disp_call(script_ctx_t *ctx, IDispatch *disp, DISPID id, WORD flags, DISPPARAMS *dp, VARIANT *retv,
        jsexcept_t *ei, IServiceProvider *caller)
{
    jsdisp_t *jsdisp;
    IDispatchEx *dispex;
    HRESULT hres;

    jsdisp = iface_to_jsdisp((IUnknown*)disp);
    if(jsdisp) {
        hres = jsdisp_call(jsdisp, id, flags, dp, retv, ei, caller);
        jsdisp_release(jsdisp);
        return hres;
    }

    memset(ei, 0, sizeof(*ei));
    if(retv && arg_cnt(dp))
        flags |= DISPATCH_PROPERTYGET;

    if(retv)
        V_VT(retv) = VT_EMPTY;
    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(FAILED(hres)) {
        UINT err = 0;

        if(flags == DISPATCH_CONSTRUCT) {
            WARN("IDispatch cannot be constructor\n");
            return DISP_E_MEMBERNOTFOUND;
        }

        TRACE("using IDispatch\n");
        return IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, flags, dp, retv, &ei->ei, &err);
    }

    hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, flags, dp, retv, &ei->ei, caller);
    IDispatchEx_Release(dispex);

    return hres;
}

HRESULT jsdisp_propput_name(jsdisp_t *obj, const WCHAR *name, VARIANT *val, jsexcept_t *ei, IServiceProvider *caller)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = ensure_prop_name(obj, name, FALSE, PROPF_ENUM, &prop);
    if(FAILED(hres))
        return hres;

    return prop_put(obj, prop, val, ei, caller);
}

HRESULT jsdisp_propput_const(jsdisp_t *obj, const WCHAR *name, VARIANT *val)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = ensure_prop_name(obj, name, FALSE, PROPF_ENUM|PROPF_CONST, &prop);
    if(FAILED(hres))
        return hres;

    return VariantCopy(&prop->u.var, val);
}

HRESULT jsdisp_propput_idx(jsdisp_t *obj, DWORD idx, VARIANT *val, jsexcept_t *ei, IServiceProvider *caller)
{
    WCHAR buf[12];

    static const WCHAR formatW[] = {'%','d',0};

    sprintfW(buf, formatW, idx);
    return jsdisp_propput_name(obj, buf, val, ei, caller);
}

HRESULT disp_propput(script_ctx_t *ctx, IDispatch *disp, DISPID id, VARIANT *val, jsexcept_t *ei, IServiceProvider *caller)
{
    jsdisp_t *jsdisp;
    HRESULT hres;

    jsdisp = iface_to_jsdisp((IUnknown*)disp);
    if(jsdisp) {
        dispex_prop_t *prop;

        prop = get_prop(jsdisp, id);
        if(prop)
            hres = prop_put(jsdisp, prop, val, ei, caller);
        else
            hres = DISP_E_MEMBERNOTFOUND;

        jsdisp_release(jsdisp);
    }else {
        DISPID dispid = DISPID_PROPERTYPUT;
        DISPPARAMS dp  = {val, &dispid, 1, 1};
        IDispatchEx *dispex;

        hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
        if(SUCCEEDED(hres)) {
            hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, DISPATCH_PROPERTYPUT, &dp, NULL, &ei->ei, caller);
            IDispatchEx_Release(dispex);
        }else {
            ULONG err = 0;

            TRACE("using IDispatch\n");
            hres = IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, DISPATCH_PROPERTYPUT, &dp, NULL, &ei->ei, &err);
        }
    }

    return hres;
}

HRESULT jsdisp_propget_name(jsdisp_t *obj, const WCHAR *name, VARIANT *var, jsexcept_t *ei, IServiceProvider *caller)
{
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    V_VT(var) = VT_EMPTY;
    if(!prop || prop->type==PROP_DELETED)
        return S_OK;

    return prop_get(obj, prop, &dp, var, ei, caller);
}

HRESULT jsdisp_get_idx(jsdisp_t *obj, DWORD idx, VARIANT *var, jsexcept_t *ei, IServiceProvider *caller)
{
    WCHAR name[12];
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    dispex_prop_t *prop;
    HRESULT hres;

    static const WCHAR formatW[] = {'%','d',0};

    sprintfW(name, formatW, idx);

    hres = find_prop_name_prot(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    V_VT(var) = VT_EMPTY;
    if(!prop || prop->type==PROP_DELETED)
        return DISP_E_UNKNOWNNAME;

    return prop_get(obj, prop, &dp, var, ei, caller);
}

HRESULT jsdisp_propget(jsdisp_t *jsdisp, DISPID id, VARIANT *val, jsexcept_t *ei, IServiceProvider *caller)
{
    DISPPARAMS dp  = {NULL,NULL,0,0};
    dispex_prop_t *prop;

    prop = get_prop(jsdisp, id);
    if(!prop)
        return DISP_E_MEMBERNOTFOUND;

    V_VT(val) = VT_EMPTY;
    return prop_get(jsdisp, prop, &dp, val, ei, caller);
}

HRESULT disp_propget(script_ctx_t *ctx, IDispatch *disp, DISPID id, VARIANT *val, jsexcept_t *ei, IServiceProvider *caller)
{
    DISPPARAMS dp  = {NULL,NULL,0,0};
    IDispatchEx *dispex;
    jsdisp_t *jsdisp;
    HRESULT hres;

    jsdisp = iface_to_jsdisp((IUnknown*)disp);
    if(jsdisp) {
        hres = jsdisp_propget(jsdisp, id, val, ei, caller);
        jsdisp_release(jsdisp);
        return hres;
    }

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(FAILED(hres)) {
        ULONG err = 0;

        TRACE("using IDispatch\n");
        return IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, INVOKE_PROPERTYGET, &dp, val, &ei->ei, &err);
    }

    hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, INVOKE_PROPERTYGET, &dp, val, &ei->ei, caller);
    IDispatchEx_Release(dispex);

    return hres;
}

HRESULT jsdisp_delete_idx(jsdisp_t *obj, DWORD idx)
{
    static const WCHAR formatW[] = {'%','d',0};
    WCHAR buf[12];
    dispex_prop_t *prop;
    HRESULT hres;

    sprintfW(buf, formatW, idx);

    hres = find_prop_name(obj, string_hash(buf), buf, &prop);
    if(FAILED(hres) || !prop)
        return hres;

    return delete_prop(prop);
}

VARIANT_BOOL jsdisp_is_own_prop(jsdisp_t *obj, BSTR name)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return VARIANT_FALSE;
    else if(!prop)
        return VARIANT_FALSE;

    return prop->type==PROP_VARIANT ? VARIANT_TRUE : VARIANT_FALSE;
}
