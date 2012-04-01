/*
 * Copyright 2008-2009 Jacek Caban for CodeWeavers
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

static CRITICAL_SECTION cs_dispex_static_data;
static CRITICAL_SECTION_DEBUG cs_dispex_static_data_dbg =
{
    0, 0, &cs_dispex_static_data,
    { &cs_dispex_static_data_dbg.ProcessLocksList, &cs_dispex_static_data_dbg.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dispex_static_data") }
};
static CRITICAL_SECTION cs_dispex_static_data = { &cs_dispex_static_data_dbg, -1, 0, 0, 0, 0 };


static const WCHAR objectW[] = {'[','o','b','j','e','c','t',']',0};

typedef struct {
    DISPID id;
    BSTR name;
    tid_t tid;
    int func_disp_idx;
} func_info_t;

struct dispex_data_t {
    DWORD func_cnt;
    func_info_t *funcs;
    func_info_t **name_table;
    DWORD func_disp_cnt;

    struct list entry;
};

typedef struct {
    VARIANT var;
    LPWSTR name;
    DWORD flags;
} dynamic_prop_t;

#define DYNPROP_DELETED    0x01

typedef struct {
    DispatchEx dispex;
    IUnknown IUnknown_iface;
    DispatchEx *obj;
    func_info_t *info;
} func_disp_t;

struct dispex_dynamic_data_t {
    DWORD buf_size;
    DWORD prop_cnt;
    dynamic_prop_t *props;
    func_disp_t **func_disps;
};

#define DISPID_DYNPROP_0    0x50000000
#define DISPID_DYNPROP_MAX  0x5fffffff

#define FDEX_VERSION_MASK 0xf0000000

static ITypeLib *typelib;
static ITypeInfo *typeinfos[LAST_tid];
static struct list dispex_data_list = LIST_INIT(dispex_data_list);

static REFIID tid_ids[] = {
#define XIID(iface) &IID_ ## iface,
#define XDIID(iface) &DIID_ ## iface,
TID_LIST
#undef XIID
#undef XDIID
};

static HRESULT load_typelib(void)
{
    HRESULT hres;
    ITypeLib *tl;

    hres = LoadRegTypeLib(&LIBID_MSHTML, 4, 0, LOCALE_SYSTEM_DEFAULT, &tl);
    if(FAILED(hres)) {
        ERR("LoadRegTypeLib failed: %08x\n", hres);
        return hres;
    }

    if(InterlockedCompareExchangePointer((void**)&typelib, tl, NULL))
        ITypeLib_Release(tl);
    return hres;
}

static HRESULT get_typeinfo(tid_t tid, ITypeInfo **typeinfo)
{
    HRESULT hres;

    if (!typelib)
        hres = load_typelib();
    if (!typelib)
        return hres;

    if(!typeinfos[tid]) {
        ITypeInfo *ti;

        hres = ITypeLib_GetTypeInfoOfGuid(typelib, tid_ids[tid], &ti);
        if(FAILED(hres)) {
            ERR("GetTypeInfoOfGuid(%s) failed: %08x\n", debugstr_guid(tid_ids[tid]), hres);
            return hres;
        }

        if(InterlockedCompareExchangePointer((void**)(typeinfos+tid), ti, NULL))
            ITypeInfo_Release(ti);
    }

    *typeinfo = typeinfos[tid];
    return S_OK;
}

void release_typelib(void)
{
    dispex_data_t *iter;
    unsigned i;

    while(!list_empty(&dispex_data_list)) {
        iter = LIST_ENTRY(list_head(&dispex_data_list), dispex_data_t, entry);
        list_remove(&iter->entry);

        for(i=0; i < iter->func_cnt; i++)
            SysFreeString(iter->funcs[i].name);

        heap_free(iter->funcs);
        heap_free(iter->name_table);
        heap_free(iter);
    }

    if(!typelib)
        return;

    for(i=0; i < sizeof(typeinfos)/sizeof(*typeinfos); i++)
        if(typeinfos[i])
            ITypeInfo_Release(typeinfos[i]);

    ITypeLib_Release(typelib);
    DeleteCriticalSection(&cs_dispex_static_data);
}

HRESULT get_htmldoc_classinfo(ITypeInfo **typeinfo)
{
    HRESULT hres;

    if (!typelib)
        hres = load_typelib();
    if (!typelib)
        return hres;

    hres = ITypeLib_GetTypeInfoOfGuid(typelib, &CLSID_HTMLDocument, typeinfo);
    if(FAILED(hres))
        ERR("GetTypeInfoOfGuid failed: %08x\n", hres);
    return hres;
}

static void add_func_info(dispex_data_t *data, DWORD *size, tid_t tid, const FUNCDESC *desc, ITypeInfo *dti)
{
    HRESULT hres;

    if(data->func_cnt && data->funcs[data->func_cnt-1].id == desc->memid)
        return;

    if(data->func_cnt == *size)
        data->funcs = heap_realloc(data->funcs, (*size <<= 1)*sizeof(func_info_t));

    hres = ITypeInfo_GetDocumentation(dti, desc->memid, &data->funcs[data->func_cnt].name, NULL, NULL, NULL);
    if(FAILED(hres))
        return;

    data->funcs[data->func_cnt].id = desc->memid;
    data->funcs[data->func_cnt].tid = tid;
    data->funcs[data->func_cnt].func_disp_idx = (desc->invkind & DISPATCH_METHOD) ? data->func_disp_cnt++ : -1;

    data->func_cnt++;
}

static int dispid_cmp(const void *p1, const void *p2)
{
    return ((const func_info_t*)p1)->id - ((const func_info_t*)p2)->id;
}

static int func_name_cmp(const void *p1, const void *p2)
{
    return strcmpiW((*(func_info_t* const*)p1)->name, (*(func_info_t* const*)p2)->name);
}

static dispex_data_t *preprocess_dispex_data(DispatchEx *This)
{
    const tid_t *tid = This->data->iface_tids;
    FUNCDESC *funcdesc;
    dispex_data_t *data;
    DWORD size = 16, i;
    ITypeInfo *ti, *dti;
    HRESULT hres;

    TRACE("(%p)\n", This);

    if(This->data->disp_tid) {
        hres = get_typeinfo(This->data->disp_tid, &dti);
        if(FAILED(hres)) {
            ERR("Could not get disp type info: %08x\n", hres);
            return NULL;
        }
    }

    data = heap_alloc(sizeof(dispex_data_t));
    data->func_cnt = 0;
    data->func_disp_cnt = 0;
    data->funcs = heap_alloc(size*sizeof(func_info_t));
    list_add_tail(&dispex_data_list, &data->entry);

    while(*tid) {
        hres = get_typeinfo(*tid, &ti);
        if(FAILED(hres))
            break;

        i=7;
        while(1) {
            hres = ITypeInfo_GetFuncDesc(ti, i++, &funcdesc);
            if(FAILED(hres))
                break;

            add_func_info(data, &size, *tid, funcdesc, dti);
            ITypeInfo_ReleaseFuncDesc(ti, funcdesc);
        }

        tid++;
    }

    if(!data->func_cnt) {
        heap_free(data->funcs);
        data->name_table = NULL;
        data->funcs = NULL;
        return data;
    }


    data->funcs = heap_realloc(data->funcs, data->func_cnt * sizeof(func_info_t));
    qsort(data->funcs, data->func_cnt, sizeof(func_info_t), dispid_cmp);

    data->name_table = heap_alloc(data->func_cnt * sizeof(func_info_t*));
    for(i=0; i < data->func_cnt; i++)
        data->name_table[i] = data->funcs+i;
    qsort(data->name_table, data->func_cnt, sizeof(func_info_t*), func_name_cmp);

    return data;
}

static int id_cmp(const void *p1, const void *p2)
{
    return *(const DISPID*)p1 - *(const DISPID*)p2;
}

HRESULT get_dispids(tid_t tid, DWORD *ret_size, DISPID **ret)
{
    unsigned i, func_cnt;
    FUNCDESC *funcdesc;
    ITypeInfo *ti;
    TYPEATTR *attr;
    DISPID *ids;
    HRESULT hres;

    hres = get_typeinfo(tid, &ti);
    if(FAILED(hres))
        return hres;

    hres = ITypeInfo_GetTypeAttr(ti, &attr);
    if(FAILED(hres)) {
        ITypeInfo_Release(ti);
        return hres;
    }

    func_cnt = attr->cFuncs;
    ITypeInfo_ReleaseTypeAttr(ti, attr);

    ids = heap_alloc(func_cnt*sizeof(DISPID));
    if(!ids) {
        ITypeInfo_Release(ti);
        return E_OUTOFMEMORY;
    }

    for(i=0; i < func_cnt; i++) {
        hres = ITypeInfo_GetFuncDesc(ti, i, &funcdesc);
        if(FAILED(hres))
            break;

        ids[i] = funcdesc->memid;
        ITypeInfo_ReleaseFuncDesc(ti, funcdesc);
    }

    ITypeInfo_Release(ti);
    if(FAILED(hres)) {
        heap_free(ids);
        return hres;
    }

    qsort(ids, func_cnt, sizeof(DISPID), id_cmp);

    *ret_size = func_cnt;
    *ret = ids;
    return S_OK;
}

static dispex_data_t *get_dispex_data(DispatchEx *This)
{
    if(This->data->data)
        return This->data->data;

    EnterCriticalSection(&cs_dispex_static_data);

    if(!This->data->data)
        This->data->data = preprocess_dispex_data(This);

    LeaveCriticalSection(&cs_dispex_static_data);

    return This->data->data;
}

static inline BOOL is_custom_dispid(DISPID id)
{
    return MSHTML_DISPID_CUSTOM_MIN <= id && id <= MSHTML_DISPID_CUSTOM_MAX;
}

static inline BOOL is_dynamic_dispid(DISPID id)
{
    return DISPID_DYNPROP_0 <= id && id <= DISPID_DYNPROP_MAX;
}

static HRESULT variant_copy(VARIANT *dest, VARIANT *src)
{
    if(V_VT(src) == VT_BSTR && !V_BSTR(src)) {
        V_VT(dest) = VT_BSTR;
        V_BSTR(dest) = NULL;
        return S_OK;
    }

    return VariantCopy(dest, src);
}

static inline dispex_dynamic_data_t *get_dynamic_data(DispatchEx *This)
{
    if(This->dynamic_data)
        return This->dynamic_data;

    This->dynamic_data = heap_alloc_zero(sizeof(dispex_dynamic_data_t));
    if(!This->dynamic_data)
        return NULL;

    if(This->data->vtbl && This->data->vtbl->populate_props)
        This->data->vtbl->populate_props(This);

    return This->dynamic_data;
}

static HRESULT get_dynamic_prop(DispatchEx *This, const WCHAR *name, DWORD flags, dynamic_prop_t **ret)
{
    const BOOL alloc = flags & fdexNameEnsure;
    dispex_dynamic_data_t *data;
    dynamic_prop_t *prop;

    data = get_dynamic_data(This);
    if(!data)
        return E_OUTOFMEMORY;

    for(prop = data->props; prop < data->props+data->prop_cnt; prop++) {
        if(flags & fdexNameCaseInsensitive ? !strcmpiW(prop->name, name) : !strcmpW(prop->name, name)) {
            if(prop->flags & DYNPROP_DELETED) {
                if(!alloc)
                    return DISP_E_UNKNOWNNAME;
                prop->flags &= ~DYNPROP_DELETED;
            }
            *ret = prop;
            return S_OK;
        }
    }

    if(!alloc)
        return DISP_E_UNKNOWNNAME;

    TRACE("creating dynamic prop %s\n", debugstr_w(name));

    if(!data->buf_size) {
        data->props = heap_alloc(sizeof(dynamic_prop_t)*4);
        if(!data->props)
            return E_OUTOFMEMORY;
        data->buf_size = 4;
    }else if(data->buf_size == data->prop_cnt) {
        dynamic_prop_t *new_props;

        new_props = heap_realloc(data->props, sizeof(dynamic_prop_t)*(data->buf_size<<1));
        if(!new_props)
            return E_OUTOFMEMORY;

        data->props = new_props;
        data->buf_size <<= 1;
    }

    prop = data->props + data->prop_cnt;

    prop->name = heap_strdupW(name);
    if(!prop->name)
        return E_OUTOFMEMORY;

    VariantInit(&prop->var);
    prop->flags = 0;
    data->prop_cnt++;
    *ret = prop;
    return S_OK;
}

HRESULT dispex_get_dprop_ref(DispatchEx *This, const WCHAR *name, BOOL alloc, VARIANT **ret)
{
    dynamic_prop_t *prop;
    HRESULT hres;

    hres = get_dynamic_prop(This, name, alloc ? fdexNameEnsure : 0, &prop);
    if(FAILED(hres))
        return hres;

    *ret = &prop->var;
    return S_OK;
}

static HRESULT dispex_value(DispatchEx *This, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    if(This->data->vtbl && This->data->vtbl->value)
        return This->data->vtbl->value(This, lcid, flags, params, res, ei, caller);

    switch(flags) {
    case DISPATCH_PROPERTYGET:
        V_VT(res) = VT_BSTR;
        V_BSTR(res) = SysAllocString(objectW);
        if(!V_BSTR(res))
            return E_OUTOFMEMORY;
        break;
    default:
        FIXME("Unimplemented flags %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT typeinfo_invoke(DispatchEx *This, func_info_t *func, WORD flags, DISPPARAMS *dp, VARIANT *res,
        EXCEPINFO *ei)
{
    ITypeInfo *ti;
    IUnknown *unk;
    UINT argerr=0;
    HRESULT hres;

    hres = get_typeinfo(func->tid, &ti);
    if(FAILED(hres)) {
        ERR("Could not get type info: %08x\n", hres);
        return hres;
    }

    hres = IUnknown_QueryInterface(This->outer, tid_ids[func->tid], (void**)&unk);
    if(FAILED(hres)) {
        ERR("Could not get iface %s: %08x\n", debugstr_guid(tid_ids[func->tid]), hres);
        return E_FAIL;
    }

    hres = ITypeInfo_Invoke(ti, unk, func->id, flags, dp, res, ei, &argerr);

    IUnknown_Release(unk);
    return hres;
}

static inline func_disp_t *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, func_disp_t, IUnknown_iface);
}

static HRESULT WINAPI Function_QueryInterface(IUnknown *iface, REFIID riid, void **ppv)
{
    func_disp_t *This = impl_from_IUnknown(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IUnknown_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI Function_AddRef(IUnknown *iface)
{
    func_disp_t *This = impl_from_IUnknown(iface);

    TRACE("(%p)\n", This);

    return IDispatchEx_AddRef(&This->obj->IDispatchEx_iface);
}

static ULONG WINAPI Function_Release(IUnknown *iface)
{
    func_disp_t *This = impl_from_IUnknown(iface);

    TRACE("(%p)\n", This);

    return IDispatchEx_Release(&This->obj->IDispatchEx_iface);
}

static const IUnknownVtbl FunctionUnkVtbl = {
    Function_QueryInterface,
    Function_AddRef,
    Function_Release
};

static inline func_disp_t *impl_from_DispatchEx(DispatchEx *iface)
{
    return CONTAINING_RECORD(iface, func_disp_t, dispex);
}

static HRESULT function_value(DispatchEx *dispex, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_disp_t *This = impl_from_DispatchEx(dispex);
    HRESULT hres;

    switch(flags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        hres = typeinfo_invoke(This->obj, This->info, flags, params, res, ei);
        break;
    default:
        FIXME("Unimplemented flags %x\n", flags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static const dispex_static_data_vtbl_t function_dispex_vtbl = {
    function_value,
    NULL,
    NULL,
    NULL
};

static const tid_t function_iface_tids[] = {0};

static dispex_static_data_t function_dispex = {
    &function_dispex_vtbl,
    NULL_tid,
    NULL,
    function_iface_tids
};

static func_disp_t *create_func_disp(DispatchEx *obj, func_info_t *info)
{
    func_disp_t *ret;

    ret = heap_alloc_zero(sizeof(func_disp_t));
    if(!ret)
        return NULL;

    ret->IUnknown_iface.lpVtbl = &FunctionUnkVtbl;
    init_dispex(&ret->dispex, &ret->IUnknown_iface,  &function_dispex);
    ret->obj = obj;
    ret->info = info;

    return ret;
}

static HRESULT function_invoke(DispatchEx *This, func_info_t *func, WORD flags, DISPPARAMS *dp, VARIANT *res,
        EXCEPINFO *ei)
{
    HRESULT hres;

    switch(flags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        hres = typeinfo_invoke(This, func, flags, dp, res, ei);
        break;
    case DISPATCH_PROPERTYGET: {
        dispex_dynamic_data_t *dynamic_data;

        if(func->id == DISPID_VALUE) {
            BSTR ret;

            ret = SysAllocString(objectW);
            if(!ret)
                return E_OUTOFMEMORY;

            V_VT(res) = VT_BSTR;
            V_BSTR(res) = ret;
            return S_OK;
        }

        dynamic_data = get_dynamic_data(This);
        if(!dynamic_data)
            return E_OUTOFMEMORY;

        if(!dynamic_data->func_disps) {
            dynamic_data->func_disps = heap_alloc_zero(This->data->data->func_disp_cnt * sizeof(func_disp_t*));
            if(!dynamic_data->func_disps)
                return E_OUTOFMEMORY;
        }

        if(!dynamic_data->func_disps[func->func_disp_idx]) {
            dynamic_data->func_disps[func->func_disp_idx] = create_func_disp(This, func);
            if(!dynamic_data->func_disps[func->func_disp_idx])
                return E_OUTOFMEMORY;
        }

        V_VT(res) = VT_DISPATCH;
        V_DISPATCH(res) = (IDispatch*)&dynamic_data->func_disps[func->func_disp_idx]->dispex.IDispatchEx_iface;
        IDispatch_AddRef(V_DISPATCH(res));
        hres = S_OK;
        break;
    }
    default:
        FIXME("Unimplemented flags %x\n", flags);
        /* fall through */
    case DISPATCH_PROPERTYPUT:
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT get_builtin_func(dispex_data_t *data, DISPID id, func_info_t **ret)
{
    int min, max, n;

    min = 0;
    max = data->func_cnt-1;

    while(min <= max) {
        n = (min+max)/2;

        if(data->funcs[n].id == id) {
            *ret = data->funcs+n;
            return S_OK;
        }

        if(data->funcs[n].id < id)
            min = n+1;
        else
            max = n-1;
    }

    WARN("invalid id %x\n", id);
    return DISP_E_UNKNOWNNAME;
}

static HRESULT get_builtin_id(DispatchEx *This, BSTR name, DWORD grfdex, DISPID *ret)
{
    dispex_data_t *data;
    int min, max, n, c;

    data = get_dispex_data(This);
    if(!data)
        return E_FAIL;

    min = 0;
    max = data->func_cnt-1;

    while(min <= max) {
        n = (min+max)/2;

        c = strcmpiW(data->name_table[n]->name, name);
        if(!c) {
            if((grfdex & fdexNameCaseSensitive) && strcmpW(data->name_table[n]->name, name))
                break;

            *ret = data->name_table[n]->id;
            return S_OK;
        }

        if(c > 0)
            max = n-1;
        else
            min = n+1;
    }

    if(This->data->vtbl && This->data->vtbl->get_dispid) {
        HRESULT hres;

        hres = This->data->vtbl->get_dispid(This, name, grfdex, ret);
        if(hres != DISP_E_UNKNOWNNAME)
            return hres;
    }

    return DISP_E_UNKNOWNNAME;
}

static HRESULT invoke_builtin_prop(DispatchEx *This, DISPID id, LCID lcid, WORD flags, DISPPARAMS *dp,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    dispex_data_t *data;
    func_info_t *func;
    HRESULT hres;

    data = get_dispex_data(This);
    if(!data)
        return E_FAIL;

    hres = get_builtin_func(data, id, &func);
    if(id == DISPID_VALUE && hres == DISP_E_UNKNOWNNAME)
        return dispex_value(This, lcid, flags, dp, res, ei, caller);
    if(FAILED(hres))
        return hres;

    if(func->func_disp_idx == -1)
        hres = typeinfo_invoke(This, func, flags, dp, res, ei);
    else
        hres = function_invoke(This, func, flags, dp, res, ei);

    return hres;
}

HRESULT remove_prop(DispatchEx *This, BSTR name, VARIANT_BOOL *success)
{
    dynamic_prop_t *prop;
    DISPID id;
    HRESULT hres;

    hres = get_builtin_id(This, name, 0, &id);
    if(hres == S_OK) {
        DISPID named_id = DISPID_PROPERTYPUT;
        VARIANT var;
        DISPPARAMS dp = {&var,&named_id,1,1};
        EXCEPINFO ei;

        V_VT(&var) = VT_EMPTY;
        memset(&ei, 0, sizeof(ei));
        hres = invoke_builtin_prop(This, id, 0, DISPATCH_PROPERTYPUT, &dp, NULL, &ei, NULL);
        if(FAILED(hres))
            return hres;

        *success = VARIANT_TRUE;
        return S_OK;
    }

    hres = get_dynamic_prop(This, name, 0, &prop);
    if(FAILED(hres)) {
        if(hres != DISP_E_UNKNOWNNAME)
            return hres;
        *success = VARIANT_FALSE;
        return S_OK;
    }

    VariantClear(&prop->var);
    prop->flags |= DYNPROP_DELETED;
    *success = VARIANT_TRUE;
    return S_OK;
}

static inline DispatchEx *impl_from_IDispatchEx(IDispatchEx *iface)
{
    return CONTAINING_RECORD(iface, DispatchEx, IDispatchEx_iface);
}

static HRESULT WINAPI DispatchEx_QueryInterface(IDispatchEx *iface, REFIID riid, void **ppv)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    return IUnknown_QueryInterface(This->outer, riid, ppv);
}

static ULONG WINAPI DispatchEx_AddRef(IDispatchEx *iface)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    return IUnknown_AddRef(This->outer);
}

static ULONG WINAPI DispatchEx_Release(IDispatchEx *iface)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    return IUnknown_Release(This->outer);
}

static HRESULT WINAPI DispatchEx_GetTypeInfoCount(IDispatchEx *iface, UINT *pctinfo)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetTypeInfo(IDispatchEx *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    HRESULT hres;

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    hres = get_typeinfo(This->data->disp_tid, ppTInfo);
    if(FAILED(hres))
        return hres;

    ITypeInfo_AddRef(*ppTInfo);
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetIDsOfNames(IDispatchEx *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
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
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    return IDispatchEx_InvokeEx(&This->IDispatchEx_iface, dispIdMember, lcid, wFlags, pDispParams,
            pVarResult, pExcepInfo, NULL);
}

static HRESULT WINAPI DispatchEx_GetDispID(IDispatchEx *iface, BSTR bstrName, DWORD grfdex, DISPID *pid)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    dynamic_prop_t *dprop;
    HRESULT hres;

    TRACE("(%p)->(%s %x %p)\n", This, debugstr_w(bstrName), grfdex, pid);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameCaseInsensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK))
        FIXME("Unsupported grfdex %x\n", grfdex);

    hres = get_builtin_id(This, bstrName, grfdex, pid);
    if(hres != DISP_E_UNKNOWNNAME)
        return hres;

    hres = get_dynamic_prop(This, bstrName, grfdex, &dprop);
    if(FAILED(hres))
        return hres;

    *pid = DISPID_DYNPROP_0 + (dprop - This->dynamic_data->props);
    return S_OK;
}

static HRESULT WINAPI DispatchEx_InvokeEx(IDispatchEx *iface, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *pvarRes, EXCEPINFO *pei, IServiceProvider *pspCaller)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    HRESULT hres;

    TRACE("(%p)->(%x %x %x %p %p %p %p)\n", This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    if(is_custom_dispid(id) && This->data->vtbl && This->data->vtbl->invoke)
        return This->data->vtbl->invoke(This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    if(wFlags == DISPATCH_CONSTRUCT) {
        if(id == DISPID_VALUE) {
            if(This->data->vtbl && This->data->vtbl->value) {
                return This->data->vtbl->value(This, lcid, wFlags, pdp, pvarRes, pei, pspCaller);
            }
            FIXME("DISPATCH_CONSTRUCT flag but missing value function\n");
            return E_FAIL;
        }
        FIXME("DISPATCH_CONSTRUCT flag without DISPID_VALUE\n");
        return E_FAIL;
    }

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;
        dynamic_prop_t *prop;

        if(!get_dynamic_data(This) || This->dynamic_data->prop_cnt <= idx)
            return DISP_E_UNKNOWNNAME;

        prop = This->dynamic_data->props+idx;

        switch(wFlags) {
        case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
            if(!pvarRes)
                return E_INVALIDARG;
            /* fall through */
        case DISPATCH_METHOD: {
            DISPID named_arg = DISPID_THIS;
            DISPPARAMS dp = {NULL, &named_arg, 0, 1};
            IDispatchEx *dispex;

            if(V_VT(&prop->var) != VT_DISPATCH) {
                FIXME("invoke %s\n", debugstr_variant(&prop->var));
                return E_NOTIMPL;
            }

            if(pdp->cNamedArgs) {
                FIXME("named args not supported\n");
                return E_NOTIMPL;
            }

            dp.rgvarg = heap_alloc((pdp->cArgs+1)*sizeof(VARIANTARG));
            if(!dp.rgvarg)
                return E_OUTOFMEMORY;

            dp.cArgs = pdp->cArgs+1;
            memcpy(dp.rgvarg+1, pdp->rgvarg, pdp->cArgs*sizeof(VARIANTARG));

            V_VT(dp.rgvarg) = VT_DISPATCH;
            V_DISPATCH(dp.rgvarg) = (IDispatch*)&This->IDispatchEx_iface;

            hres = IDispatch_QueryInterface(V_DISPATCH(&prop->var), &IID_IDispatchEx, (void**)&dispex);
            TRACE("%s call\n", debugstr_w(This->dynamic_data->props[idx].name));
            if(SUCCEEDED(hres)) {
                hres = IDispatchEx_InvokeEx(dispex, DISPID_VALUE, lcid, wFlags, &dp, pvarRes, pei, pspCaller);
                IDispatchEx_Release(dispex);
            }else {
                ULONG err = 0;
                hres = IDispatch_Invoke(V_DISPATCH(&prop->var), DISPID_VALUE, &IID_NULL, lcid, wFlags, pdp, pvarRes, pei, &err);
            }
            TRACE("%s ret %08x\n", debugstr_w(This->dynamic_data->props[idx].name), hres);

            heap_free(dp.rgvarg);
            return hres;
        }
        case DISPATCH_PROPERTYGET:
            if(prop->flags & DYNPROP_DELETED)
                return DISP_E_UNKNOWNNAME;
            V_VT(pvarRes) = VT_EMPTY;
            return variant_copy(pvarRes, &prop->var);
        case DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF:
        case DISPATCH_PROPERTYPUT:
            if(pdp->cArgs != 1 || (pdp->cNamedArgs == 1 && *pdp->rgdispidNamedArgs != DISPID_PROPERTYPUT)
               || pdp->cNamedArgs > 1) {
                FIXME("invalid args\n");
                return E_INVALIDARG;
            }

            TRACE("put %s\n", debugstr_variant(pdp->rgvarg));
            VariantClear(&prop->var);
            hres = variant_copy(&prop->var, pdp->rgvarg);
            if(FAILED(hres))
                return hres;

            prop->flags &= ~DYNPROP_DELETED;
            return S_OK;
        default:
            FIXME("unhandled wFlags %x\n", wFlags);
            return E_NOTIMPL;
        }
    }

    return invoke_builtin_prop(This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);
}

static HRESULT WINAPI DispatchEx_DeleteMemberByName(IDispatchEx *iface, BSTR bstrName, DWORD grfdex)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%s %x)\n", This, debugstr_w(bstrName), grfdex);

    /* Not implemented by IE */
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_DeleteMemberByDispID(IDispatchEx *iface, DISPID id)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%x)\n", This, id);

    /* Not implemented by IE */
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberProperties(IDispatchEx *iface, DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %x %p)\n", This, id, grfdexFetch, pgrfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberName(IDispatchEx *iface, DISPID id, BSTR *pbstrName)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    dispex_data_t *data;
    func_info_t *func;
    HRESULT hres;

    TRACE("(%p)->(%x %p)\n", This, id, pbstrName);

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;

        if(!get_dynamic_data(This) || This->dynamic_data->prop_cnt <= idx)
            return DISP_E_UNKNOWNNAME;

        *pbstrName = SysAllocString(This->dynamic_data->props[idx].name);
        if(!*pbstrName)
            return E_OUTOFMEMORY;

        return S_OK;
    }

    data = get_dispex_data(This);
    if(!data)
        return E_FAIL;

    hres = get_builtin_func(data, id, &func);
    if(FAILED(hres))
        return hres;

    *pbstrName = SysAllocString(func->name);
    if(!*pbstrName)
        return E_OUTOFMEMORY;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetNextDispID(IDispatchEx *iface, DWORD grfdex, DISPID id, DISPID *pid)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    dispex_data_t *data;
    func_info_t *func;
    HRESULT hres;

    TRACE("(%p)->(%x %x %p)\n", This, grfdex, id, pid);

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;

        if(!get_dynamic_data(This) || This->dynamic_data->prop_cnt <= idx)
            return DISP_E_UNKNOWNNAME;

        while(++idx < This->dynamic_data->prop_cnt && This->dynamic_data->props[idx].flags & DYNPROP_DELETED);

        if(idx == This->dynamic_data->prop_cnt) {
            *pid = DISPID_STARTENUM;
            return S_FALSE;
        }

        *pid = DISPID_DYNPROP_0+idx;
        return S_OK;
    }

    data = get_dispex_data(This);
    if(!data)
        return E_FAIL;

    if(id == DISPID_STARTENUM) {
        func = data->funcs;
    }else {
        hres = get_builtin_func(data, id, &func);
        if(FAILED(hres))
            return hres;
        func++;
    }

    while(func < data->funcs+data->func_cnt) {
        /* FIXME: Skip hidden properties */
        if(func->func_disp_idx == -1) {
            *pid = func->id;
            return S_OK;
        }
        func++;
    }

    if(get_dynamic_data(This) && This->dynamic_data->prop_cnt) {
        *pid = DISPID_DYNPROP_0;
        return S_OK;
    }

    *pid = DISPID_STARTENUM;
    return S_FALSE;
}

static HRESULT WINAPI DispatchEx_GetNameSpaceParent(IDispatchEx *iface, IUnknown **ppunk)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
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

BOOL dispex_query_interface(DispatchEx *This, REFIID riid, void **ppv)
{
    static const IID IID_UndocumentedScriptIface =
        {0x719c3050,0xf9d3,0x11cf,{0xa4,0x93,0x00,0x40,0x05,0x23,0xa8,0xa0}};
    static const IID IID_IDispatchJS =
        {0x719c3050,0xf9d3,0x11cf,{0xa4,0x93,0x00,0x40,0x05,0x23,0xa8,0xa6}};

    if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchEx, riid)) {
        TRACE("(%p)->(IID_IDispatchEx %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchJS, riid)) {
        TRACE("(%p)->(IID_IDispatchJS %p) returning NULL\n", This, ppv);
        *ppv = NULL;
    }else if(IsEqualGUID(&IID_UndocumentedScriptIface, riid)) {
        TRACE("(%p)->(IID_UndocumentedScriptIface %p) returning NULL\n", This, ppv);
        *ppv = NULL;
    }else {
        return FALSE;
    }

    if(*ppv)
        IUnknown_AddRef((IUnknown*)*ppv);
    return TRUE;
}

void release_dispex(DispatchEx *This)
{
    dynamic_prop_t *prop;

    if(!This->dynamic_data)
        return;

    for(prop = This->dynamic_data->props; prop < This->dynamic_data->props + This->dynamic_data->prop_cnt; prop++) {
        VariantClear(&prop->var);
        heap_free(prop->name);
    }

    heap_free(This->dynamic_data->props);

    if(This->dynamic_data->func_disps) {
        unsigned i;

        for(i=0; i < This->data->data->func_disp_cnt; i++) {
            if(This->dynamic_data->func_disps[i]) {
                release_dispex(&This->dynamic_data->func_disps[i]->dispex);
                heap_free(This->dynamic_data->func_disps[i]);
            }
        }

        heap_free(This->dynamic_data->func_disps);
    }

    heap_free(This->dynamic_data);
}

void init_dispex(DispatchEx *dispex, IUnknown *outer, dispex_static_data_t *data)
{
    dispex->IDispatchEx_iface.lpVtbl = &DispatchExVtbl;
    dispex->outer = outer;
    dispex->data = data;
    dispex->dynamic_data = NULL;
}
