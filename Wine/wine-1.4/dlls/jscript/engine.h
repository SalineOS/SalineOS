/*
 * Copyright 2008,2011 Jacek Caban for CodeWeavers
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

typedef struct _source_elements_t source_elements_t;
typedef struct _function_expression_t function_expression_t;
typedef struct _expression_t expression_t;
typedef struct _statement_t statement_t;

typedef struct _function_declaration_t {
    function_expression_t *expr;

    struct _function_declaration_t *next;
} function_declaration_t;

typedef struct _var_list_t {
    const WCHAR *identifier;

    struct _var_list_t *next;
} var_list_t;

typedef struct _func_stack {
    function_declaration_t *func_head;
    function_declaration_t *func_tail;
    var_list_t *var_head;
    var_list_t *var_tail;

    struct _func_stack *next;
} func_stack_t;

#define OP_LIST                            \
    X(add,        1, 0,0)                  \
    X(and,        1, 0,0)                  \
    X(array,      1, 0,0)                  \
    X(assign,     1, 0,0)                  \
    X(bool,       1, ARG_INT,    0)        \
    X(bneg,       1, 0,0)                  \
    X(call,       1, ARG_UINT,   ARG_UINT) \
    X(call_member,1, ARG_UINT,   ARG_UINT) \
    X(carray,     1, ARG_UINT,   0)        \
    X(case,       0, ARG_ADDR,   0)        \
    X(cnd_nz,     0, ARG_ADDR,   0)        \
    X(cnd_z,      0, ARG_ADDR,   0)        \
    X(delete,     1, 0,0)                  \
    X(delete_ident,1,ARG_BSTR,   0)        \
    X(div,        1, 0,0)                  \
    X(double,     1, ARG_DBL,    0)        \
    X(end_finally,1, 0,0)                  \
    X(eq,         1, 0,0)                  \
    X(eq2,        1, 0,0)                  \
    X(forin,      0, ARG_ADDR,   0)        \
    X(func,       1, ARG_FUNC,   0)        \
    X(gt,         1, 0,0)                  \
    X(gteq,       1, 0,0)                  \
    X(ident,      1, ARG_BSTR,   0)        \
    X(identid,    1, ARG_BSTR,   ARG_INT)  \
    X(in,         1, 0,0)                  \
    X(instanceof, 1, 0,0)                  \
    X(int,        1, ARG_INT,    0)        \
    X(jmp,        0, ARG_ADDR,   0)        \
    X(jmp_z,      0, ARG_ADDR,   0)        \
    X(lshift,     1, 0,0)                  \
    X(lt,         1, 0,0)                  \
    X(lteq,       1, 0,0)                  \
    X(member,     1, ARG_BSTR,   0)        \
    X(memberid,   1, ARG_UINT,   0)        \
    X(minus,      1, 0,0)                  \
    X(mod,        1, 0,0)                  \
    X(mul,        1, 0,0)                  \
    X(neg,        1, 0,0)                  \
    X(neq,        1, 0,0)                  \
    X(neq2,       1, 0,0)                  \
    X(new,        1, ARG_INT,    0)        \
    X(new_obj,    1, 0,0)                  \
    X(null,       1, 0,0)                  \
    X(obj_prop,   1, ARG_BSTR,   0)        \
    X(or,         1, 0,0)                  \
    X(pop,        1, 0,0)                  \
    X(pop_except, 1, 0,0)                  \
    X(pop_scope,  1, 0,0)                  \
    X(postinc,    1, ARG_INT,    0)        \
    X(preinc,     1, ARG_INT,    0)        \
    X(push_except,1, ARG_ADDR,   ARG_BSTR) \
    X(push_scope, 1, 0,0)                  \
    X(regexp,     1, ARG_STR,    ARG_INT)  \
    X(rshift,     1, 0,0)                  \
    X(rshift2,    1, 0,0)                  \
    X(str,        1, ARG_STR,    0)        \
    X(this,       1, 0,0)                  \
    X(throw,      0, 0,0)                  \
    X(throw_ref,  0, ARG_UINT,   0)        \
    X(throw_type, 0, ARG_UINT,   ARG_STR)  \
    X(tonum,      1, 0,0)                  \
    X(typeof,     1, 0,0)                  \
    X(typeofid,   1, 0,0)                  \
    X(typeofident,1, 0,0)                  \
    X(refval,     1, 0,0)                  \
    X(ret,        0, 0,0)                  \
    X(sub,        1, 0,0)                  \
    X(undefined,  1, 0,0)                  \
    X(var_set,    1, ARG_BSTR,   0)        \
    X(void,       1, 0,0)                  \
    X(xor,        1, 0,0)

typedef enum {
#define X(x,a,b,c) OP_##x,
OP_LIST
#undef X
    OP_LAST
} jsop_t;

typedef union {
    BSTR bstr;
    double *dbl;
    LONG lng;
    WCHAR *str;
    unsigned uint;
    function_expression_t *func; /* FIXME */
} instr_arg_t;

typedef enum {
    ARG_NONE = 0,
    ARG_ADDR,
    ARG_BSTR,
    ARG_DBL,
    ARG_FUNC,
    ARG_INT,
    ARG_STR,
    ARG_UINT
} instr_arg_type_t;

typedef struct {
    jsop_t op;
    instr_arg_t arg1;
    instr_arg_t arg2;
} instr_t;

typedef struct {
    instr_t *instrs;
    jsheap_t heap;

    BSTR *bstr_pool;
    unsigned bstr_pool_size;
    unsigned bstr_cnt;
} bytecode_t;

void release_bytecode(bytecode_t*);

typedef struct _compiler_ctx_t compiler_ctx_t;

void release_compiler(compiler_ctx_t*);

typedef struct _parser_ctx_t {
    LONG ref;

    WCHAR *begin;
    const WCHAR *end;
    const WCHAR *ptr;

    script_ctx_t *script;
    source_elements_t *source;
    BOOL nl;
    BOOL is_html;
    BOOL lexer_error;
    HRESULT hres;

    jsheap_t heap;

    func_stack_t *func_stack;

    bytecode_t *code;
    compiler_ctx_t *compiler;

    struct _parser_ctx_t *next;
} parser_ctx_t;

HRESULT script_parse(script_ctx_t*,const WCHAR*,const WCHAR*,parser_ctx_t**) DECLSPEC_HIDDEN;
void parser_release(parser_ctx_t*) DECLSPEC_HIDDEN;

int parser_lex(void*,parser_ctx_t*) DECLSPEC_HIDDEN;

static inline void parser_addref(parser_ctx_t *ctx)
{
    ctx->ref++;
}

static inline void *parser_alloc(parser_ctx_t *ctx, DWORD size)
{
    return jsheap_alloc(&ctx->heap, size);
}

static inline void *parser_alloc_tmp(parser_ctx_t *ctx, DWORD size)
{
    return jsheap_alloc(&ctx->script->tmp_heap, size);
}

typedef struct _scope_chain_t {
    LONG ref;
    jsdisp_t *obj;
    struct _scope_chain_t *next;
} scope_chain_t;

HRESULT scope_push(scope_chain_t*,jsdisp_t*,scope_chain_t**) DECLSPEC_HIDDEN;
void scope_release(scope_chain_t*) DECLSPEC_HIDDEN;

static inline void scope_addref(scope_chain_t *scope)
{
    scope->ref++;
}

typedef struct _except_frame_t except_frame_t;

struct _exec_ctx_t {
    LONG ref;

    parser_ctx_t *parser;
    scope_chain_t *scope_chain;
    jsdisp_t *var_disp;
    IDispatch *this_obj;
    BOOL is_global;

    VARIANT *stack;
    unsigned stack_size;
    unsigned top;
    except_frame_t *except_frame;

    unsigned ip;
    jsexcept_t *ei;
};

static inline void exec_addref(exec_ctx_t *ctx)
{
    ctx->ref++;
}

void exec_release(exec_ctx_t*) DECLSPEC_HIDDEN;
HRESULT create_exec_ctx(script_ctx_t*,IDispatch*,jsdisp_t*,scope_chain_t*,BOOL,exec_ctx_t**) DECLSPEC_HIDDEN;
HRESULT exec_source(exec_ctx_t*,parser_ctx_t*,source_elements_t*,BOOL,jsexcept_t*,VARIANT*) DECLSPEC_HIDDEN;

typedef struct _parameter_t parameter_t;

HRESULT create_source_function(parser_ctx_t*,parameter_t*,source_elements_t*,scope_chain_t*,
        const WCHAR*,DWORD,jsdisp_t**) DECLSPEC_HIDDEN;

typedef enum {
    LT_INT,
    LT_DOUBLE,
    LT_STRING,
    LT_BOOL,
    LT_NULL,
    LT_REGEXP
}literal_type_t;

typedef struct {
    literal_type_t type;
    union {
        LONG lval;
        double dval;
        const WCHAR *wstr;
        VARIANT_BOOL bval;
        struct {
            const WCHAR *str;
            DWORD str_len;
            DWORD flags;
        } regexp;
    } u;
} literal_t;

literal_t *parse_regexp(parser_ctx_t*) DECLSPEC_HIDDEN;
literal_t *new_boolean_literal(parser_ctx_t*,VARIANT_BOOL) DECLSPEC_HIDDEN;

typedef struct _variable_declaration_t {
    const WCHAR *identifier;
    expression_t *expr;

    struct _variable_declaration_t *next;
} variable_declaration_t;

typedef enum {
    STAT_BLOCK,
    STAT_BREAK,
    STAT_CONTINUE,
    STAT_EMPTY,
    STAT_EXPR,
    STAT_FOR,
    STAT_FORIN,
    STAT_IF,
    STAT_LABEL,
    STAT_RETURN,
    STAT_SWITCH,
    STAT_THROW,
    STAT_TRY,
    STAT_VAR,
    STAT_WHILE,
    STAT_WITH
} statement_type_t;

struct _statement_t {
    statement_type_t type;
    statement_t *next;
};

typedef struct {
    statement_t stat;
    statement_t *stat_list;
} block_statement_t;

typedef struct {
    statement_t stat;
    variable_declaration_t *variable_list;
} var_statement_t;

typedef struct {
    statement_t stat;
    expression_t *expr;
} expression_statement_t;

typedef struct {
    statement_t stat;
    expression_t *expr;
    statement_t *if_stat;
    statement_t *else_stat;
} if_statement_t;

typedef struct {
    statement_t stat;
    BOOL do_while;
    expression_t *expr;
    statement_t *statement;
} while_statement_t;

typedef struct {
    statement_t stat;
    variable_declaration_t *variable_list;
    expression_t *begin_expr;
    expression_t *expr;
    expression_t *end_expr;
    statement_t *statement;
} for_statement_t;

typedef struct {
    statement_t stat;
    variable_declaration_t *variable;
    expression_t *expr;
    expression_t *in_expr;
    statement_t *statement;
} forin_statement_t;

typedef struct {
    statement_t stat;
    const WCHAR *identifier;
} branch_statement_t;

typedef struct {
    statement_t stat;
    expression_t *expr;
    statement_t *statement;
} with_statement_t;

typedef struct {
    statement_t stat;
    const WCHAR *identifier;
    statement_t *statement;
} labelled_statement_t;

typedef struct _case_clausule_t {
    expression_t *expr;
    statement_t *stat;

    struct _case_clausule_t *next;
} case_clausule_t;

typedef struct {
    statement_t stat;
    expression_t *expr;
    case_clausule_t *case_list;
} switch_statement_t;

typedef struct {
    const WCHAR *identifier;
    statement_t *statement;
} catch_block_t;

typedef struct {
    statement_t stat;
    statement_t *try_statement;
    catch_block_t *catch_block;
    statement_t *finally_statement;
} try_statement_t;

typedef struct {
    enum {
        EXPRVAL_VARIANT,
        EXPRVAL_IDREF,
        EXPRVAL_INVALID
    } type;
    union {
        VARIANT var;
        struct {
            IDispatch *disp;
            DISPID id;
        } idref;
    } u;
} exprval_t;

typedef enum {
     EXPR_COMMA,
     EXPR_OR,
     EXPR_AND,
     EXPR_BOR,
     EXPR_BXOR,
     EXPR_BAND,
     EXPR_INSTANCEOF,
     EXPR_IN,
     EXPR_ADD,
     EXPR_SUB,
     EXPR_MUL,
     EXPR_DIV,
     EXPR_MOD,
     EXPR_DELETE,
     EXPR_VOID,
     EXPR_TYPEOF,
     EXPR_MINUS,
     EXPR_PLUS,
     EXPR_POSTINC,
     EXPR_POSTDEC,
     EXPR_PREINC,
     EXPR_PREDEC,
     EXPR_EQ,
     EXPR_EQEQ,
     EXPR_NOTEQ,
     EXPR_NOTEQEQ,
     EXPR_LESS,
     EXPR_LESSEQ,
     EXPR_GREATER,
     EXPR_GREATEREQ,
     EXPR_BITNEG,
     EXPR_LOGNEG,
     EXPR_LSHIFT,
     EXPR_RSHIFT,
     EXPR_RRSHIFT,
     EXPR_ASSIGN,
     EXPR_ASSIGNLSHIFT,
     EXPR_ASSIGNRSHIFT,
     EXPR_ASSIGNRRSHIFT,
     EXPR_ASSIGNADD,
     EXPR_ASSIGNSUB,
     EXPR_ASSIGNMUL,
     EXPR_ASSIGNDIV,
     EXPR_ASSIGNMOD,
     EXPR_ASSIGNAND,
     EXPR_ASSIGNOR,
     EXPR_ASSIGNXOR,
     EXPR_COND,
     EXPR_ARRAY,
     EXPR_MEMBER,
     EXPR_NEW,
     EXPR_CALL,
     EXPR_THIS,
     EXPR_FUNC,
     EXPR_IDENT,
     EXPR_ARRAYLIT,
     EXPR_PROPVAL,
     EXPR_LITERAL
} expression_type_t;

struct _expression_t {
    expression_type_t type;
};

struct _parameter_t {
    const WCHAR *identifier;

    struct _parameter_t *next;
};

struct _source_elements_t {
    statement_t *statement;
    statement_t *statement_tail;
    function_declaration_t *functions;
    var_list_t *variables;
    unsigned instr_off;
};

struct _function_expression_t {
    expression_t expr;
    const WCHAR *identifier;
    parameter_t *parameter_list;
    source_elements_t *source_elements;
    const WCHAR *src_str;
    DWORD src_len;
};

typedef struct {
    expression_t expr;
    expression_t *expression1;
    expression_t *expression2;
} binary_expression_t;

typedef struct {
    expression_t expr;
    expression_t *expression;
} unary_expression_t;

typedef struct {
    expression_t expr;
    expression_t *expression;
    expression_t *true_expression;
    expression_t *false_expression;
} conditional_expression_t;

typedef struct {
    expression_t expr;
    expression_t *expression;
    const WCHAR *identifier;
} member_expression_t;

typedef struct _argument_t {
    expression_t *expr;

    struct _argument_t *next;
} argument_t;

typedef struct {
    expression_t expr;
    expression_t *expression;
    argument_t *argument_list;
} call_expression_t;

typedef struct {
    expression_t expr;
    const WCHAR *identifier;
} identifier_expression_t;

typedef struct {
    expression_t expr;
    literal_t *literal;
} literal_expression_t;

typedef struct _array_element_t {
    int elision;
    expression_t *expr;

    struct _array_element_t *next;
} array_element_t;

typedef struct {
    expression_t expr;
    array_element_t *element_list;
    int length;
} array_literal_expression_t;

typedef struct _prop_val_t {
    literal_t *name;
    expression_t *value;

    struct _prop_val_t *next;
} prop_val_t;

typedef struct {
    expression_t expr;
    prop_val_t *property_list;
} property_value_expression_t;

HRESULT compile_subscript_stat(parser_ctx_t*,statement_t*,BOOL,unsigned*) DECLSPEC_HIDDEN;
