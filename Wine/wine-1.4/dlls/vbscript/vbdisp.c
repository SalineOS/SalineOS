/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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

#include "vbscript.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vbscript);

static inline BOOL is_func_id(vbdisp_t *This, DISPID id)
{
    return id < This->desc->func_cnt;
}

static BOOL get_func_id(vbdisp_t *This, const WCHAR *name, vbdisp_invoke_type_t invoke_type, BOOL search_private, DISPID *id)
{
    unsigned i;

    for(i = invoke_type == VBDISP_ANY ? 0 : 1; i < This->desc->func_cnt; i++) {
        if(invoke_type == VBDISP_ANY) {
            if(!search_private && !This->desc->funcs[i].is_public)
                continue;
            if(!i && !This->desc->funcs[0].name) /* default value may not exist */
                continue;
        }else {
            if(!This->desc->funcs[i].entries[invoke_type]
                || (!search_private && !This->desc->funcs[i].entries[invoke_type]->is_public))
                continue;
        }

        if(!strcmpiW(This->desc->funcs[i].name, name)) {
            *id = i;
            return TRUE;
        }
    }

    return FALSE;
}

HRESULT vbdisp_get_id(vbdisp_t *This, BSTR name, vbdisp_invoke_type_t invoke_type, BOOL search_private, DISPID *id)
{
    unsigned i;

    if(get_func_id(This, name, invoke_type, search_private, id))
        return S_OK;

    for(i=0; i < This->desc->prop_cnt; i++) {
        if(!search_private && !This->desc->props[i].is_public)
            continue;

        if(!strcmpiW(This->desc->props[i].name, name)) {
            *id = i + This->desc->func_cnt;
            return S_OK;
        }
    }

    if(This->desc->typeinfo) {
        HRESULT hres;

        hres = ITypeInfo_GetIDsOfNames(This->desc->typeinfo, &name, 1, id);
        if(SUCCEEDED(hres))
            return S_OK;
    }

    *id = -1;
    return DISP_E_UNKNOWNNAME;
}

static VARIANT *get_propput_arg(const DISPPARAMS *dp)
{
    unsigned i;

    for(i=0; i < dp->cNamedArgs; i++) {
        if(dp->rgdispidNamedArgs[i] == DISPID_PROPERTYPUT)
            return dp->rgvarg+i;
    }

    return NULL;
}

static HRESULT invoke_variant_prop(vbdisp_t *This, VARIANT *v, WORD flags, DISPPARAMS *dp, VARIANT *res)
{
    HRESULT hres;

    switch(flags) {
    case DISPATCH_PROPERTYGET|DISPATCH_METHOD:
        if(dp->cArgs) {
            WARN("called with arguments\n");
            return DISP_E_MEMBERNOTFOUND; /* That's what tests show */
        }

        hres = VariantCopy(res, v);
        break;

    case DISPATCH_PROPERTYPUT: {
        VARIANT *put_val;

        put_val = get_propput_arg(dp);
        if(!put_val) {
            WARN("no value to set\n");
            return DISP_E_PARAMNOTOPTIONAL;
        }

        if(res)
            V_VT(res) = VT_EMPTY;

        hres = VariantCopy(v, put_val);
        break;
    }

    default:
        FIXME("unimplemented flags %x\n", flags);
        return E_NOTIMPL;
    }

    return hres;
}

static HRESULT invoke_builtin(vbdisp_t *This, const builtin_prop_t *prop, WORD flags, DISPPARAMS *dp, VARIANT *res)
{
    VARIANT *args, arg_buf[8];
    unsigned argn;

    switch(flags) {
    case DISPATCH_PROPERTYGET:
        if(!(prop->flags & (BP_GET|BP_GETPUT))) {
            FIXME("property does not support DISPATCH_PROPERTYGET\n");
            return E_FAIL;
        }
        break;
    case DISPATCH_PROPERTYGET|DISPATCH_METHOD:
        break;
    case DISPATCH_METHOD:
        if(prop->flags & (BP_GET|BP_GETPUT)) {
            FIXME("Call on property\n");
            return E_FAIL;
        }
        break;
    case DISPATCH_PROPERTYPUT:
        if(!(prop->flags & (BP_GET|BP_GETPUT))) {
            FIXME("property does not support DISPATCH_PROPERTYPUT\n");
            return E_FAIL;
        }

        FIXME("call put\n");
        return E_NOTIMPL;
    default:
        FIXME("unsupported flags %x\n", flags);
        return E_NOTIMPL;
    }

    argn = arg_cnt(dp);

    if(argn < prop->min_args || argn > (prop->max_args ? prop->max_args : prop->min_args)) {
        FIXME("invalid number of arguments\n");
        return E_FAIL;
    }

    args = dp->rgvarg;

    if(argn == 1) {
        if(V_VT(dp->rgvarg) == (VT_BYREF|VT_VARIANT))
            args = V_VARIANTREF(dp->rgvarg);
    }else {
        unsigned i;

        assert(argn < sizeof(arg_buf)/sizeof(*arg_buf));

        for(i=0; i < argn; i++) {
            if(V_VT(dp->rgvarg+i) == (VT_BYREF|VT_VARIANT)) {
                for(i=0; i < argn; i++) {
                    if(V_VT(dp->rgvarg+i) == (VT_BYREF|VT_VARIANT))
                        arg_buf[i] = *V_VARIANTREF(dp->rgvarg+i);
                    else
                        arg_buf[i] = dp->rgvarg[i];
                }
                args = arg_buf;
                break;
            }
        }
    }

    return prop->proc(This, args, dp->cArgs, res);
}

static BOOL run_terminator(vbdisp_t *This)
{
    DISPPARAMS dp = {0};

    if(This->terminator_ran)
        return TRUE;
    This->terminator_ran = TRUE;

    if(!This->desc->class_terminate_id)
        return TRUE;

    This->ref++;
    exec_script(This->desc->ctx, This->desc->funcs[This->desc->class_terminate_id].entries[VBDISP_CALLGET],
            (IDispatch*)&This->IDispatchEx_iface, &dp, NULL);
    return !--This->ref;
}

static void clean_props(vbdisp_t *This)
{
    unsigned i;

    if(!This->desc)
        return;

    for(i=0; i < This->desc->prop_cnt; i++)
        VariantClear(This->props+i);
}

static inline vbdisp_t *impl_from_IDispatchEx(IDispatchEx *iface)
{
    return CONTAINING_RECORD(iface, vbdisp_t, IDispatchEx_iface);
}

static HRESULT WINAPI DispatchEx_QueryInterface(IDispatchEx *iface, REFIID riid, void **ppv)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchEx, riid)) {
        TRACE("(%p)->(IID_IDispatchEx %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
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
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI DispatchEx_Release(IDispatchEx *iface)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref && run_terminator(This)) {
        clean_props(This);
        list_remove(&This->entry);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI DispatchEx_GetTypeInfoCount(IDispatchEx *iface, UINT *pctinfo)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetTypeInfo(IDispatchEx *iface, UINT iTInfo, LCID lcid,
                                              ITypeInfo **ppTInfo)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetIDsOfNames(IDispatchEx *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                                                DISPID *rgDispId)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_Invoke(IDispatchEx *iface, DISPID dispIdMember,
                                        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetDispID(IDispatchEx *iface, BSTR bstrName, DWORD grfdex, DISPID *pid)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%s %x %p)\n", This, debugstr_w(bstrName), grfdex, pid);

    if(!This->desc)
        return E_UNEXPECTED;

    if(grfdex & ~(fdexNameEnsure|fdexNameCaseInsensitive)) {
        FIXME("unsupported flags %x\n", grfdex);
        return E_NOTIMPL;
    }

    return vbdisp_get_id(This, bstrName, VBDISP_ANY, FALSE, pid);
}

static HRESULT WINAPI DispatchEx_InvokeEx(IDispatchEx *iface, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *pvarRes, EXCEPINFO *pei, IServiceProvider *pspCaller)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%x %x %x %p %p %p %p)\n", This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    if(!This->desc)
        return E_UNEXPECTED;

    if(pvarRes)
        V_VT(pvarRes) = VT_EMPTY;

    if(id < 0)
        return DISP_E_MEMBERNOTFOUND;

    if(is_func_id(This, id)) {
        function_t *func;

        switch(wFlags) {
        case DISPATCH_METHOD:
        case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
            func = This->desc->funcs[id].entries[VBDISP_CALLGET];
            if(!func) {
                FIXME("no invoke/getter\n");
                return DISP_E_MEMBERNOTFOUND;
            }

            return exec_script(This->desc->ctx, func, (IDispatch*)&This->IDispatchEx_iface, pdp, pvarRes);
        case DISPATCH_PROPERTYPUT: {
            VARIANT *put_val;
            DISPPARAMS dp = {NULL, NULL, 1, 0};

            if(arg_cnt(pdp)) {
                FIXME("arguments not implemented\n");
                return E_NOTIMPL;
            }

            put_val = get_propput_arg(pdp);
            if(!put_val) {
                WARN("no value to set\n");
                return DISP_E_PARAMNOTOPTIONAL;
            }

            dp.rgvarg = put_val;
            func = This->desc->funcs[id].entries[V_VT(put_val) == VT_DISPATCH ? VBDISP_SET : VBDISP_LET];
            if(!func) {
                FIXME("no letter/setter\n");
                return DISP_E_MEMBERNOTFOUND;
            }

            return exec_script(This->desc->ctx, func, (IDispatch*)&This->IDispatchEx_iface, &dp, NULL);
        }
        default:
            FIXME("flags %x\n", wFlags);
            return DISP_E_MEMBERNOTFOUND;
        }
    }

    if(id < This->desc->prop_cnt + This->desc->func_cnt)
        return invoke_variant_prop(This, This->props+(id-This->desc->func_cnt), wFlags, pdp, pvarRes);

    if(This->desc->builtin_prop_cnt) {
        unsigned min = 0, max = This->desc->builtin_prop_cnt-1, i;

        while(min <= max) {
            i = (min+max)/2;
            if(This->desc->builtin_props[i].id == id)
                return invoke_builtin(This, This->desc->builtin_props+i, wFlags, pdp, pvarRes);
            if(This->desc->builtin_props[i].id < id)
                min = i+1;
            else
                max = i-1;
        }
    }

    return DISP_E_MEMBERNOTFOUND;
}

static HRESULT WINAPI DispatchEx_DeleteMemberByName(IDispatchEx *iface, BSTR bstrName, DWORD grfdex)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%s %x)\n", This, debugstr_w(bstrName), grfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_DeleteMemberByDispID(IDispatchEx *iface, DISPID id)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x)\n", This, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberProperties(IDispatchEx *iface, DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %x %p)\n", This, id, grfdexFetch, pgrfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberName(IDispatchEx *iface, DISPID id, BSTR *pbstrName)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %p)\n", This, id, pbstrName);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetNextDispID(IDispatchEx *iface, DWORD grfdex, DISPID id, DISPID *pid)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %x %p)\n", This, grfdex, id, pid);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetNameSpaceParent(IDispatchEx *iface, IUnknown **ppunk)
{
    vbdisp_t *This = impl_from_IDispatchEx(iface);
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

static inline vbdisp_t *unsafe_impl_from_IDispatch(IDispatch *iface)
{
    return iface->lpVtbl == (IDispatchVtbl*)&DispatchExVtbl
        ? CONTAINING_RECORD(iface, vbdisp_t, IDispatchEx_iface)
        : NULL;
}

HRESULT create_vbdisp(const class_desc_t *desc, vbdisp_t **ret)
{
    vbdisp_t *vbdisp;

    vbdisp = heap_alloc_zero( FIELD_OFFSET( vbdisp_t, props[desc->prop_cnt] ));
    if(!vbdisp)
        return E_OUTOFMEMORY;

    vbdisp->IDispatchEx_iface.lpVtbl = &DispatchExVtbl;
    vbdisp->ref = 1;
    vbdisp->desc = desc;

    if(desc->class_initialize_id) {
        DISPPARAMS dp = {0};
        HRESULT hres;

        hres = exec_script(desc->ctx, desc->funcs[desc->class_initialize_id].entries[VBDISP_CALLGET],
                           (IDispatch*)&vbdisp->IDispatchEx_iface, &dp, NULL);
        if(FAILED(hres)) {
            IDispatchEx_Release(&vbdisp->IDispatchEx_iface);
            return hres;
        }
    }

    list_add_tail(&desc->ctx->objects, &vbdisp->entry);
    *ret = vbdisp;
    return S_OK;
}

void collect_objects(script_ctx_t *ctx)
{
    vbdisp_t *iter, *iter2;

    LIST_FOR_EACH_ENTRY_SAFE(iter, iter2, &ctx->objects, vbdisp_t, entry)
        run_terminator(iter);

    while(!list_empty(&ctx->objects)) {
        iter = LIST_ENTRY(list_head(&ctx->objects), vbdisp_t, entry);

        IDispatchEx_AddRef(&iter->IDispatchEx_iface);
        clean_props(iter);
        iter->desc = NULL;
        list_remove(&iter->entry);
        list_init(&iter->entry);
        IDispatchEx_Release(&iter->IDispatchEx_iface);
    }
}

HRESULT disp_get_id(IDispatch *disp, BSTR name, vbdisp_invoke_type_t invoke_type, BOOL search_private, DISPID *id)
{
    IDispatchEx *dispex;
    vbdisp_t *vbdisp;
    HRESULT hres;

    vbdisp = unsafe_impl_from_IDispatch(disp);
    if(vbdisp)
        return vbdisp_get_id(vbdisp, name, invoke_type, search_private, id);

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(FAILED(hres)) {
        TRACE("unsing IDispatch\n");
        return IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, 0, id);
    }

    hres = IDispatchEx_GetDispID(dispex, name, fdexNameCaseInsensitive, id);
    IDispatchEx_Release(dispex);
    return hres;
}

HRESULT disp_call(script_ctx_t *ctx, IDispatch *disp, DISPID id, DISPPARAMS *dp, VARIANT *retv)
{
    const WORD flags = DISPATCH_METHOD|(retv ? DISPATCH_PROPERTYGET : 0);
    IDispatchEx *dispex;
    EXCEPINFO ei;
    HRESULT hres;

    memset(&ei, 0, sizeof(ei));
    if(retv)
        V_VT(retv) = VT_EMPTY;

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(FAILED(hres)) {
        UINT err = 0;

        TRACE("using IDispatch\n");
        return IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, flags, dp, retv, &ei, &err);
    }

    hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, flags, dp, retv, &ei, NULL /* CALLER_FIXME */);
    IDispatchEx_Release(dispex);
    return hres;
}

HRESULT disp_propput(script_ctx_t *ctx, IDispatch *disp, DISPID id, VARIANT *val)
{
    DISPID propput_dispid = DISPID_PROPERTYPUT;
    DISPPARAMS dp  = {val, &propput_dispid, 1, 1};
    IDispatchEx *dispex;
    EXCEPINFO ei = {0};
    HRESULT hres;

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres)) {
        hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, DISPATCH_PROPERTYPUT, &dp, NULL, &ei, NULL /* FIXME! */);
        IDispatchEx_Release(dispex);
    }else {
        ULONG err = 0;

        TRACE("using IDispatch\n");
        hres = IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, DISPATCH_PROPERTYPUT, &dp, NULL, &ei, &err);
    }

    return hres;
}
