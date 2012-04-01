/*
 * Copyright 2011 Piotr Caban for CodeWeavers
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

#include <stdarg.h>
#include <stdio.h>

#include "msvcp90.h"
#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(msvcp90);

typedef enum {
    IOSTATE_goodbit   = 0x00,
    IOSTATE_eofbit    = 0x01,
    IOSTATE_failbit   = 0x02,
    IOSTATE_badbit    = 0x04,
    IOSTATE__Hardfail = 0x10
} IOSB_iostate;

typedef enum {
    FMTFLAG_skipws      = 0x0001,
    FMTFLAG_unitbuf     = 0x0002,
    FMTFLAG_uppercase   = 0x0004,
    FMTFLAG_showbase    = 0x0008,
    FMTFLAG_showpoint   = 0x0010,
    FMTFLAG_showpos     = 0x0020,
    FMTFLAG_left        = 0x0040,
    FMTFLAG_right       = 0x0080,
    FMTFLAG_internal    = 0x0100,
    FMTFLAG_dec         = 0x0200,
    FMTFLAG_oct         = 0x0400,
    FMTFLAG_hex         = 0x0800,
    FMTFLAG_scientific  = 0x1000,
    FMTFLAG_fixed       = 0x2000,
    FMTFLAG_hexfloat    = 0x3000,
    FMTFLAG_boolalpha   = 0x4000,
    FMTFLAG_stdio       = 0x8000,
    FMTFLAG_adjustfield = FMTFLAG_left|FMTFLAG_right|FMTFLAG_internal,
    FMTFLAG_basefield   = FMTFLAG_dec|FMTFLAG_oct|FMTFLAG_hex,
    FMTFLAG_floadfield  = FMTFLAG_scientific|FMTFLAG_fixed
} IOSB_fmtflags;

typedef struct _iosarray {
    struct _iosarray *next;
    int index;
    int long_val;
    void *ptr_val;
} IOS_BASE_iosarray;

typedef enum {
    EVENT_erase_event,
    EVENT_imbue_event,
    EVENT_copyfmt_event
} IOS_BASE_event;

struct _ios_base;
typedef void (CDECL *IOS_BASE_event_callback)(IOS_BASE_event, struct _ios_base*, int);
typedef struct _fnarray {
    struct _fnarray *next;
    int index;
    IOS_BASE_event_callback event_handler;
} IOS_BASE_fnarray;

/* ?_Index@ios_base@std@@0HA */
int ios_base_Index = 0;
/* ?_Sync@ios_base@std@@0_NA */
MSVCP_bool ios_base_Sync = FALSE;

typedef struct _ios_base {
    const vtable_ptr *vtable;
    MSVCP_size_t stdstr;
    IOSB_iostate state;
    IOSB_iostate except;
    IOSB_fmtflags fmtfl;
    MSVCP_size_t prec;
    MSVCP_size_t wide;
    IOS_BASE_iosarray *arr;
    IOS_BASE_fnarray *calls;
    locale *loc;
} ios_base;

typedef struct {
    streamoff off;
    __int64 DECLSPEC_ALIGN(8) pos;
    int state;
} fpos_int;

static inline const char* debugstr_fpos_int(fpos_int *fpos)
{
    return wine_dbg_sprintf("fpos(%lu %s %d)", fpos->off, wine_dbgstr_longlong(fpos->pos), fpos->state);
}

typedef struct {
    const vtable_ptr *vtable;
    mutex lock;
    char *rbuf;
    char *wbuf;
    char **prbuf;
    char **pwbuf;
    char *rpos;
    char *wpos;
    char **prpos;
    char **pwpos;
    int rsize;
    int wsize;
    int *prsize;
    int *pwsize;
    locale *loc;
} basic_streambuf_char;

void __thiscall basic_streambuf_char__Init_empty(basic_streambuf_char*);
void __thiscall basic_streambuf_char_setp(basic_streambuf_char*, char*, char*);
void __thiscall basic_streambuf_char_setg(basic_streambuf_char*, char*, char*, char*);

typedef struct {
    ios_base child;
    basic_streambuf_char *strbuf;
    struct basic_ostream_char *stream;
    char fillch;
} basic_ios_char;

typedef struct {
    ios_base child;
    /*basic_streambuf_wchar*/void *strbuf;
    /*basic_ostream_wchar*/void *stream;
    char fillch;
} basic_ios_wchar;

typedef struct {
    const vtable_ptr *vtable;
    basic_ios_char child;
} basic_ostream_char;

/* ??_7ios_base@std@@6B@ */
extern const vtable_ptr MSVCP_ios_base_vtable;

/* ??_7?$basic_ios@DU?$char_traits@D@std@@@std@@6B@ */
extern const vtable_ptr MSVCP_basic_ios_char_vtable;

/* ??_7?$basic_ios@_WU?$char_traits@_W@std@@@std@@6B@ */
extern const vtable_ptr MSVCP_basic_ios_wchar_vtable;

/* ??_7?$basic_streambuf@DU?$char_traits@D@std@@@std@@6B@ */
extern const vtable_ptr MSVCP_basic_streambuf_char_vtable;

/* ??_7?$basic_ostream@DU?$char_traits@D@std@@@std@@6B@ */
extern const vtable_ptr MSVCP_basic_ostream_char_vtable;

static const type_info iosb_type_info = {
    &MSVCP_ios_base_vtable,
    NULL,
    ".?AV?$_Iosb@H@std@@"
};
    
static const rtti_base_descriptor iosb_rtti_base_descriptor = {
    &iosb_type_info,
    0,
    { 4, -1, 0 },
    64
};

DEFINE_RTTI_DATA(ios_base, 0, 1, &iosb_rtti_base_descriptor, NULL, NULL, ".?AV?$_Iosb@H@std@@");
DEFINE_RTTI_DATA(basic_ios_char, 0, 2, &ios_base_rtti_base_descriptor, &iosb_rtti_base_descriptor,
        NULL, ".?AV?$basic_ios@DU?$char_traits@D@std@@@std@@");
DEFINE_RTTI_DATA(basic_ios_wchar, 0, 2, &ios_base_rtti_base_descriptor, &iosb_rtti_base_descriptor,
        NULL, ".?AV?$basic_ios@_WU?$char_traits@_W@std@@@std@@");
DEFINE_RTTI_DATA(basic_streambuf_char, 0, 0, NULL, NULL, NULL,
        ".?AV?$basic_streambuf@DU?$char_traits@D@std@@@std@@");
DEFINE_RTTI_DATA(basic_ostream_char, 4, 3, &basic_ios_char_rtti_base_descriptor, &ios_base_rtti_base_descriptor,
        &iosb_rtti_base_descriptor, ".?AV?$basic_ostream@DU?$char_traits@D@std@@@std@@");

#ifndef __GNUC__
void __asm_dummy_vtables(void) {
#endif
    __ASM_VTABLE(ios_base, "");
    __ASM_VTABLE(basic_ios_char, "");
    __ASM_VTABLE(basic_ios_wchar, "");
    __ASM_VTABLE(basic_streambuf_char,
            VTABLE_ADD_FUNC(basic_streambuf_char_overflow)
            VTABLE_ADD_FUNC(basic_streambuf_char_pbackfail)
            VTABLE_ADD_FUNC(basic_streambuf_char_showmanyc)
            VTABLE_ADD_FUNC(basic_streambuf_char_underflow)
            VTABLE_ADD_FUNC(basic_streambuf_char_uflow)
            VTABLE_ADD_FUNC(basic_streambuf_char_xsgetn)
            VTABLE_ADD_FUNC(basic_streambuf_char__Xsgetn_s)
            VTABLE_ADD_FUNC(basic_streambuf_char_xsputn)
            VTABLE_ADD_FUNC(basic_streambuf_char_seekoff)
            VTABLE_ADD_FUNC(basic_streambuf_char_seekpos)
            VTABLE_ADD_FUNC(basic_streambuf_char_setbuf)
            VTABLE_ADD_FUNC(basic_streambuf_char_sync)
            VTABLE_ADD_FUNC(basic_streambuf_char_imbue));
    __ASM_VTABLE(basic_ostream_char, "");
#ifndef __GNUC__
}
#endif

/* ??0ios_base@std@@IAE@XZ */
/* ??0ios_base@std@@IEAA@XZ */
DEFINE_THISCALL_WRAPPER(ios_base_ctor, 4)
ios_base* __thiscall ios_base_ctor(ios_base *this)
{
    FIXME("(%p) stub\n", this);

    this->vtable = &MSVCP_ios_base_vtable;
    return NULL;
}

/* ??0ios_base@std@@QAE@ABV01@@Z */
/* ??0ios_base@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(ios_base_copy_ctor, 8)
ios_base* __thiscall ios_base_copy_ctor(ios_base *this, const ios_base *copy)
{
    FIXME("(%p %p) stub\n", this, copy);
    return NULL;
}

/* ??1ios_base@std@@UAE@XZ */
/* ??1ios_base@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(ios_base_dtor, 4)
void __thiscall ios_base_dtor(ios_base *this)
{
    FIXME("(%p) stub\n", this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_ios_base_vector_dtor, 8)
ios_base* __thiscall MSVCP_ios_base_vector_dtor(ios_base *this, unsigned int flags)
{
    TRACE("(%p %x) stub\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)this-1;

        for(i=*ptr-1; i>=0; i--)
            ios_base_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        ios_base_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??4ios_base@std@@QAEAAV01@ABV01@@Z */
/* ??4ios_base@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(ios_base_assign, 8)
ios_base* __thiscall ios_base_assign(ios_base *this, const ios_base *right)
{
    FIXME("(%p %p) stub\n", this, right);
    return NULL;
}

/* ??7ios_base@std@@QBE_NXZ */
/* ??7ios_base@std@@QEBA_NXZ */
DEFINE_THISCALL_WRAPPER(ios_base_op_succ, 4)
MSVCP_bool __thiscall ios_base_op_succ(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return FALSE;
}

/* ??Bios_base@std@@QBEPAXXZ */
/* ??Bios_base@std@@QEBAPEAXXZ */
DEFINE_THISCALL_WRAPPER(ios_base_op_fail, 4)
void* __thiscall ios_base_op_fail(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?_Addstd@ios_base@std@@SAXPAV12@@Z */
/* ?_Addstd@ios_base@std@@SAXPEAV12@@Z */
void CDECL ios_base_Addstd(ios_base *add)
{
    FIXME("(%p) stub\n", add);
}

/* ?_Callfns@ios_base@std@@AAEXW4event@12@@Z */
/* ?_Callfns@ios_base@std@@AEAAXW4event@12@@Z */
DEFINE_THISCALL_WRAPPER(ios_base_Callfns, 8)
void __thiscall ios_base_Callfns(ios_base *this, IOS_BASE_event event)
{
    FIXME("(%p %x) stub\n", this, event);
}

/* ?_Findarr@ios_base@std@@AAEAAU_Iosarray@12@H@Z */
/* ?_Findarr@ios_base@std@@AEAAAEAU_Iosarray@12@H@Z */
DEFINE_THISCALL_WRAPPER(ios_base_Findarr, 8)
IOS_BASE_iosarray* __thiscall ios_base_Findarr(ios_base *this, int index)
{
    FIXME("(%p %d) stub\n", this, index);
    return NULL;
}

/* ?_Index_func@ios_base@std@@CAAAHXZ */
/* ?_Index_func@ios_base@std@@CAAEAHXZ */
int* CDECL ios_base_Index_func(void)
{
    TRACE("\n");
    return &ios_base_Index;
}

/* ?_Init@ios_base@std@@IAEXXZ */
/* ?_Init@ios_base@std@@IEAAXXZ */
DEFINE_THISCALL_WRAPPER(ios_base_Init, 4)
void __thiscall ios_base_Init(ios_base *this)
{
    FIXME("(%p) stub\n", this);
}

/* ?_Ios_base_dtor@ios_base@std@@CAXPAV12@@Z */
/* ?_Ios_base_dtor@ios_base@std@@CAXPEAV12@@Z */
void CDECL ios_base_Ios_base_dtor(ios_base *obj)
{
    FIXME("(%p) stub\n", obj);
}

/* ?_Sync_func@ios_base@std@@CAAA_NXZ */
/* ?_Sync_func@ios_base@std@@CAAEA_NXZ */
MSVCP_bool* CDECL ios_base_Sync_func(void)
{
    TRACE("\n");
    return &ios_base_Sync;
}

/* ?_Tidy@ios_base@std@@AAAXXZ */
/* ?_Tidy@ios_base@std@@AEAAXXZ */
void CDECL ios_base_Tidy(void)
{
    FIXME("stub\n");
}

/* ?bad@ios_base@std@@QBE_NXZ */
/* ?bad@ios_base@std@@QEBA_NXZ */
DEFINE_THISCALL_WRAPPER(ios_base_bad, 4)
MSVCP_bool __thiscall ios_base_bad(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return FALSE;
}

/* ?clear@ios_base@std@@QAEXH_N@Z */
/* ?clear@ios_base@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(ios_base_clear_reraise, 12)
void __thiscall ios_base_clear_reraise(ios_base *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?clear@ios_base@std@@QAEXH@Z */
/* ?clear@ios_base@std@@QEAAXH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_clear, 8)
void __thiscall ios_base_clear(ios_base *this, IOSB_iostate state)
{
    ios_base_clear_reraise(this, state, FALSE);
}

/* ?clear@ios_base@std@@QAEXI@Z */
/* ?clear@ios_base@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(ios_base_clear_unsigned, 8)
void __thiscall ios_base_clear_unsigned(ios_base *this, unsigned int state)
{
    ios_base_clear_reraise(this, (IOSB_iostate)state, FALSE);
}

/* ?copyfmt@ios_base@std@@QAEAAV12@ABV12@@Z */
/* ?copyfmt@ios_base@std@@QEAAAEAV12@AEBV12@@Z */
DEFINE_THISCALL_WRAPPER(ios_base_copyfmt, 8)
ios_base* __thiscall ios_base_copyfmt(ios_base *this, const ios_base *obj)
{
    FIXME("(%p %p) stub\n", this, obj);
    return NULL;
}

/* ?eof@ios_base@std@@QBE_NXZ */
/* ?eof@ios_base@std@@QEBA_NXZ */
DEFINE_THISCALL_WRAPPER(ios_base_eof, 4)
MSVCP_bool __thiscall ios_base_eof(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return FALSE;
}

/* ?exceptions@ios_base@std@@QAEXH@Z */
/* ?exceptions@ios_base@std@@QEAAXH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_exception_set, 8)
void __thiscall ios_base_exception_set(ios_base *this, IOSB_iostate state)
{
    FIXME("(%p %x) stub\n", this, state);
}

/* ?exceptions@ios_base@std@@QAEXI@Z */
/* ?exceptions@ios_base@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(ios_base_exception_set_unsigned, 8)
void __thiscall ios_base_exception_set_unsigned(ios_base *this, unsigned int state)
{
    FIXME("(%p %x) stub\n", this, state);
}

/* ?exceptions@ios_base@std@@QBEHXZ */
/* ?exceptions@ios_base@std@@QEBAHXZ */
DEFINE_THISCALL_WRAPPER(ios_base_exception_get, 4)
IOSB_iostate __thiscall ios_base_exception_get(ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?fail@ios_base@std@@QBE_NXZ */
/* ?fail@ios_base@std@@QEBA_NXZ */
DEFINE_THISCALL_WRAPPER(ios_base_fail, 4)
MSVCP_bool __thiscall ios_base_fail(ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?flags@ios_base@std@@QAEHH@Z */
/* ?flags@ios_base@std@@QEAAHH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_flags_set, 8)
IOSB_fmtflags __thiscall ios_base_flags_set(ios_base *this, IOSB_fmtflags flags)
{
    FIXME("(%p %x) stub\n", this, flags);
    return 0;
}

/* ?flags@ios_base@std@@QBEHXZ */
/* ?flags@ios_base@std@@QEBAHXZ */
DEFINE_THISCALL_WRAPPER(ios_base_flags_get, 4)
IOSB_fmtflags __thiscall ios_base_flags_get(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?getloc@ios_base@std@@QBE?AVlocale@2@XZ */
/* ?getloc@ios_base@std@@QEBA?AVlocale@2@XZ */
DEFINE_THISCALL_WRAPPER(ios_base_getloc, 8)
locale* __thiscall ios_base_getloc(const ios_base *this, locale *ret)
{
    FIXME("(%p) stub\n", this);
    return ret;
}

/* ?good@ios_base@std@@QBE_NXZ */
/* ?good@ios_base@std@@QEBA_NXZ */
DEFINE_THISCALL_WRAPPER(ios_base_good, 4)
MSVCP_bool __thiscall ios_base_good(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return FALSE;
}

/* ?imbue@ios_base@std@@QAE?AVlocale@2@ABV32@@Z */
/* ?imbue@ios_base@std@@QEAA?AVlocale@2@AEBV32@@Z */
DEFINE_THISCALL_WRAPPER(ios_base_imbue, 12)
locale* __thiscall ios_base_imbue(ios_base *this, locale *ret, const locale *loc)
{
    FIXME("(%p %p) stub\n", this, loc);
    return ret;
}

/* ?iword@ios_base@std@@QAEAAJH@Z */
/* ?iword@ios_base@std@@QEAAAEAJH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_iword, 8)
LONG* __thiscall ios_base_iword(ios_base *this, int index)
{
    FIXME("(%p %d) stub\n", this, index);
    return NULL;
}

/* ?precision@ios_base@std@@QAEHH@Z */
/* ?precision@ios_base@std@@QEAA_J_J@Z */
DEFINE_THISCALL_WRAPPER(ios_base_precision_set, 8)
MSVCP_size_t __thiscall ios_base_precision_set(ios_base *this, MSVCP_size_t precision)
{
    FIXME("(%p %lu) stub\n", this, precision);
    return 0;
}

/* ?precision@ios_base@std@@QBEHXZ */
/* ?precision@ios_base@std@@QEBA_JXZ */
DEFINE_THISCALL_WRAPPER(ios_base_precision_get, 4)
MSVCP_size_t __thiscall ios_base_precision_get(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?pword@ios_base@std@@QAEAAPAXH@Z */
/* ?pword@ios_base@std@@QEAAAEAPEAXH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_pword, 8)
void** __thiscall ios_base_pword(ios_base *this, int index)
{
    FIXME("(%p %d) stub\n", this, index);
    return NULL;
}

/* ?rdstate@ios_base@std@@QBEHXZ */
/* ?rdstate@ios_base@std@@QEBAHXZ */
DEFINE_THISCALL_WRAPPER(ios_base_rdstate, 4)
IOSB_iostate __thiscall ios_base_rdstate(const ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?register_callback@ios_base@std@@QAEXP6AXW4event@12@AAV12@H@ZH@Z */
/* ?register_callback@ios_base@std@@QEAAXP6AXW4event@12@AEAV12@H@ZH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_register_callback, 12)
void __thiscall ios_base_register_callback(ios_base *this, IOS_BASE_event_callback callback, int index)
{
    FIXME("(%p %p %d) stub\n", this, callback, index);
}

/* ?setf@ios_base@std@@QAEHHH@Z */
/* ?setf@ios_base@std@@QEAAHHH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_setf_mask, 12)
IOSB_fmtflags __thiscall ios_base_setf_mask(ios_base *this, IOSB_fmtflags flags, IOSB_fmtflags mask)
{
    FIXME("(%p %x %x) stub\n", this, flags, mask);
    return 0;
}

/* ?setf@ios_base@std@@QAEHH@Z */
/* ?setf@ios_base@std@@QEAAHH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_setf, 8)
IOSB_fmtflags __thiscall ios_base_setf(ios_base *this, IOSB_fmtflags flags)
{
    return ios_base_setf_mask(this, flags, ~0);
}

/* ?setstate@ios_base@std@@QAEXH_N@Z */
/* ?setstate@ios_base@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(ios_base_setstate_reraise, 12)
void __thiscall ios_base_setstate_reraise(ios_base *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?setstate@ios_base@std@@QAEXH@Z */
/* ?setstate@ios_base@std@@QEAAXH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_setstate, 8)
void __thiscall ios_base_setstate(ios_base *this, IOSB_iostate state)
{
    ios_base_setstate_reraise(this, state, FALSE);
}

/* ?setstate@ios_base@std@@QAEXI@Z */
/* ?setstate@ios_base@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(ios_base_setstate_unsigned, 8)
void __thiscall ios_base_setstate_unsigned(ios_base *this, unsigned int state)
{
    ios_base_setstate_reraise(this, (IOSB_iostate)state, FALSE);
}

/* ?sync_with_stdio@ios_base@std@@SA_N_N@Z */
MSVCP_bool CDECL ios_base_sync_with_stdio(MSVCP_bool sync)
{
    FIXME("(%x) stub\n", sync);
    return FALSE;
}

/* ?unsetf@ios_base@std@@QAEXH@Z */
/* ?unsetf@ios_base@std@@QEAAXH@Z */
DEFINE_THISCALL_WRAPPER(ios_base_unsetf, 8)
void __thiscall ios_base_unsetf(ios_base *this, IOSB_fmtflags flags)
{
    FIXME("(%p %x) stub\n", this, flags);
}

/* ?width@ios_base@std@@QAEHH@Z */
/* ?width@ios_base@std@@QEAA_J_J@Z */
DEFINE_THISCALL_WRAPPER(ios_base_width_set, 8)
MSVCP_size_t __thiscall ios_base_width_set(ios_base *this, MSVCP_size_t width)
{
    FIXME("(%p %lu) stub\n", this, width);
    return 0;
}

/* ?width@ios_base@std@@QBEHXZ */
/* ?width@ios_base@std@@QEBA_JXZ */
DEFINE_THISCALL_WRAPPER(ios_base_width_get, 4)
MSVCP_size_t __thiscall ios_base_width_get(ios_base *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?xalloc@ios_base@std@@SAHXZ */
int CDECL ios_base_xalloc(void)
{
    FIXME("stub\n");
    return 0;
}

/* ??0?$basic_ios@DU?$char_traits@D@std@@@std@@IAE@XZ */
/* ??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_char_ctor, 4)
basic_ios_char* __thiscall basic_ios_char_ctor(basic_ios_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ??0?$basic_ios@DU?$char_traits@D@std@@@std@@QAE@PAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z */
/* ??0?$basic_ios@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_copy_ctor, 8)
basic_ios_char* __thiscall basic_ios_char_copy_ctor(basic_ios_char *this, const basic_ios_char *copy)
{
    FIXME("(%p %p) stub\n", this, copy);
    return NULL;
}

/* ??1?$basic_ios@DU?$char_traits@D@std@@@std@@UAE@XZ */
/* ??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_char_dtor, 4)
void __thiscall basic_ios_char_dtor(basic_ios_char *this)
{
    FIXME("(%p) stub\n", this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_basic_ios_char_vector_dtor, 8)
basic_ios_char* __thiscall MSVCP_basic_ios_char_vector_dtor(basic_ios_char *this, unsigned int flags)
{
    TRACE("(%p %x) stub\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)this-1;

        for(i=*ptr-1; i>=0; i--)
            basic_ios_char_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        basic_ios_char_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXH_N@Z */
/* ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_clear_reraise, 12)
void __thiscall basic_ios_char_clear_reraise(basic_ios_char *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXI@Z */
/* ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_clear, 8)
void __thiscall basic_ios_char_clear(basic_ios_char *this, unsigned int state)
{
    basic_ios_char_clear_reraise(this, (IOSB_iostate)state, FALSE);
}

/* ?copyfmt@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEAAV12@ABV12@@Z */
/* ?copyfmt@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAAEAV12@AEBV12@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_copyfmt, 8)
basic_ios_char* __thiscall basic_ios_char_copyfmt(basic_ios_char *this, basic_ios_char *copy)
{
    FIXME("(%p %p) stub\n", this, copy);
    return NULL;
}

/* ?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEDD@Z */
/* ?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAADD@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_fill_set, 8)
char __thiscall basic_ios_char_fill_set(basic_ios_char *this, char fill)
{
    FIXME("(%p %c) stub\n", this, fill);
    return 0;
}

/* ?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QBEDXZ */
/* ?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADXZ */
DEFINE_THISCALL_WRAPPER(basic_ios_char_fill_get, 4)
char __thiscall basic_ios_char_fill_get(basic_ios_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?imbue@?$basic_ios@DU?$char_traits@D@std@@@std@@QAE?AVlocale@2@ABV32@@Z */
/* ?imbue@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_imbue, 12)
locale *__thiscall basic_ios_char_imbue(basic_ios_char *this, locale *ret, const locale *loc)
{
    FIXME("(%p %p %p) stub\n", this, ret, loc);
    return ret;
}

/* ?init@?$basic_ios@DU?$char_traits@D@std@@@std@@IAEXPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@_N@Z */
/* ?init@?$basic_ios@DU?$char_traits@D@std@@@std@@IEAAXPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_init, 12)
void __thiscall basic_ios_char_init(basic_ios_char *this, basic_streambuf_char *streambuf, MSVCP_bool isstd)
{
    FIXME("(%p %p %x) stub\n", this, streambuf, isstd);
}

/* ?narrow@?$basic_ios@DU?$char_traits@D@std@@@std@@QBEDDD@Z */
/* ?narrow@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADDD@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_narrow, 12)
char __thiscall basic_ios_char_narrow(basic_ios_char *this, char ch, char def)
{
    FIXME("(%p %c %c) stub\n", this, ch, def);
    return def;
}

/* ?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@PAV32@@Z */
/* ?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@PEAV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_rdbuf_set, 8)
basic_streambuf_char* __thiscall basic_ios_char_rdbuf_set(basic_ios_char *this, basic_streambuf_char *streambuf)
{
    FIXME("(%p %p) stub\n", this, streambuf);
    return NULL;
}

/* ?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QBEPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ */
/* ?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_char_rdbuf_get, 4)
basic_streambuf_char* __thiscall basic_ios_char_rdbuf_get(const basic_ios_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXH_N@Z */
/* ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_setstate_reraise, 12)
void __thiscall basic_ios_char_setstate_reraise(basic_ios_char *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXI@Z */
/* ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_setstate, 8)
void __thiscall basic_ios_char_setstate(basic_ios_char *this, unsigned int state)
{
    basic_ios_char_setstate_reraise(this, (IOSB_iostate)state, FALSE);
}

/* ?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEPAV?$basic_ostream@DU?$char_traits@D@std@@@2@PAV32@@Z */
/* ?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAPEAV?$basic_ostream@DU?$char_traits@D@std@@@2@PEAV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_tie_set, 8)
basic_ostream_char* __thiscall basic_ios_char_tie_set(basic_ios_char *this, basic_ostream_char *ostream)
{
    FIXME("(%p %p) stub\n", this, ostream);
    return NULL;
}

/* ?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QBEPAV?$basic_ostream@DU?$char_traits@D@std@@@2@XZ */
/* ?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_ostream@DU?$char_traits@D@std@@@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_char_tie_get, 4)
basic_ostream_char* __thiscall basic_ios_char_tie_get(const basic_ios_char *this)
{
    FIXME("(%p)\n", this);
    return NULL;
}

/* ?widen@?$basic_ios@DU?$char_traits@D@std@@@std@@QBEDD@Z */
/* ?widen@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADD@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_char_widen, 8)
char __thiscall basic_ios_char_widen(basic_ios_char *this, char ch)
{
    FIXME("(%p %c)\n", this, ch);
    return 0;
}


/* ??0?$basic_ios@_WU?$char_traits@_W@std@@@std@@IAE@XZ */
/* ??0?$basic_ios@_WU?$char_traits@_W@std@@@std@@IEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_ctor, 4)
basic_ios_wchar* __thiscall basic_ios_wchar_ctor(basic_ios_wchar *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ??0?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAE@PAV?$basic_streambuf@_WU?$char_traits@_W@std@@@1@@Z */
/* ??0?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAA@PEAV?$basic_streambuf@_WU?$char_traits@_W@std@@@1@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_copy_ctor, 8)
basic_ios_wchar* __thiscall basic_ios_wchar_copy_ctor(basic_ios_wchar *this, const basic_ios_wchar *copy)
{
    FIXME("(%p %p) stub\n", this, copy);
    return NULL;
}

/* ??1?$basic_ios@_WU?$char_traits@_W@std@@@std@@UAE@XZ */
/* ??1?$basic_ios@_WU?$char_traits@_W@std@@@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_dtor, 4)
void __thiscall basic_ios_wchar_dtor(basic_ios_wchar *this)
{
    FIXME("(%p) stub\n", this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_basic_ios_wchar_vector_dtor, 8)
basic_ios_wchar* __thiscall MSVCP_basic_ios_wchar_vector_dtor(basic_ios_wchar *this, unsigned int flags)
{
    TRACE("(%p %x) stub\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)this-1;

        for(i=*ptr-1; i>=0; i--)
            basic_ios_wchar_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        basic_ios_wchar_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXH_N@Z */
/* ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_clear_reraise, 12)
void __thiscall basic_ios_wchar_clear_reraise(basic_ios_wchar *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXI@Z */
/* ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_clear, 8)
void __thiscall basic_ios_wchar_clear(basic_ios_wchar *this, unsigned int state)
{
    basic_ios_wchar_clear_reraise(this, (IOSB_iostate)state, FALSE);
}

/* ?copyfmt@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEAAV12@ABV12@@Z */
/* ?copyfmt@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAAEAV12@AEBV12@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_copyfmt, 8)
basic_ios_wchar* __thiscall basic_ios_wchar_copyfmt(basic_ios_wchar *this, basic_ios_wchar *copy)
{
    FIXME("(%p %p) stub\n", this, copy);
    return NULL;
}

/* ?fill@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAE_W_W@Z */
/* ?fill@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAA_W_W@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_fill_set, 8)
wchar_t __thiscall basic_ios_wchar_fill_set(basic_ios_wchar *this, wchar_t fill)
{
    FIXME("(%p %c) stub\n", this, fill);
    return 0;
}

/* ?fill@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QBE_WXZ */
/* ?fill@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEBA_WXZ */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_fill_get, 4)
wchar_t __thiscall basic_ios_wchar_fill_get(basic_ios_wchar *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?imbue@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAE?AVlocale@2@ABV32@@Z */
/* ?imbue@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_imbue, 12)
locale *__thiscall basic_ios_wchar_imbue(basic_ios_wchar *this, locale *ret, const locale *loc)
{
    FIXME("(%p %p %p) stub\n", this, ret, loc);
    return ret;
}

/* ?init@?$basic_ios@_WU?$char_traits@_W@std@@@std@@IAEXPAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@_N@Z */
/* ?init@?$basic_ios@_WU?$char_traits@_W@std@@@std@@IEAAXPEAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_init, 12)
void __thiscall basic_ios_wchar_init(basic_ios_wchar *this, /*basic_streambuf_wchar*/void *streambuf, MSVCP_bool isstd)
{
    FIXME("(%p %p %x) stub\n", this, streambuf, isstd);
}

/* ?narrow@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QBED_WD@Z */
/* ?narrow@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEBAD_WD@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_narrow, 12)
char __thiscall basic_ios_wchar_narrow(basic_ios_wchar *this, wchar_t ch, char def)
{
    FIXME("(%p %c %c) stub\n", this, ch, def);
    return def;
}

/* ?rdbuf@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEPAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@PAV32@@Z */
/* ?rdbuf@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAPEAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@PEAV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_rdbuf_set, 8)
/*basic_streambuf_wchar*/void* __thiscall basic_ios_wchar_rdbuf_set(basic_ios_wchar *this, /*basic_streambuf_wchar*/void *streambuf)
{
    FIXME("(%p %p) stub\n", this, streambuf);
    return NULL;
}

/* ?rdbuf@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QBEPAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@XZ */
/* ?rdbuf@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEBAPEAV?$basic_streambuf@_WU?$char_traits@_W@std@@@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_rdbuf_get, 4)
/*basic_streambuf_wchar*/void* __thiscall basic_ios_wchar_rdbuf_get(const basic_ios_wchar *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?setstate@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXH_N@Z */
/* ?setstate@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAXH_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_setstate_reraise, 12)
void __thiscall basic_ios_wchar_setstate_reraise(basic_ios_wchar *this, IOSB_iostate state, MSVCP_bool reraise)
{
    FIXME("(%p %x %x) stub\n", this, state, reraise);
}

/* ?setstate@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXI@Z */
/* ?setstate@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAXI@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_setstate, 8)
void __thiscall basic_ios_wchar_setstate(basic_ios_wchar *this, IOSB_iostate state)
{
    basic_ios_wchar_setstate_reraise(this, state, FALSE);
}

/* ?tie@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEPAV?$basic_ostream@_WU?$char_traits@_W@std@@@2@PAV32@@Z */
/* ?tie@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAAPEAV?$basic_ostream@_WU?$char_traits@_W@std@@@2@PEAV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_tie_set, 8)
/*basic_ostream_wchar*/void* __thiscall basic_ios_wchar_tie_set(basic_ios_wchar *this, /*basic_ostream_wchar*/void *ostream)
{
    FIXME("(%p %p) stub\n", this, ostream);
    return NULL;
}

/* ?tie@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QBEPAV?$basic_ostream@_WU?$char_traits@_W@std@@@2@XZ */
/* ?tie@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEBAPEAV?$basic_ostream@_WU?$char_traits@_W@std@@@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_tie_get, 4)
/*basic_ostream_wchar*/void* __thiscall basic_ios_wchar_tie_get(const basic_ios_wchar *this)
{
    FIXME("(%p)\n", this);
    return NULL;
}

/* ?widen@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QBE_WD@Z */
/* ?widen@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEBA_WD@Z */
DEFINE_THISCALL_WRAPPER(basic_ios_wchar_widen, 8)
wchar_t __thiscall basic_ios_wchar_widen(basic_ios_wchar *this, char ch)
{
    FIXME("(%p %c)\n", this, ch);
    return 0;
}

/* ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAE@W4_Uninitialized@1@@Z */
/* ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@W4_Uninitialized@1@@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_ctor_uninitialized, 8)
basic_streambuf_char* __thiscall basic_streambuf_char_ctor_uninitialized(basic_streambuf_char *this, int uninitialized)
{
    TRACE("(%p %d)\n", this, uninitialized);
    this->vtable = &MSVCP_basic_streambuf_char_vtable;
    mutex_ctor(&this->lock);
    return this;
}

/* ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAE@XZ */
/* ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_ctor, 4)
basic_streambuf_char* __thiscall basic_streambuf_char_ctor(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);

    this->vtable = &MSVCP_basic_streambuf_char_vtable;
    mutex_ctor(&this->lock);
    this->loc = MSVCRT_operator_new(sizeof(locale));
    locale_ctor(this->loc);
    basic_streambuf_char__Init_empty(this);

    return this;
}

/* ??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UAE@XZ */
/* ??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_dtor, 4)
void __thiscall basic_streambuf_char_dtor(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);

    mutex_dtor(&this->lock);
    locale_dtor(this->loc);
    MSVCRT_operator_delete(this->loc);
}

DEFINE_THISCALL_WRAPPER(MSVCP_basic_streambuf_char_vector_dtor, 8)
basic_streambuf_char* __thiscall MSVCP_basic_streambuf_char_vector_dtor(basic_streambuf_char *this, unsigned int flags)
{
    TRACE("(%p %x)\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)this-1;

        for(i=*ptr-1; i>=0; i--)
            basic_streambuf_char_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        basic_streambuf_char_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ?_Gnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEHXZ */
/* ?_Gnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBA_JXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Gnavail, 4)
streamsize __thiscall basic_streambuf_char__Gnavail(const basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    return *this->prpos ? *this->prsize : 0;
}

/* ?_Gndec@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEPADXZ */
/* ?_Gndec@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Gndec, 4)
char* __thiscall basic_streambuf_char__Gndec(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    (*this->prsize)++;
    (*this->prpos)--;
    return *this->prpos;
}

/* ?_Gninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEPADXZ */
/* ?_Gninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Gninc, 4)
char* __thiscall basic_streambuf_char__Gninc(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    (*this->prsize)--;
    return (*this->prpos)++;
}

/* ?_Gnpreinc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEPADXZ */
/* ?_Gnpreinc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Gnpreinc, 4)
char* __thiscall basic_streambuf_char__Gnpreinc(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    (*this->prsize)--;
    (*this->prpos)++;
    return *this->prpos;
}

/* ?_Init@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXPAPAD0PAH001@Z */
/* ?_Init@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAPEAD0PEAH001@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Init, 28)
void __thiscall basic_streambuf_char__Init(basic_streambuf_char *this, char **gf, char **gn, int *gc, char **pf, char **pn, int *pc)
{
    TRACE("(%p %p %p %p %p %p %p)\n", this, gf, gn, gc, pf, pn, pc);

    this->prbuf = gf;
    this->pwbuf = pf;
    this->prpos = gn;
    this->pwpos = pn;
    this->prsize = gc;
    this->pwsize = pc;
}

/* ?_Init@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXXZ */
/* ?_Init@?$basic_streambuf@GU?$char_traits@G@std@@@std@@IEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Init_empty, 4)
void __thiscall basic_streambuf_char__Init_empty(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);

    this->prbuf = &this->rbuf;
    this->pwbuf = &this->wbuf;
    this->prpos = &this->rpos;
    this->pwpos = &this->wpos;
    this->prsize = &this->rsize;
    this->pwsize = &this->wsize;

    basic_streambuf_char_setp(this, NULL, NULL);
    basic_streambuf_char_setg(this, NULL, NULL, NULL);
}

/* ?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEXXZ */
/* ?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Lock, 4)
void __thiscall basic_streambuf_char__Lock(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    mutex_lock(&this->lock);
}

/* ?_Pnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEHXZ */
/* ?_Pnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBA_JXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Pnavail, 4)
streamsize __thiscall basic_streambuf_char__Pnavail(const basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    return *this->pwpos ? *this->pwsize : 0;
}

/* ?_Pninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEPADXZ */
/* ?_Pninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Pninc, 4)
char* __thiscall basic_streambuf_char__Pninc(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    (*this->pwsize)--;
    return (*this->pwpos)++;
}

/* ?_Sgetn_s@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHPADIH@Z */
/* ?_Sgetn_s@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEAD_K_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Sgetn_s, 16)
streamsize __thiscall basic_streambuf_char__Sgetn_s(basic_streambuf_char *this, char *ptr, MSVCP_size_t size, streamsize count)
{
    FIXME("(%p %p %lu %lu) stub\n", this, ptr, size, count);
    return 0;
}

/* ?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEXXZ */
/* ?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Unlock, 4)
void __thiscall basic_streambuf_char__Unlock(basic_streambuf_char *this)
{
    TRACE("(%p)\n", this);
    mutex_unlock(&this->lock);
}

/* ?_Xsgetn_s@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHPADIH@Z */
/* ?_Xsgetn_s@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_K_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char__Xsgetn_s, 16)
streamsize __thiscall basic_streambuf_char__Xsgetn_s(basic_streambuf_char *this, char *ptr, MSVCP_size_t size, streamsize count)
{
    FIXME("(%p %p %lu %lu) stub\n", this, ptr, size, count);
    return 0;
}

/* ?eback@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?eback@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_eback, 4)
char* __thiscall basic_streambuf_char_eback(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?egptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?egptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_egptr, 4)
char* __thiscall basic_streambuf_char_egptr(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?epptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?epptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_epptr, 4)
char* __thiscall basic_streambuf_char_epptr(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?gbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXH@Z */
/* ?gbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_gbump, 8)
void __thiscall basic_streambuf_char_gbump(basic_streambuf_char *this, int off)
{
    FIXME("(%p %d) stub\n", this, off);
}

/* ?getloc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QBE?AVlocale@2@XZ */
/* ?getloc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEBA?AVlocale@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_getloc, 8)
locale* __thiscall basic_streambuf_char_getloc(const basic_streambuf_char *this, locale *ret)
{
    FIXME("(%p) stub\n", this);
    return ret;
}

/* ?gptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?gptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_gptr, 4)
char* __thiscall basic_streambuf_char_gptr(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEXABVlocale@2@@Z */
/* ?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAXAEBVlocale@2@@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_imbue, 8)
void __thiscall basic_streambuf_char_imbue(basic_streambuf_char *this, const locale *loc)
{
    FIXME("(%p %p) stub\n", this, loc);
}

/* ?in_avail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?in_avail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_in_avail, 4)
streamsize __thiscall basic_streambuf_char_in_avail(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?overflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHH@Z */
/* ?overflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_overflow, 8)
int __thiscall basic_streambuf_char_overflow(basic_streambuf_char *this, int ch)
{
    FIXME("(%p %d) stub\n", this, ch);
    return 0;
}

/* ?pbackfail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHH@Z */
/* ?pbackfail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pbackfail, 8)
int __thiscall basic_streambuf_char_pbackfail(basic_streambuf_char *this, int ch)
{
    FIXME("(%p %d) stub\n", this, ch);
    return 0;
}

/* ?pbase@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?pbase@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pbase, 4)
char* __thiscall basic_streambuf_char_pbase(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?pbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXH@Z */
/* ?pbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pbump, 8)
void __thiscall basic_streambuf_char_pbump(basic_streambuf_char *this, int off)
{
    FIXME("(%p %d) stub\n", this, off);
}

/* ?pptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IBEPADXZ */
/* ?pptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pptr, 4)
char* __thiscall basic_streambuf_char_pptr(const basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?pubimbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAE?AVlocale@2@ABV32@@Z */
/* ?pubimbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubimbue, 12)
locale* __thiscall basic_streambuf_char_pubimbue(basic_streambuf_char *this, locale *ret, const locale *loc)
{
    FIXME("(%p %p) stub\n", this, loc);
    return ret;
}

/* ?pubseekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAE?AV?$fpos@H@2@JHH@Z */
/* ?pubseekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@H@2@_JHH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubseekoff, 20)
fpos_int* __thiscall basic_streambuf_char_pubseekoff(basic_streambuf_char *this,
        fpos_int *ret, streamoff off, int way, int mode)
{
    FIXME("(%p %lu %d %d) stub\n", this, off, way, mode);
    return ret;
}

/* ?pubseekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAE?AV?$fpos@H@2@JII@Z */
/* ?pubseekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@H@2@_JII@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubseekoff_old, 20)
fpos_int* __thiscall basic_streambuf_char_pubseekoff_old(basic_streambuf_char *this,
        fpos_int *ret, streamoff off, unsigned int way, unsigned int mode)
{
    FIXME("(%p %lu %d %d) stub\n", this, off, way, mode);
    return ret;
}

/* ?pubseekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAE?AV?$fpos@H@2@V32@H@Z */
/* ?pubseekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@H@2@V32@H@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubseekpos, 36)
fpos_int* __thiscall basic_streambuf_char_pubseekpos(basic_streambuf_char *this,
        fpos_int *ret, fpos_int pos, int mode)
{
    FIXME("(%p %s %d) stub\n", this, debugstr_fpos_int(&pos), mode);
    return ret;
}

/* ?pubseekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAE?AV?$fpos@H@2@V32@I@Z */
/* ?pubseekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@H@2@V32@I@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubseekpos_old, 36)
fpos_int* __thiscall basic_streambuf_char_pubseekpos_old(basic_streambuf_char *this,
        fpos_int *ret, fpos_int pos, unsigned int mode)
{
    FIXME("(%p %s %d) stub\n", this, debugstr_fpos_int(&pos), mode);
    return ret;
}

/* ?pubsetbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEPAV12@PADH@Z */
/* ?pubsetbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAPEAV12@PEAD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubsetbuf, 12)
basic_streambuf_char* __thiscall basic_streambuf_char_pubsetbuf(basic_streambuf_char *this, char *buf, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, buf, count);
    return NULL;
}

/* ?pubsync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?pubsync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_pubsync, 4)
int __thiscall basic_streambuf_char_pubsync(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?sbumpc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?sbumpc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sbumpc, 4)
int __thiscall basic_streambuf_char_sbumpc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?seekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAE?AV?$fpos@H@2@JHH@Z */
/* ?seekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA?AV?$fpos@H@2@_JHH@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_seekoff, 20)
fpos_int* __thiscall basic_streambuf_char_seekoff(basic_streambuf_char *this,
        fpos_int *ret, streamoff off, int way, int mode)
{
    FIXME("(%p %lu %d %d) stub\n", this, off, way, mode);
    return ret;
}

/* ?seekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAE?AV?$fpos@H@2@V32@H@Z */
/* ?seekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA?AV?$fpos@H@2@V32@H@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_seekpos, 36)
fpos_int* __thiscall basic_streambuf_char_seekpos(basic_streambuf_char *this,
        fpos_int *ret, fpos_int pos, int mode)
{
    FIXME("(%p %s %d) stub\n", this, debugstr_fpos_int(&pos), mode);
    return ret;
}

/* ?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEPAV12@PADH@Z */
/* ?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAPEAV12@PEAD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_setbuf, 12)
basic_streambuf_char* __thiscall basic_streambuf_char_setbuf(basic_streambuf_char *this, char *buf, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, buf, count);
    return NULL;
}

/* ?setg@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXPAD00@Z */
/* ?setg@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD00@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_setg, 16)
void __thiscall basic_streambuf_char_setg(basic_streambuf_char *this, char *first, char *next, char *last)
{
    TRACE("(%p %p %p %p)\n", this, first, next, last);

    this->rbuf = first;
    this->rpos = next;
    this->rsize = last-next;
}

/* ?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXPAD00@Z */
/* ?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD00@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_setp_next, 16)
void __thiscall basic_streambuf_char_setp_next(basic_streambuf_char *this, char *first, char *next, char *last)
{
    TRACE("(%p %p %p %p)\n", this, first, next, last);

    this->wbuf = first;
    this->wpos = next;
    this->wsize = last-next;
}

/* ?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IAEXPAD0@Z */
/* ?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD0@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_setp, 12)
void __thiscall basic_streambuf_char_setp(basic_streambuf_char *this, char *first, char *last)
{
    basic_streambuf_char_setp_next(this, first, first, last);
}

/* ?sgetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?sgetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sgetc, 4)
int __thiscall basic_streambuf_char_sgetc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?sgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHPADH@Z */
/* ?sgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEAD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sgetn, 12)
streamsize __thiscall basic_streambuf_char_sgetn(basic_streambuf_char *this, char *ptr, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, ptr, count);
    return 0;
}

/* ?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHXZ */
/* ?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_showmanyc, 4)
streamsize __thiscall basic_streambuf_char_showmanyc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?snextc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?snextc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_snextc, 4)
int __thiscall basic_streambuf_char_snextc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?sputbackc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHD@Z */
/* ?sputbackc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sputbackc, 8)
int __thiscall basic_streambuf_char_sputbackc(basic_streambuf_char *this, char ch)
{
    FIXME("(%p %d) stub\n", this, ch);
    return 0;
}

/* ?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHD@Z */
/* ?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sputc, 8)
int __thiscall basic_streambuf_char_sputc(basic_streambuf_char *this, char ch)
{
    FIXME("(%p %d) stub\n", this, ch);
    return 0;
}

/* ?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHPBDH@Z */
/* ?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sputn, 12)
streamsize __thiscall basic_streambuf_char_sputn(basic_streambuf_char *this, const char *ptr, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, ptr, count);
    return 0;
}

/* ?stossc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEXXZ */
/* ?stossc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_stossc, 4)
void __thiscall basic_streambuf_char_stossc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
}

/* ?sungetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QAEHXZ */
/* ?sungetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sungetc, 4)
int __thiscall basic_streambuf_char_sungetc(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHXZ */
/* ?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_sync, 4)
int __thiscall basic_streambuf_char_sync(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHXZ */
/* ?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_uflow, 4)
int __thiscall basic_streambuf_char_uflow(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?underflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHXZ */
/* ?underflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_underflow, 4)
int __thiscall basic_streambuf_char_underflow(basic_streambuf_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHPADH@Z */
/* ?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_xsgetn, 12)
streamsize __thiscall basic_streambuf_char_xsgetn(basic_streambuf_char *this, char *ptr, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, ptr, count);
    return 0;
}

/* ?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MAEHPBDH@Z */
/* ?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_streambuf_char_xsputn, 12)
streamsize __thiscall basic_streambuf_char_xsputn(basic_streambuf_char *this, const char *ptr, streamsize count)
{
    FIXME("(%p %p %lu) stub\n", this, ptr, count);
    return 0;
}

/* ??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QAE@PAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z */
/* ??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_ctor, 12)
basic_ostream_char* __thiscall basic_ostream_char_ctor(basic_ostream_char *this, basic_streambuf_char *strbuf, MSVCP_bool isstd)
{
    FIXME("(%p %p %d) stub\n", this, strbuf, isstd);

    this->vtable = &MSVCP_basic_ostream_char_vtable+1;
    this->child.child.vtable = &MSVCP_basic_ostream_char_vtable;
    return NULL;
}

/* ??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QAE@W4_Uninitialized@1@_N@Z */
/* ??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@W4_Uninitialized@1@_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_uninitialized, 12)
basic_ostream_char* __thiscall basic_ostream_char_uninitialized(basic_ostream_char *this, int uninitialized, MSVCP_bool addstd)
{
    FIXME("(%p %d %x) stub\n", this, uninitialized, addstd);
    return NULL;
}

/* ??1?$basic_ostream@DU?$char_traits@D@std@@@std@@UAE@XZ */
/* ??1?$basic_ostream@DU?$char_traits@D@std@@@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_dtor, 4)
void __thiscall basic_ostream_char_dtor(basic_ostream_char *this)
{
    FIXME("(%p) stub\n", this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_basic_ostream_char_vector_dtor, 8)
basic_ostream_char* __thiscall MSVCP_basic_ostream_char_vector_dtor(basic_ostream_char *this, unsigned int flags)
{
    TRACE("(%p %x) stub\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)this-1;

        for(i=*ptr-1; i>=0; i--)
            basic_ostream_char_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        basic_ostream_char_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@F@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@F@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_short, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_short(basic_ostream_char *this, short val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@G@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_ushort, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_ushort(basic_ostream_char *this, unsigned short val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@H@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@H@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_int, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_int(basic_ostream_char *this, int val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@I@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@I@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_uint, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_uint(basic_ostream_char *this, unsigned int val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@J@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@J@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_long, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_long(basic_ostream_char *this, LONG val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@K@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@K@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_ulong, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_ulong(basic_ostream_char *this, ULONG val)
{
    FIXME("(%p %d) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@M@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@M@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_float, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_float(basic_ostream_char *this, float val)
{
    FIXME("(%p %f) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@N@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@N@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_double, 12)
basic_ostream_char* __thiscall basic_ostream_char_print_double(basic_ostream_char *this, double val)
{
    FIXME("(%p %lf) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@PAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_streambuf, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_streambuf(basic_ostream_char *this, basic_streambuf_char *val)
{
    FIXME("(%p %p) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@PBX@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@PEBX@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_ptr, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_ptr(basic_ostream_char *this, const void *val)
{
    FIXME("(%p %p) stub\n", this, val);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@_J@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_J@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_int64, 12)
basic_ostream_char* __thiscall basic_ostream_char_print_int64(basic_ostream_char *this, __int64 val)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@_K@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_K@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_uint64, 12)
basic_ostream_char* __thiscall basic_ostream_char_print_uint64(basic_ostream_char *this, unsigned __int64 val)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@_N@Z */
/* ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_N@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_print_bool, 8)
basic_ostream_char* __thiscall basic_ostream_char_print_bool(basic_ostream_char *this, MSVCP_bool val)
{
    FIXME("(%p %x) stub\n", this, val);
    return NULL;
}

/* ?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEXXZ */
/* ?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char__Osfx, 4)
void __thiscall basic_ostream_char__Osfx(basic_ostream_char *this)
{
    FIXME("(%p) stub\n", this);
}

/* ?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV12@XZ */
/* ?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_flush, 4)
basic_ostream_char* __thiscall basic_ostream_char_flush(basic_ostream_char *this)
{
    FIXME("(%p) stub\n", this);
    return NULL;
}

/* ?opfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAE_NXZ */
/* ?opfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA_NXZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_opfx, 4)
MSVCP_bool __thiscall basic_ostream_char_opfx(basic_ostream_char *this)
{
    FIXME("(%p) stub\n", this);
    return 0;
}

/* ?osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEXXZ */
/* ?osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_osfx, 4)
void __thiscall basic_ostream_char_osfx(basic_ostream_char *this)
{
    FIXME("(%p) stub\n", this);
}

/* ?put@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV12@D@Z */
/* ?put@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@D@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_put, 8)
basic_ostream_char* __thiscall basic_ostream_char_put(basic_ostream_char *this, char ch)
{
    FIXME("(%p %c) stub\n", this, ch);
    return NULL;
}

/* ?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV12@JH@Z */
/* ?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@_JH@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_seekp, 12)
basic_ostream_char* __thiscall basic_ostream_char_seekp(basic_ostream_char *this, streamoff off, int way)
{
    FIXME("(%p %lu %d) stub\n", this, off, way);
    return NULL;
}

/* ?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV12@V?$fpos@H@2@@Z */
/* ?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@V?$fpos@H@2@@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_seekp_fpos, 28)
basic_ostream_char* __thiscall basic_ostream_char_seekp_fpos(basic_ostream_char *this, fpos_int pos)
{
    FIXME("(%p %s) stub\n", this, debugstr_fpos_int(&pos));
    return NULL;
}

/* ?tellp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAE?AV?$fpos@H@2@XZ */
/* ?tellp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@H@2@XZ */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_tellp, 8)
fpos_int* __thiscall basic_ostream_char_tellp(basic_ostream_char *this, fpos_int *ret)
{
    FIXME("(%p) stub\n", this);
    return ret;
}

/* ?write@?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV12@PBDH@Z */
/* ?write@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEBD_J@Z */
DEFINE_THISCALL_WRAPPER(basic_ostream_char_write, 12)
basic_ostream_char* __thiscall basic_ostream_char_write(basic_ostream_char *this, const char *str, streamsize count)
{
    FIXME("(%p %s %lu) stub\n", this, debugstr_a(str), count);
    return NULL;
}
