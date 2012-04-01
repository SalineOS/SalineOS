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

typedef struct {
    vbscode_t *code;
    instr_t *instr;
    script_ctx_t *script;
    function_t *func;
    IDispatch *this_obj;

    VARIANT *args;
    VARIANT *vars;

    dynamic_var_t *dynamic_vars;
    vbsheap_t heap;

    BOOL resume_next;

    unsigned stack_size;
    unsigned top;
    VARIANT *stack;

    VARIANT ret_val;
} exec_ctx_t;

typedef HRESULT (*instr_func_t)(exec_ctx_t*);

typedef enum {
    REF_NONE,
    REF_DISP,
    REF_VAR,
    REF_OBJ,
    REF_CONST,
    REF_FUNC
} ref_type_t;

typedef struct {
    ref_type_t type;
    union {
        struct {
            IDispatch *disp;
            DISPID id;
        } d;
        VARIANT *v;
        function_t *f;
        IDispatch *obj;
    } u;
} ref_t;

typedef struct {
    VARIANT *v;
    VARIANT store;
    BOOL owned;
} variant_val_t;

static BOOL lookup_dynamic_vars(dynamic_var_t *var, const WCHAR *name, ref_t *ref)
{
    while(var) {
        if(!strcmpiW(var->name, name)) {
            ref->type = var->is_const ? REF_CONST : REF_VAR;
            ref->u.v = &var->v;
            return TRUE;
        }

        var = var->next;
    }

    return FALSE;
}

static HRESULT lookup_identifier(exec_ctx_t *ctx, BSTR name, vbdisp_invoke_type_t invoke_type, ref_t *ref)
{
    named_item_t *item;
    function_t *func;
    unsigned i;
    DISPID id;
    HRESULT hres;

    static const WCHAR errW[] = {'e','r','r',0};

    if(invoke_type == VBDISP_LET
            && (ctx->func->type == FUNC_FUNCTION || ctx->func->type == FUNC_PROPGET || ctx->func->type == FUNC_DEFGET)
            && !strcmpiW(name, ctx->func->name)) {
        ref->type = REF_VAR;
        ref->u.v = &ctx->ret_val;
        return S_OK;
    }

    for(i=0; i < ctx->func->var_cnt; i++) {
        if(!strcmpiW(ctx->func->vars[i].name, name)) {
            ref->type = REF_VAR;
            ref->u.v = ctx->vars+i;
            return TRUE;
        }
    }

    for(i=0; i < ctx->func->arg_cnt; i++) {
        if(!strcmpiW(ctx->func->args[i].name, name)) {
            ref->type = REF_VAR;
            ref->u.v = ctx->args+i;
            return S_OK;
        }
    }

    if(lookup_dynamic_vars(ctx->func->type == FUNC_GLOBAL ? ctx->script->global_vars : ctx->dynamic_vars, name, ref))
        return S_OK;

    if(ctx->func->type != FUNC_GLOBAL) {
        hres = disp_get_id(ctx->this_obj, name, invoke_type, TRUE, &id);
        if(SUCCEEDED(hres)) {
            ref->type = REF_DISP;
            ref->u.d.disp = ctx->this_obj;
            ref->u.d.id = id;
            return S_OK;
        }
    }

    if(ctx->func->type != FUNC_GLOBAL && lookup_dynamic_vars(ctx->script->global_vars, name, ref))
        return S_OK;

    for(func = ctx->script->global_funcs; func; func = func->next) {
        if(!strcmpiW(func->name, name)) {
            ref->type = REF_FUNC;
            ref->u.f = func;
            return S_OK;
        }
    }

    if(!strcmpiW(name, errW)) {
        ref->type = REF_OBJ;
        ref->u.obj = (IDispatch*)&ctx->script->err_obj->IDispatchEx_iface;
        return S_OK;
    }

    hres = vbdisp_get_id(ctx->script->global_obj, name, invoke_type, TRUE, &id);
    if(SUCCEEDED(hres)) {
        ref->type = REF_DISP;
        ref->u.d.disp = (IDispatch*)&ctx->script->global_obj->IDispatchEx_iface;
        ref->u.d.id = id;
        return S_OK;
    }

    LIST_FOR_EACH_ENTRY(item, &ctx->script->named_items, named_item_t, entry) {
        if((item->flags & SCRIPTITEM_GLOBALMEMBERS)) {
            hres = disp_get_id(item->disp, name, invoke_type, FALSE, &id);
            if(SUCCEEDED(hres)) {
                ref->type = REF_DISP;
                ref->u.d.disp = item->disp;
                ref->u.d.id = id;
                return S_OK;
            }
        }

        if((item->flags & SCRIPTITEM_ISVISIBLE) && !strcmpiW(item->name, name)) {
            if(!item->disp) {
                IUnknown *unk;

                hres = IActiveScriptSite_GetItemInfo(ctx->script->site, name, SCRIPTINFO_IUNKNOWN, &unk, NULL);
                if(FAILED(hres)) {
                    WARN("GetItemInfo failed: %08x\n", hres);
                    continue;
                }

                hres = IUnknown_QueryInterface(unk, &IID_IDispatch, (void**)&item->disp);
                IUnknown_Release(unk);
                if(FAILED(hres)) {
                    WARN("object does not implement IDispatch\n");
                    continue;
                }
            }

            ref->type = REF_OBJ;
            ref->u.obj = item->disp;
            return S_OK;
        }
    }

    ref->type = REF_NONE;
    return S_OK;
}

static HRESULT add_dynamic_var(exec_ctx_t *ctx, const WCHAR *name, BOOL is_const, VARIANT *val, BOOL own_val)
{
    dynamic_var_t *new_var;
    vbsheap_t *heap;
    WCHAR *str;
    unsigned size;
    HRESULT hres;

    heap = ctx->func->type == FUNC_GLOBAL ? &ctx->script->heap : &ctx->heap;

    new_var = vbsheap_alloc(heap, sizeof(*new_var));
    if(!new_var)
        return E_OUTOFMEMORY;

    size = (strlenW(name)+1)*sizeof(WCHAR);
    str = vbsheap_alloc(heap, size);
    if(!str)
        return E_OUTOFMEMORY;
    memcpy(str, name, size);
    new_var->name = str;
    new_var->is_const = is_const;

    if(own_val) {
        new_var->v = *val;
    }else {
        hres = VariantCopy(&new_var->v, val);
        if(FAILED(hres))
            return hres;
    }

    if(ctx->func->type == FUNC_GLOBAL) {
        new_var->next = ctx->script->global_vars;
        ctx->script->global_vars = new_var;
    }else {
        new_var->next = ctx->dynamic_vars;
        ctx->dynamic_vars = new_var;
    }

    return S_OK;
}

static inline VARIANT *stack_pop(exec_ctx_t *ctx)
{
    assert(ctx->top);
    return ctx->stack + --ctx->top;
}

static inline VARIANT *stack_top(exec_ctx_t *ctx, unsigned n)
{
    assert(ctx->top >= n);
    return ctx->stack + (ctx->top-n-1);
}

static HRESULT stack_push(exec_ctx_t *ctx, VARIANT *v)
{
    if(ctx->stack_size == ctx->top) {
        VARIANT *new_stack;

        new_stack = heap_realloc(ctx->stack, ctx->stack_size*2*sizeof(*ctx->stack));
        if(!new_stack) {
            VariantClear(v);
            return E_OUTOFMEMORY;
        }

        ctx->stack = new_stack;
        ctx->stack_size *= 2;
    }

    ctx->stack[ctx->top++] = *v;
    return S_OK;
}

static void stack_popn(exec_ctx_t *ctx, unsigned n)
{
    while(n--)
        VariantClear(stack_pop(ctx));
}

static HRESULT stack_pop_val(exec_ctx_t *ctx, variant_val_t *v)
{
    VARIANT *var;

    var = stack_pop(ctx);

    if(V_VT(var) == (VT_BYREF|VT_VARIANT)) {
        v->owned = FALSE;
        var = V_VARIANTREF(var);
    }else {
        v->owned = TRUE;
    }

    if(V_VT(var) == VT_DISPATCH) {
        DISPPARAMS dp = {0};
        HRESULT hres;

        hres = disp_call(ctx->script, V_DISPATCH(var), DISPID_VALUE, &dp, &v->store);
        if(v->owned)
            IDispatch_Release(V_DISPATCH(var));
        if(FAILED(hres))
            return hres;

        v->owned = TRUE;
        v->v = &v->store;
    }else {
        v->v = var;
    }

    return S_OK;
}

static inline void release_val(variant_val_t *v)
{
    if(v->owned)
        VariantClear(v->v);
}

static HRESULT stack_pop_disp(exec_ctx_t *ctx, IDispatch **ret)
{
    VARIANT *v = stack_pop(ctx);

    if(V_VT(v) == VT_DISPATCH) {
        *ret = V_DISPATCH(v);
        return S_OK;
    }

    if(V_VT(v) != (VT_VARIANT|VT_BYREF)) {
        FIXME("not supported type: %s\n", debugstr_variant(v));
        VariantClear(v);
        return E_FAIL;
    }

    v = V_BYREF(v);
    if(V_VT(v) != VT_DISPATCH) {
        FIXME("not disp %s\n", debugstr_variant(v));
        return E_FAIL;
    }

    if(V_DISPATCH(v))
        IDispatch_AddRef(V_DISPATCH(v));
    *ret = V_DISPATCH(v);
    return S_OK;
}

static inline void instr_jmp(exec_ctx_t *ctx, unsigned addr)
{
    ctx->instr = ctx->code->instrs + addr;
}

static void vbstack_to_dp(exec_ctx_t *ctx, unsigned arg_cnt, DISPPARAMS *dp)
{
    dp->cArgs = arg_cnt;
    dp->rgdispidNamedArgs = NULL;
    dp->cNamedArgs = 0;

    if(arg_cnt) {
        VARIANT tmp;
        unsigned i;

        assert(ctx->top >= arg_cnt);

        for(i=1; i*2 <= arg_cnt; i++) {
            tmp = ctx->stack[ctx->top-i];
            ctx->stack[ctx->top-i] = ctx->stack[ctx->top-arg_cnt+i-1];
            ctx->stack[ctx->top-arg_cnt+i-1] = tmp;
        }

        dp->rgvarg = ctx->stack + ctx->top-arg_cnt;
    }else {
        dp->rgvarg = NULL;
    }
}

static HRESULT do_icall(exec_ctx_t *ctx, VARIANT *res)
{
    BSTR identifier = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    DISPPARAMS dp;
    ref_t ref;
    HRESULT hres;

    hres = lookup_identifier(ctx, identifier, VBDISP_CALLGET, &ref);
    if(FAILED(hres))
        return hres;

    vbstack_to_dp(ctx, arg_cnt, &dp);

    switch(ref.type) {
    case REF_VAR:
    case REF_CONST:
        if(!res) {
            FIXME("REF_VAR no res\n");
            return E_NOTIMPL;
        }

        if(arg_cnt) {
            FIXME("arguments not implemented\n");
            return E_NOTIMPL;
        }

        V_VT(res) = VT_BYREF|VT_VARIANT;
        V_BYREF(res) = V_VT(ref.u.v) == (VT_VARIANT|VT_BYREF) ? V_VARIANTREF(ref.u.v) : ref.u.v;
        break;
    case REF_DISP:
        hres = disp_call(ctx->script, ref.u.d.disp, ref.u.d.id, &dp, res);
        if(FAILED(hres))
            return hres;
        break;
    case REF_FUNC:
        hres = exec_script(ctx->script, ref.u.f, NULL, &dp, res);
        if(FAILED(hres))
            return hres;
        break;
    case REF_OBJ:
        if(arg_cnt) {
            FIXME("arguments on object\n");
            return E_NOTIMPL;
        }

        if(res) {
            IDispatch_AddRef(ref.u.obj);
            V_VT(res) = VT_DISPATCH;
            V_DISPATCH(res) = ref.u.obj;
        }
        break;
    case REF_NONE:
        FIXME("%s not found\n", debugstr_w(identifier));
        return DISP_E_UNKNOWNNAME;
    }

    stack_popn(ctx, arg_cnt);
    return S_OK;
}

static HRESULT interp_icall(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = do_icall(ctx, &v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_icallv(exec_ctx_t *ctx)
{
    TRACE("\n");
    return do_icall(ctx, NULL);
}

static HRESULT do_mcall(exec_ctx_t *ctx, VARIANT *res)
{
    const BSTR identifier = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    IDispatch *obj;
    DISPPARAMS dp;
    DISPID id;
    HRESULT hres;

    hres = stack_pop_disp(ctx, &obj);
    if(FAILED(hres))
        return hres;

    if(!obj) {
        FIXME("NULL obj\n");
        return E_FAIL;
    }

    vbstack_to_dp(ctx, arg_cnt, &dp);

    hres = disp_get_id(obj, identifier, VBDISP_CALLGET, FALSE, &id);
    if(SUCCEEDED(hres))
        hres = disp_call(ctx->script, obj, id, &dp, res);
    IDispatch_Release(obj);
    if(FAILED(hres))
        return hres;

    stack_popn(ctx, arg_cnt);
    return S_OK;
}

static HRESULT interp_mcall(exec_ctx_t *ctx)
{
    VARIANT res;
    HRESULT hres;

    TRACE("\n");

    hres = do_mcall(ctx, &res);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &res);
}

static HRESULT interp_mcallv(exec_ctx_t *ctx)
{
    TRACE("\n");

    return do_mcall(ctx, NULL);
}

static HRESULT assign_ident(exec_ctx_t *ctx, BSTR name, VARIANT *val, BOOL own_val)
{
    ref_t ref;
    HRESULT hres;

    hres = lookup_identifier(ctx, name, VBDISP_LET, &ref);
    if(FAILED(hres))
        return hres;

    switch(ref.type) {
    case REF_VAR: {
        VARIANT *v = ref.u.v;

        if(V_VT(v) == (VT_VARIANT|VT_BYREF))
            v = V_VARIANTREF(v);

        if(own_val) {
            VariantClear(v);
            *v = *val;
            hres = S_OK;
        }else {
            hres = VariantCopy(v, val);
        }
        break;
    }
    case REF_DISP:
        hres = disp_propput(ctx->script, ref.u.d.disp, ref.u.d.id, val);
        if(own_val)
            VariantClear(val);
        break;
    case REF_FUNC:
        FIXME("functions not implemented\n");
        return E_NOTIMPL;
    case REF_OBJ:
        FIXME("REF_OBJ\n");
        return E_NOTIMPL;
    case REF_CONST:
        FIXME("REF_CONST\n");
        return E_NOTIMPL;
    case REF_NONE:
        if(ctx->func->code_ctx->option_explicit) {
            FIXME("throw exception\n");
            hres = E_FAIL;
        }else {
            TRACE("creating variable %s\n", debugstr_w(name));
            hres = add_dynamic_var(ctx, name, FALSE, val, own_val);
        }
    }

    return hres;
}

static HRESULT interp_assign_ident(exec_ctx_t *ctx)
{
    const BSTR arg = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    variant_val_t v;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    if(arg_cnt) {
        FIXME("arguments not supported\n");
        return E_NOTIMPL;
    }

    hres = stack_pop_val(ctx, &v);
    if(FAILED(hres))
        return hres;

    return assign_ident(ctx, arg, v.v, v.owned);
}

static HRESULT interp_set_ident(exec_ctx_t *ctx)
{
    const BSTR arg = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    IDispatch *disp;
    VARIANT v;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    if(arg_cnt) {
        FIXME("arguments not supported\n");
        return E_NOTIMPL;
    }

    hres = stack_pop_disp(ctx, &disp);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = disp;
    return assign_ident(ctx, ctx->instr->arg1.bstr, &v, TRUE);
}

static HRESULT interp_assign_member(exec_ctx_t *ctx)
{
    BSTR identifier = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    variant_val_t val;
    IDispatch *obj;
    DISPID id;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(identifier));

    if(arg_cnt) {
        FIXME("arguments not supported\n");
        return E_NOTIMPL;
    }

    hres = stack_pop_disp(ctx, &obj);
    if(FAILED(hres))
        return hres;

    if(!obj) {
        FIXME("NULL obj\n");
        return E_FAIL;
    }

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres)) {
        IDispatch_Release(obj);
        return hres;
    }

    hres = disp_get_id(obj, identifier, VBDISP_LET, FALSE, &id);
    if(SUCCEEDED(hres))
        hres = disp_propput(ctx->script, obj, id, val.v);

    release_val(&val);
    IDispatch_Release(obj);
    return hres;
}

static HRESULT interp_set_member(exec_ctx_t *ctx)
{
    BSTR identifier = ctx->instr->arg1.bstr;
    const unsigned arg_cnt = ctx->instr->arg2.uint;
    IDispatch *obj, *val;
    DISPID id;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(identifier));

    if(arg_cnt) {
        FIXME("arguments not supported\n");
        return E_NOTIMPL;
    }

    hres = stack_pop_disp(ctx, &obj);
    if(FAILED(hres))
        return hres;

    if(!obj) {
        FIXME("NULL obj\n");
        return E_FAIL;
    }

    hres = stack_pop_disp(ctx, &val);
    if(FAILED(hres)) {
        IDispatch_Release(obj);
        return hres;
    }

    hres = disp_get_id(obj, identifier, VBDISP_SET, FALSE, &id);
    if(SUCCEEDED(hres)) {
        VARIANT v;

        V_VT(&v) = VT_DISPATCH;
        V_DISPATCH(&v) = val;
        hres = disp_propput(ctx->script, obj, id, &v);
    }

    if(val)
        IDispatch_Release(val);
    IDispatch_Release(obj);
    return hres;
}

static HRESULT interp_const(exec_ctx_t *ctx)
{
    BSTR arg = ctx->instr->arg1.bstr;
    variant_val_t val;
    ref_t ref;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    assert(ctx->func->type == FUNC_GLOBAL);

    hres = lookup_identifier(ctx, arg, VBDISP_CALLGET, &ref);
    if(FAILED(hres))
        return hres;

    if(ref.type != REF_NONE) {
        FIXME("%s already defined\n", debugstr_w(arg));
        return E_FAIL;
    }

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    return add_dynamic_var(ctx, arg, TRUE, val.v, val.owned);
}

static HRESULT interp_val(exec_ctx_t *ctx)
{
    variant_val_t val;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    if(!val.owned) {
        V_VT(&v) = VT_EMPTY;
        hres = VariantCopy(&v, val.v);
        if(FAILED(hres))
            return hres;
    }

    return stack_push(ctx, val.owned ? val.v : &v);
}

static HRESULT interp_pop(exec_ctx_t *ctx)
{
    const unsigned n = ctx->instr->arg1.uint;

    TRACE("%u\n", n);

    stack_popn(ctx, n);
    return S_OK;
}

static HRESULT interp_new(exec_ctx_t *ctx)
{
    const WCHAR *arg = ctx->instr->arg1.bstr;
    class_desc_t *class_desc;
    vbdisp_t *obj;
    VARIANT v;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    for(class_desc = ctx->script->classes; class_desc; class_desc = class_desc->next) {
        if(!strcmpiW(class_desc->name, arg))
            break;
    }
    if(!class_desc) {
        FIXME("Class %s not found\n", debugstr_w(arg));
        return E_FAIL;
    }

    hres = create_vbdisp(class_desc, &obj);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = (IDispatch*)&obj->IDispatchEx_iface;
    return stack_push(ctx, &v);
}

static HRESULT interp_step(exec_ctx_t *ctx)
{
    const BSTR ident = ctx->instr->arg2.bstr;
    BOOL gteq_zero;
    VARIANT zero;
    ref_t ref;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(ident));

    V_VT(&zero) = VT_I2;
    V_I2(&zero) = 0;
    hres = VarCmp(stack_top(ctx, 0), &zero, ctx->script->lcid, 0);
    if(FAILED(hres))
        return hres;

    gteq_zero = hres == VARCMP_GT || hres == VARCMP_EQ;

    hres = lookup_identifier(ctx, ident, VBDISP_ANY, &ref);
    if(FAILED(hres))
        return hres;

    if(ref.type != REF_VAR) {
        FIXME("%s is not REF_VAR\n", debugstr_w(ident));
        return E_FAIL;
    }

    hres = VarCmp(ref.u.v, stack_top(ctx, 1), ctx->script->lcid, 0);
    if(FAILED(hres))
        return hres;

    if(hres == VARCMP_EQ || hres == (gteq_zero ? VARCMP_LT : VARCMP_GT))
        ctx->instr++;
    else
        instr_jmp(ctx, ctx->instr->arg1.uint);
    return S_OK;
}

static HRESULT interp_jmp(exec_ctx_t *ctx)
{
    const unsigned arg = ctx->instr->arg1.uint;

    TRACE("%u\n", arg);

    instr_jmp(ctx, arg);
    return S_OK;
}

static HRESULT interp_jmp_false(exec_ctx_t *ctx)
{
    const unsigned arg = ctx->instr->arg1.uint;
    variant_val_t val;
    HRESULT hres;

    TRACE("%u\n", arg);

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    if(V_VT(val.v) != VT_BOOL) {
        FIXME("unsupported for %s\n", debugstr_variant(val.v));
        release_val(&val);
        return E_NOTIMPL;
    }

    if(V_BOOL(val.v))
        ctx->instr++;
    else
        instr_jmp(ctx, ctx->instr->arg1.uint);
    return S_OK;
}

static HRESULT interp_jmp_true(exec_ctx_t *ctx)
{
    const unsigned arg = ctx->instr->arg1.uint;
    variant_val_t val;
    HRESULT hres;

    TRACE("%u\n", arg);

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    if(V_VT(val.v) != VT_BOOL) {
        FIXME("unsupported for %s\n", debugstr_variant(val.v));
        release_val(&val);
        return E_NOTIMPL;
    }

    if(V_BOOL(val.v))
        instr_jmp(ctx, ctx->instr->arg1.uint);
    else
        ctx->instr++;
    return S_OK;
}

static HRESULT interp_ret(exec_ctx_t *ctx)
{
    TRACE("\n");

    ctx->instr = NULL;
    return S_OK;
}

static HRESULT interp_stop(exec_ctx_t *ctx)
{
    WARN("\n");

    /* NOTE: this should have effect in debugging mode (that we don't support yet) */
    return S_OK;
}

static HRESULT interp_me(exec_ctx_t *ctx)
{
    VARIANT v;

    TRACE("\n");

    IDispatch_AddRef(ctx->this_obj);
    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = ctx->this_obj;
    return stack_push(ctx, &v);
}

static HRESULT interp_bool(exec_ctx_t *ctx)
{
    const VARIANT_BOOL arg = ctx->instr->arg1.lng;
    VARIANT v;

    TRACE("%s\n", arg ? "true" : "false");

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = arg;
    return stack_push(ctx, &v);
}

static HRESULT interp_errmode(exec_ctx_t *ctx)
{
    const int err_mode = ctx->instr->arg1.uint;

    TRACE("%d\n", err_mode);

    ctx->resume_next = err_mode;
    return S_OK;
}

static HRESULT interp_string(exec_ctx_t *ctx)
{
    VARIANT v;

    TRACE("\n");

    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(ctx->instr->arg1.str);
    if(!V_BSTR(&v))
        return E_OUTOFMEMORY;

    return stack_push(ctx, &v);
}

static HRESULT interp_long(exec_ctx_t *ctx)
{
    const LONG arg = ctx->instr->arg1.lng;
    VARIANT v;

    TRACE("%d\n", arg);

    V_VT(&v) = VT_I4;
    V_I4(&v) = arg;
    return stack_push(ctx, &v);
}

static HRESULT interp_short(exec_ctx_t *ctx)
{
    const LONG arg = ctx->instr->arg1.lng;
    VARIANT v;

    TRACE("%d\n", arg);

    V_VT(&v) = VT_I2;
    V_I2(&v) = arg;
    return stack_push(ctx, &v);
}

static HRESULT interp_double(exec_ctx_t *ctx)
{
    const DOUBLE *arg = ctx->instr->arg1.dbl;
    VARIANT v;

    TRACE("%lf\n", *arg);

    V_VT(&v) = VT_R8;
    V_R8(&v) = *arg;
    return stack_push(ctx, &v);
}

static HRESULT interp_empty(exec_ctx_t *ctx)
{
    VARIANT v;

    TRACE("\n");

    V_VT(&v) = VT_EMPTY;
    return stack_push(ctx, &v);
}

static HRESULT interp_null(exec_ctx_t *ctx)
{
    VARIANT v;

    TRACE("\n");

    V_VT(&v) = VT_NULL;
    return stack_push(ctx, &v);
}

static HRESULT interp_nothing(exec_ctx_t *ctx)
{
    VARIANT v;

    TRACE("\n");

    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = NULL;
    return stack_push(ctx, &v);
}

static HRESULT interp_not(exec_ctx_t *ctx)
{
    variant_val_t val;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    hres = VarNot(val.v, &v);
    release_val(&val);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_and(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarAnd(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_or(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarOr(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_xor(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarXor(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_eqv(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarEqv(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_imp(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarImp(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT cmp_oper(exec_ctx_t *ctx)
{
    variant_val_t l, r;
    HRESULT hres;

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        if(V_VT(l.v) == VT_NULL || V_VT(r.v) == VT_NULL) {
            FIXME("comparing nulls is not implemented\n");
            hres = E_NOTIMPL;
        }else {
            hres = VarCmp(l.v, r.v, ctx->script->lcid, 0);
        }
    }

    release_val(&r);
    release_val(&l);
    return hres;
}

static HRESULT interp_equal(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres == VARCMP_EQ ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT interp_nequal(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres != VARCMP_EQ ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT interp_gt(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres == VARCMP_GT ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT interp_gteq(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres == VARCMP_GT || hres == VARCMP_EQ ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT interp_lt(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres == VARCMP_LT ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT interp_lteq(exec_ctx_t *ctx)
{
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = cmp_oper(ctx);
    if(FAILED(hres))
        return hres;

    V_VT(&v) = VT_BOOL;
    V_BOOL(&v) = hres == VARCMP_LT || hres == VARCMP_EQ ? VARIANT_TRUE : VARIANT_FALSE;
    return stack_push(ctx, &v);
}

static HRESULT disp_cmp(IDispatch *disp1, IDispatch *disp2, VARIANT_BOOL *ret)
{
    IObjectIdentity *identity;
    IUnknown *unk1, *unk2;
    HRESULT hres;

    if(disp1 == disp2) {
        *ret = VARIANT_TRUE;
        return S_OK;
    }

    if(!disp1 || !disp2) {
        *ret = VARIANT_FALSE;
        return S_OK;
    }

    hres = IDispatch_QueryInterface(disp1, &IID_IUnknown, (void**)&unk1);
    if(FAILED(hres))
        return hres;

    hres = IDispatch_QueryInterface(disp2, &IID_IUnknown, (void**)&unk2);
    if(FAILED(hres)) {
        IUnknown_Release(unk1);
        return hres;
    }

    if(unk1 == unk2) {
        *ret = VARIANT_TRUE;
    }else {
        hres = IUnknown_QueryInterface(unk1, &IID_IObjectIdentity, (void**)&identity);
        if(SUCCEEDED(hres)) {
            hres = IObjectIdentity_IsEqualObject(identity, unk2);
            IObjectIdentity_Release(identity);
            *ret = hres == S_OK ? VARIANT_TRUE : VARIANT_FALSE;
        }else {
            *ret = VARIANT_FALSE;
        }
    }

    IUnknown_Release(unk1);
    IUnknown_Release(unk2);
    return S_OK;
}

static HRESULT interp_is(exec_ctx_t *ctx)
{
    IDispatch *l, *r;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_disp(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_disp(ctx, &l);
    if(SUCCEEDED(hres)) {
        V_VT(&v) = VT_BOOL;
        hres = disp_cmp(l, r, &V_BOOL(&v));
        if(l)
            IDispatch_Release(l);
    }
    if(r)
        IDispatch_Release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_concat(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarCat(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_add(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarAdd(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_sub(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarSub(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_mod(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarMod(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_idiv(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarIdiv(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_div(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarDiv(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_mul(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarMul(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_exp(exec_ctx_t *ctx)
{
    variant_val_t r, l;
    VARIANT v;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_val(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_val(ctx, &l);
    if(SUCCEEDED(hres)) {
        hres = VarPow(l.v, r.v, &v);
        release_val(&l);
    }
    release_val(&r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_neg(exec_ctx_t *ctx)
{
    variant_val_t val;
    VARIANT v;
    HRESULT hres;

    hres = stack_pop_val(ctx, &val);
    if(FAILED(hres))
        return hres;

    hres = VarNeg(val.v, &v);
    release_val(&val);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, &v);
}

static HRESULT interp_incc(exec_ctx_t *ctx)
{
    const BSTR ident = ctx->instr->arg1.bstr;
    VARIANT v;
    ref_t ref;
    HRESULT hres;

    TRACE("\n");

    hres = lookup_identifier(ctx, ident, VBDISP_LET, &ref);
    if(FAILED(hres))
        return hres;

    if(ref.type != REF_VAR) {
        FIXME("ref.type is not REF_VAR\n");
        return E_FAIL;
    }

    hres = VarAdd(stack_top(ctx, 0), ref.u.v, &v);
    if(FAILED(hres))
        return hres;

    VariantClear(ref.u.v);
    *ref.u.v = v;
    return S_OK;
}

static const instr_func_t op_funcs[] = {
#define X(x,n,a,b) interp_ ## x,
OP_LIST
#undef X
};

static const unsigned op_move[] = {
#define X(x,n,a,b) n,
OP_LIST
#undef X
};

void release_dynamic_vars(dynamic_var_t *var)
{
    while(var) {
        VariantClear(&var->v);
        var = var->next;
    }
}

static void release_exec(exec_ctx_t *ctx)
{
    unsigned i;

    VariantClear(&ctx->ret_val);
    release_dynamic_vars(ctx->dynamic_vars);

    if(ctx->this_obj)
        IDispatch_Release(ctx->this_obj);

    if(ctx->args) {
        for(i=0; i < ctx->func->arg_cnt; i++)
            VariantClear(ctx->args+i);
    }

    if(ctx->vars) {
        for(i=0; i < ctx->func->var_cnt; i++)
            VariantClear(ctx->vars+i);
    }

    vbsheap_free(&ctx->heap);
    heap_free(ctx->args);
    heap_free(ctx->vars);
    heap_free(ctx->stack);
}

HRESULT exec_script(script_ctx_t *ctx, function_t *func, IDispatch *this_obj, DISPPARAMS *dp, VARIANT *res)
{
    exec_ctx_t exec = {func->code_ctx};
    vbsop_t op;
    HRESULT hres = S_OK;

    exec.code = func->code_ctx;

    if(dp ? func->arg_cnt != arg_cnt(dp) : func->arg_cnt) {
        FIXME("wrong arg_cnt %d, expected %d\n", dp ? arg_cnt(dp) : 0, func->arg_cnt);
        return E_FAIL;
    }

    vbsheap_init(&exec.heap);

    if(func->arg_cnt) {
        VARIANT *v;
        unsigned i;

        exec.args = heap_alloc_zero(func->arg_cnt * sizeof(VARIANT));
        if(!exec.args) {
            release_exec(&exec);
            return E_OUTOFMEMORY;
        }

        for(i=0; i < func->arg_cnt; i++) {
            v = get_arg(dp, i);
            if(V_VT(v) == (VT_VARIANT|VT_BYREF)) {
                if(func->args[i].by_ref)
                    exec.args[i] = *v;
                else
                    hres = VariantCopy(exec.args+i, V_VARIANTREF(v));
            }else {
                hres = VariantCopy(exec.args+i, v);
            }
            if(FAILED(hres)) {
                release_exec(&exec);
                return hres;
            }
        }
    }else {
        exec.args = NULL;
    }

    if(func->var_cnt) {
        exec.vars = heap_alloc_zero(func->var_cnt * sizeof(VARIANT));
        if(!exec.vars) {
            release_exec(&exec);
            return E_OUTOFMEMORY;
        }
    }else {
        exec.vars = NULL;
    }

    exec.stack_size = 16;
    exec.top = 0;
    exec.stack = heap_alloc(exec.stack_size * sizeof(VARIANT));
    if(!exec.stack) {
        release_exec(&exec);
        return E_OUTOFMEMORY;
    }

    if(this_obj)
        exec.this_obj = this_obj;
    else if (ctx->host_global)
        exec.this_obj = ctx->host_global;
    else
        exec.this_obj = (IDispatch*)&ctx->script_obj->IDispatchEx_iface;
    IDispatch_AddRef(exec.this_obj);

    exec.instr = exec.code->instrs + func->code_off;
    exec.script = ctx;
    exec.func = func;

    while(exec.instr) {
        op = exec.instr->op;
        hres = op_funcs[op](&exec);
        if(FAILED(hres)) {
            if(exec.resume_next)
                FIXME("Failed %08x in resume next mode\n", hres);
            else
                WARN("Failed %08x\n", hres);
            stack_popn(&exec, exec.top);
            break;
        }

        exec.instr += op_move[op];
    }

    assert(!exec.top);
    if(func->type != FUNC_FUNCTION && func->type != FUNC_PROPGET && func->type != FUNC_DEFGET)
        assert(V_VT(&exec.ret_val) == VT_EMPTY);

    if(SUCCEEDED(hres) && res) {
        *res = exec.ret_val;
        V_VT(&exec.ret_val) = VT_EMPTY;
    }

    release_exec(&exec);
    return hres;
}
