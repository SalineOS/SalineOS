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
#include <stdio.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "dispex.h"
#include "activscp.h"

#include "resource.h"

#include "wine/unicode.h"
#include "wine/list.h"

typedef struct _script_ctx_t script_ctx_t;
typedef struct _exec_ctx_t exec_ctx_t;
typedef struct _dispex_prop_t dispex_prop_t;

typedef struct {
    EXCEPINFO ei;
    VARIANT var;
} jsexcept_t;

typedef struct {
    void **blocks;
    DWORD block_cnt;
    DWORD last_block;
    DWORD offset;
    BOOL mark;
    struct list custom_blocks;
} jsheap_t;

void jsheap_init(jsheap_t*) DECLSPEC_HIDDEN;
void *jsheap_alloc(jsheap_t*,DWORD) __WINE_ALLOC_SIZE(2) DECLSPEC_HIDDEN;
void *jsheap_grow(jsheap_t*,void*,DWORD,DWORD) DECLSPEC_HIDDEN;
void jsheap_clear(jsheap_t*) DECLSPEC_HIDDEN;
void jsheap_free(jsheap_t*) DECLSPEC_HIDDEN;
jsheap_t *jsheap_mark(jsheap_t*) DECLSPEC_HIDDEN;

typedef struct jsdisp_t jsdisp_t;

extern HINSTANCE jscript_hinstance DECLSPEC_HIDDEN;

#define PROPF_ARGMASK 0x00ff
#define PROPF_METHOD  0x0100
#define PROPF_ENUM    0x0200
#define PROPF_CONSTR  0x0400
#define PROPF_CONST   0x0800

/* NOTE: Keep in sync with names in Object.toString implementation */
typedef enum {
    JSCLASS_NONE,
    JSCLASS_ARRAY,
    JSCLASS_BOOLEAN,
    JSCLASS_DATE,
    JSCLASS_ERROR,
    JSCLASS_FUNCTION,
    JSCLASS_GLOBAL,
    JSCLASS_MATH,
    JSCLASS_NUMBER,
    JSCLASS_OBJECT,
    JSCLASS_REGEXP,
    JSCLASS_STRING,
    JSCLASS_ARGUMENTS,
    JSCLASS_VBARRAY
} jsclass_t;

jsdisp_t *iface_to_jsdisp(IUnknown*) DECLSPEC_HIDDEN;

typedef struct {
    union {
        IDispatch *disp;
        IDispatchEx *dispex;
        jsdisp_t *jsdisp;
    } u;
    DWORD flags;
} vdisp_t;

#define VDISP_DISPEX  0x0001
#define VDISP_JSDISP  0x0002

static inline void vdisp_release(vdisp_t *vdisp)
{
    IDispatch_Release(vdisp->u.disp);
}

static inline BOOL is_jsdisp(vdisp_t *vdisp)
{
    return (vdisp->flags & VDISP_JSDISP) != 0;
}

static inline BOOL is_dispex(vdisp_t *vdisp)
{
    return (vdisp->flags & VDISP_DISPEX) != 0;
}

static inline void set_jsdisp(vdisp_t *vdisp, jsdisp_t *jsdisp)
{
    vdisp->u.jsdisp = jsdisp;
    vdisp->flags = VDISP_JSDISP | VDISP_DISPEX;
    IDispatch_AddRef(vdisp->u.disp);
}

static inline void set_disp(vdisp_t *vdisp, IDispatch *disp)
{
    IDispatchEx *dispex;
    jsdisp_t *jsdisp;
    HRESULT hres;

    jsdisp = iface_to_jsdisp((IUnknown*)disp);
    if(jsdisp) {
        vdisp->u.jsdisp = jsdisp;
        vdisp->flags = VDISP_JSDISP | VDISP_DISPEX;
        return;
    }

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres)) {
        vdisp->u.dispex = dispex;
        vdisp->flags = VDISP_DISPEX;
        return;
    }

    IDispatch_AddRef(disp);
    vdisp->u.disp = disp;
    vdisp->flags = 0;
}

static inline jsdisp_t *get_jsdisp(vdisp_t *vdisp)
{
    return is_jsdisp(vdisp) ? vdisp->u.jsdisp : NULL;
}

typedef HRESULT (*builtin_invoke_t)(script_ctx_t*,vdisp_t*,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*);

typedef struct {
    const WCHAR *name;
    builtin_invoke_t invoke;
    DWORD flags;
} builtin_prop_t;

typedef struct {
    jsclass_t class;
    builtin_prop_t value_prop;
    DWORD props_cnt;
    const builtin_prop_t *props;
    void (*destructor)(jsdisp_t*);
    void (*on_put)(jsdisp_t*,const WCHAR*);
} builtin_info_t;

struct jsdisp_t {
    IDispatchEx IDispatchEx_iface;

    LONG ref;

    DWORD buf_size;
    DWORD prop_cnt;
    dispex_prop_t *props;
    script_ctx_t *ctx;

    jsdisp_t *prototype;

    const builtin_info_t *builtin_info;
};

static inline IDispatch *to_disp(jsdisp_t *jsdisp)
{
    return (IDispatch*)&jsdisp->IDispatchEx_iface;
}

jsdisp_t *as_jsdisp(IDispatch*) DECLSPEC_HIDDEN;
jsdisp_t *to_jsdisp(IDispatch*) DECLSPEC_HIDDEN;

static inline void jsdisp_addref(jsdisp_t *jsdisp)
{
    IDispatchEx_AddRef(&jsdisp->IDispatchEx_iface);
}

static inline void jsdisp_release(jsdisp_t *jsdisp)
{
    IDispatchEx_Release(&jsdisp->IDispatchEx_iface);
}

HRESULT create_dispex(script_ctx_t*,const builtin_info_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT init_dispex(jsdisp_t*,script_ctx_t*,const builtin_info_t*,jsdisp_t*) DECLSPEC_HIDDEN;
HRESULT init_dispex_from_constr(jsdisp_t*,script_ctx_t*,const builtin_info_t*,jsdisp_t*) DECLSPEC_HIDDEN;

HRESULT disp_call(script_ctx_t*,IDispatch*,DISPID,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_call_value(jsdisp_t*,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_call(jsdisp_t*,DISPID,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_call_name(jsdisp_t*,const WCHAR*,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT disp_propget(script_ctx_t*,IDispatch*,DISPID,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT disp_propput(script_ctx_t*,IDispatch*,DISPID,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_propget(jsdisp_t*,DISPID,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_propput_name(jsdisp_t*,const WCHAR*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_propput_const(jsdisp_t*,const WCHAR*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT jsdisp_propput_idx(jsdisp_t*,DWORD,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_propget_name(jsdisp_t*,LPCWSTR,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_get_idx(jsdisp_t*,DWORD,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT jsdisp_get_id(jsdisp_t*,const WCHAR*,DWORD,DISPID*) DECLSPEC_HIDDEN;
HRESULT jsdisp_delete_idx(jsdisp_t*,DWORD) DECLSPEC_HIDDEN;
VARIANT_BOOL jsdisp_is_own_prop(jsdisp_t *obj, BSTR name) DECLSPEC_HIDDEN;

HRESULT create_builtin_function(script_ctx_t*,builtin_invoke_t,const WCHAR*,const builtin_info_t*,DWORD,
        jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT Function_value(script_ctx_t*,vdisp_t*,WORD,DISPPARAMS*,VARIANT*,jsexcept_t*,IServiceProvider*) DECLSPEC_HIDDEN;

HRESULT throw_eval_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_generic_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_range_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_reference_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_regexp_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_syntax_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_type_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT throw_uri_error(script_ctx_t*,jsexcept_t*,HRESULT,const WCHAR*) DECLSPEC_HIDDEN;

HRESULT create_object(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_math(script_ctx_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_array(script_ctx_t*,DWORD,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_regexp(script_ctx_t*,const WCHAR *,int,DWORD,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_regexp_var(script_ctx_t*,VARIANT*,VARIANT*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_string(script_ctx_t*,const WCHAR*,DWORD,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_bool(script_ctx_t*,VARIANT_BOOL,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_number(script_ctx_t*,VARIANT*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_vbarray(script_ctx_t*,SAFEARRAY*,jsdisp_t**) DECLSPEC_HIDDEN;

typedef enum {
    NO_HINT,
    HINT_STRING,
    HINT_NUMBER
} hint_t;

HRESULT to_primitive(script_ctx_t*,VARIANT*,jsexcept_t*,VARIANT*, hint_t) DECLSPEC_HIDDEN;
HRESULT to_boolean(VARIANT*,VARIANT_BOOL*) DECLSPEC_HIDDEN;
HRESULT to_number(script_ctx_t*,VARIANT*,jsexcept_t*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT to_integer(script_ctx_t*,VARIANT*,jsexcept_t*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT to_int32(script_ctx_t*,VARIANT*,jsexcept_t*,INT*) DECLSPEC_HIDDEN;
HRESULT to_uint32(script_ctx_t*,VARIANT*,jsexcept_t*,DWORD*) DECLSPEC_HIDDEN;
HRESULT to_string(script_ctx_t*,VARIANT*,jsexcept_t*,BSTR*) DECLSPEC_HIDDEN;
HRESULT to_object(script_ctx_t*,VARIANT*,IDispatch**) DECLSPEC_HIDDEN;

BSTR int_to_bstr(int) DECLSPEC_HIDDEN;
HRESULT double_to_bstr(double,BSTR*) DECLSPEC_HIDDEN;

typedef struct named_item_t {
    IDispatch *disp;
    DWORD flags;
    LPWSTR name;

    struct named_item_t *next;
} named_item_t;

typedef struct _cc_var_t cc_var_t;

typedef struct {
    cc_var_t *vars;
} cc_ctx_t;

void release_cc(cc_ctx_t*) DECLSPEC_HIDDEN;

struct _script_ctx_t {
    LONG ref;

    SCRIPTSTATE state;
    exec_ctx_t *exec_ctx;
    named_item_t *named_items;
    IActiveScriptSite *site;
    IInternetHostSecurityManager *secmgr;
    DWORD safeopt;
    DWORD version;
    LCID lcid;
    cc_ctx_t *cc;

    jsheap_t tmp_heap;

    IDispatch *host_global;

    BSTR last_match;
    DWORD last_match_index;
    DWORD last_match_length;

    jsdisp_t *global;
    jsdisp_t *function_constr;
    jsdisp_t *activex_constr;
    jsdisp_t *array_constr;
    jsdisp_t *bool_constr;
    jsdisp_t *date_constr;
    jsdisp_t *error_constr;
    jsdisp_t *eval_error_constr;
    jsdisp_t *range_error_constr;
    jsdisp_t *reference_error_constr;
    jsdisp_t *regexp_error_constr;
    jsdisp_t *syntax_error_constr;
    jsdisp_t *type_error_constr;
    jsdisp_t *uri_error_constr;
    jsdisp_t *number_constr;
    jsdisp_t *object_constr;
    jsdisp_t *regexp_constr;
    jsdisp_t *string_constr;
    jsdisp_t *vbarray_constr;
};

void script_release(script_ctx_t*) DECLSPEC_HIDDEN;

static inline void script_addref(script_ctx_t *ctx)
{
    ctx->ref++;
}

HRESULT init_global(script_ctx_t*) DECLSPEC_HIDDEN;
HRESULT init_function_constr(script_ctx_t*,jsdisp_t*) DECLSPEC_HIDDEN;
HRESULT create_object_prototype(script_ctx_t*,jsdisp_t**) DECLSPEC_HIDDEN;

HRESULT create_activex_constr(script_ctx_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_array_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_bool_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_date_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT init_error_constr(script_ctx_t*,jsdisp_t*) DECLSPEC_HIDDEN;
HRESULT create_number_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_object_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_regexp_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_string_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;
HRESULT create_vbarray_constr(script_ctx_t*,jsdisp_t*,jsdisp_t**) DECLSPEC_HIDDEN;

IUnknown *create_ax_site(script_ctx_t*) DECLSPEC_HIDDEN;

typedef struct {
    const WCHAR *str;
    DWORD len;
} match_result_t;

#define REM_CHECK_GLOBAL   0x0001
#define REM_RESET_INDEX    0x0002
#define REM_NO_CTX_UPDATE  0x0004
HRESULT regexp_match_next(script_ctx_t*,jsdisp_t*,DWORD,const WCHAR*,DWORD,const WCHAR**,match_result_t**,
        DWORD*,DWORD*,match_result_t*) DECLSPEC_HIDDEN;
HRESULT regexp_match(script_ctx_t*,jsdisp_t*,const WCHAR*,DWORD,BOOL,match_result_t**,DWORD*) DECLSPEC_HIDDEN;
HRESULT parse_regexp_flags(const WCHAR*,DWORD,DWORD*) DECLSPEC_HIDDEN;
HRESULT regexp_string_match(script_ctx_t*,jsdisp_t*,BSTR,VARIANT*,jsexcept_t*) DECLSPEC_HIDDEN;

static inline VARIANT *get_arg(DISPPARAMS *dp, DWORD i)
{
    return dp->rgvarg + dp->cArgs-i-1;
}

static inline DWORD arg_cnt(const DISPPARAMS *dp)
{
    return dp->cArgs - dp->cNamedArgs;
}

static inline BOOL is_class(jsdisp_t *jsdisp, jsclass_t class)
{
    return jsdisp->builtin_info->class == class;
}

static inline BOOL is_vclass(vdisp_t *vdisp, jsclass_t class)
{
    return is_jsdisp(vdisp) && is_class(vdisp->u.jsdisp, class);
}

static inline BOOL is_num_vt(enum VARENUM vt)
{
    return vt == VT_I4 || vt == VT_R8;
}

static inline DOUBLE num_val(const VARIANT *v)
{
    return V_VT(v) == VT_I4 ? V_I4(v) : V_R8(v);
}

static inline void num_set_int(VARIANT *v, INT i)
{
    V_VT(v) = VT_I4;
    V_I4(v) = i;
}

static inline void num_set_val(VARIANT *v, DOUBLE d)
{
    if(d == (DOUBLE)(INT)d) {
        V_VT(v) = VT_I4;
        V_I4(v) = d;
    }else {
        V_VT(v) = VT_R8;
        V_R8(v) = d;
    }
}

static inline void num_set_nan(VARIANT *v)
{
    V_VT(v) = VT_R8;
#ifdef NAN
    V_R8(v) = NAN;
#else
    V_UI8(v) = (ULONGLONG)0x7ff80000<<32;
#endif
}

static inline DOUBLE ret_nan(void)
{
    VARIANT v;
    num_set_nan(&v);
    return V_R8(&v);
}

static inline void num_set_inf(VARIANT *v, BOOL positive)
{
    V_VT(v) = VT_R8;
#ifdef INFINITY
    V_R8(v) = positive ? INFINITY : -INFINITY;
#else
    V_UI8(v) = (ULONGLONG)0x7ff00000<<32;
    if(!positive)
        V_R8(v) = -V_R8(v);
#endif
}

static inline void var_set_jsdisp(VARIANT *v, jsdisp_t *jsdisp)
{
    V_VT(v) = VT_DISPATCH;
    V_DISPATCH(v) = to_disp(jsdisp);
}

static inline DWORD make_grfdex(script_ctx_t *ctx, DWORD flags)
{
    return (ctx->version << 28) | flags;
}

#define FACILITY_JSCRIPT 10

#define MAKE_JSERROR(code) MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSCRIPT, code)

#define JS_E_TO_PRIMITIVE            MAKE_JSERROR(IDS_TO_PRIMITIVE)
#define JS_E_INVALIDARG              MAKE_JSERROR(IDS_INVALID_CALL_ARG)
#define JS_E_SUBSCRIPT_OUT_OF_RANGE  MAKE_JSERROR(IDS_SUBSCRIPT_OUT_OF_RANGE)
#define JS_E_OBJECT_REQUIRED         MAKE_JSERROR(IDS_OBJECT_REQUIRED)
#define JS_E_CANNOT_CREATE_OBJ       MAKE_JSERROR(IDS_CREATE_OBJ_ERROR)
#define JS_E_INVALID_PROPERTY        MAKE_JSERROR(IDS_NO_PROPERTY)
#define JS_E_INVALID_ACTION          MAKE_JSERROR(IDS_UNSUPPORTED_ACTION)
#define JS_E_MISSING_ARG             MAKE_JSERROR(IDS_ARG_NOT_OPT)
#define JS_E_SYNTAX                  MAKE_JSERROR(IDS_SYNTAX_ERROR)
#define JS_E_MISSING_SEMICOLON       MAKE_JSERROR(IDS_SEMICOLON)
#define JS_E_MISSING_LBRACKET        MAKE_JSERROR(IDS_LBRACKET)
#define JS_E_MISSING_RBRACKET        MAKE_JSERROR(IDS_RBRACKET)
#define JS_E_UNTERMINATED_STRING     MAKE_JSERROR(IDS_UNTERMINATED_STR)
#define JS_E_INVALID_BREAK           MAKE_JSERROR(IDS_INVALID_BREAK)
#define JS_E_INVALID_CONTINUE        MAKE_JSERROR(IDS_INVALID_CONTINUE)
#define JS_E_LABEL_REDEFINED         MAKE_JSERROR(IDS_LABEL_REDEFINED)
#define JS_E_LABEL_NOT_FOUND         MAKE_JSERROR(IDS_LABEL_NOT_FOUND)
#define JS_E_DISABLED_CC             MAKE_JSERROR(IDS_DISABLED_CC)
#define JS_E_FUNCTION_EXPECTED       MAKE_JSERROR(IDS_NOT_FUNC)
#define JS_E_DATE_EXPECTED           MAKE_JSERROR(IDS_NOT_DATE)
#define JS_E_NUMBER_EXPECTED         MAKE_JSERROR(IDS_NOT_NUM)
#define JS_E_OBJECT_EXPECTED         MAKE_JSERROR(IDS_OBJECT_EXPECTED)
#define JS_E_ILLEGAL_ASSIGN          MAKE_JSERROR(IDS_ILLEGAL_ASSIGN)
#define JS_E_UNDEFINED_VARIABLE      MAKE_JSERROR(IDS_UNDEFINED)
#define JS_E_BOOLEAN_EXPECTED        MAKE_JSERROR(IDS_NOT_BOOL)
#define JS_E_VBARRAY_EXPECTED        MAKE_JSERROR(IDS_NOT_VBARRAY)
#define JS_E_INVALID_DELETE          MAKE_JSERROR(IDS_INVALID_DELETE)
#define JS_E_JSCRIPT_EXPECTED        MAKE_JSERROR(IDS_JSCRIPT_EXPECTED)
#define JS_E_REGEXP_SYNTAX           MAKE_JSERROR(IDS_REGEXP_SYNTAX_ERROR)
#define JS_E_INVALID_URI_CODING      MAKE_JSERROR(IDS_URI_INVALID_CODING)
#define JS_E_INVALID_URI_CHAR        MAKE_JSERROR(IDS_URI_INVALID_CHAR)
#define JS_E_INVALID_LENGTH          MAKE_JSERROR(IDS_INVALID_LENGTH)
#define JS_E_ARRAY_EXPECTED          MAKE_JSERROR(IDS_ARRAY_EXPECTED)

static inline BOOL is_jscript_error(HRESULT hres)
{
    return HRESULT_FACILITY(hres) == FACILITY_JSCRIPT;
}

const char *debugstr_variant(const VARIANT*) DECLSPEC_HIDDEN;

HRESULT WINAPI JScriptFactory_CreateInstance(IClassFactory*,IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;

extern LONG module_ref DECLSPEC_HIDDEN;

static inline void lock_module(void)
{
    InterlockedIncrement(&module_ref);
}

static inline void unlock_module(void)
{
    InterlockedDecrement(&module_ref);
}

static inline void *heap_alloc(size_t len)
{
    return HeapAlloc(GetProcessHeap(), 0, len);
}

static inline void *heap_alloc_zero(size_t len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}

static inline void *heap_realloc(void *mem, size_t len)
{
    return HeapReAlloc(GetProcessHeap(), 0, mem, len);
}

static inline BOOL heap_free(void *mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}

static inline LPWSTR heap_strdupW(LPCWSTR str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD size;

        size = (strlenW(str)+1)*sizeof(WCHAR);
        ret = heap_alloc(size);
        memcpy(ret, str, size);
    }

    return ret;
}
