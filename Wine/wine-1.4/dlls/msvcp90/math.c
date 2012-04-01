/*
 * Copyright 2011 Alexandre Julliard
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
#include <limits.h>
#include <float.h>
#include <math.h>

#include "msvcp90.h"
#include "windef.h"
#include "winbase.h"

typedef double LDOUBLE;  /* long double is just a double */

typedef struct { } std_Num_base;
typedef struct { } std_Ctraits;

enum std_float_denorm_style
{
    denorm_indeterminate = -1,
    denorm_absent = 0,
    denorm_present = 1
};

enum std_float_round_style
{
    round_indeterminate = -1,
    round_toward_zero = 0,
    round_to_nearest = 1,
    round_toward_infinity = 2,
    round_toward_neg_infinity = 3
};

/* these are defined as integers but the bit patterns are then interpreted as floats/doubles */
const DWORD     _FDenorm = 1;
const ULONGLONG _Denorm  = 1;
const ULONGLONG _LDenorm = 1;
const DWORD     _FInf    = 0x7f800000;
const ULONGLONG _Inf     = (ULONGLONG)0x7ff00000 << 32;
const ULONGLONG _LInf    = (ULONGLONG)0x7ff00000 << 32;
const DWORD     _FNan    = 0x7fc00000;
const ULONGLONG _Nan     = (ULONGLONG)0x7ff80000 << 32;
const ULONGLONG _LNan    = (ULONGLONG)0x7ff80000 << 32;
const DWORD     _FSnan   = 0x7f800001;
const ULONGLONG _Snan    = ((ULONGLONG)0x7ff00000 << 32) + 1;
const ULONGLONG _LSnan   = ((ULONGLONG)0x7ff00000 << 32) + 1;
const ULONGLONG _LZero   = 0;
const ULONGLONG _Hugeval = (ULONGLONG)0x7ff00000 << 32;

const float   _FEps = FLT_EPSILON;
const double  _Eps  = DBL_EPSILON;
const LDOUBLE _LEps = LDBL_EPSILON;

/* ?digits10@?$numeric_limits@C@std@@2HB -> public: static int const std::numeric_limits<signed char>::digits10 */
const int std_numeric_limits_signed_char_digits10 = 2;

/* ?digits10@?$numeric_limits@D@std@@2HB -> public: static int const std::numeric_limits<char>::digits10 */
const int std_numeric_limits_char_digits10 = 2;

/* ?digits10@?$numeric_limits@E@std@@2HB -> public: static int const std::numeric_limits<unsigned char>::digits10 */
const int std_numeric_limits_unsigned_char_digits10 = 2;

/* ?digits10@?$numeric_limits@F@std@@2HB -> public: static int const std::numeric_limits<short>::digits10 */
const int std_numeric_limits_short_digits10 = 4;

/* ?digits10@?$numeric_limits@G@std@@2HB -> public: static int const std::numeric_limits<unsigned short>::digits10 */
const int std_numeric_limits_unsigned_short_digits10 = 4;

/* ?digits10@?$numeric_limits@H@std@@2HB -> public: static int const std::numeric_limits<int>::digits10 */
const int std_numeric_limits_int_digits10 = 9;

/* ?digits10@?$numeric_limits@I@std@@2HB -> public: static int const std::numeric_limits<unsigned int>::digits10 */
const int std_numeric_limits_unsigned_int_digits10 = 9;

/* ?digits10@?$numeric_limits@J@std@@2HB -> public: static int const std::numeric_limits<long>::digits10 */
const int std_numeric_limits_long_digits10 = 9;

/* ?digits10@?$numeric_limits@K@std@@2HB -> public: static int const std::numeric_limits<unsigned long>::digits10 */
const int std_numeric_limits_unsigned_long_digits10 = 9;

/* ?digits10@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::digits10 */
const int std_numeric_limits_float_digits10 = FLT_DIG;

/* ?digits10@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::digits10 */
const int std_numeric_limits_double_digits10 = DBL_DIG;

/* ?digits10@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::digits10 */
const int std_numeric_limits_long_double_digits10 = LDBL_DIG;

/* ?digits10@?$numeric_limits@_J@std@@2HB -> public: static int const std::numeric_limits<__int64>::digits10 */
const int std_numeric_limits_int64_digits10 = 18;

/* ?digits10@?$numeric_limits@_K@std@@2HB -> public: static int const std::numeric_limits<unsigned __int64>::digits10 */
const int std_numeric_limits_unsigned_int64_digits10 = 18;

/* ?digits10@?$numeric_limits@_N@std@@2HB -> public: static int const std::numeric_limits<bool>::digits10 */
const int std_numeric_limits_bool_digits10 = 0;

/* ?digits10@?$numeric_limits@_W@std@@2HB -> public: static int const std::numeric_limits<wchar_t>::digits10 */
const int std_numeric_limits_wchar_t_digits10 = 4;

/* ?digits10@_Num_base@std@@2HB -> public: static int const std::_Num_base::digits10 */
const int std_Num_base_digits10 = 0;

/* ?digits@?$numeric_limits@C@std@@2HB -> public: static int const std::numeric_limits<signed char>::digits */
const int std_numeric_limits_signed_char_digits = 7;

/* ?digits@?$numeric_limits@D@std@@2HB -> public: static int const std::numeric_limits<char>::digits */
const int std_numeric_limits_char_digits = (CHAR_MIN < 0) ? 7 : 8;

/* ?digits@?$numeric_limits@E@std@@2HB -> public: static int const std::numeric_limits<unsigned char>::digits */
const int std_numeric_limits_unsigned_char_digits = 8;

/* ?digits@?$numeric_limits@F@std@@2HB -> public: static int const std::numeric_limits<short>::digits */
const int std_numeric_limits_short_digits = 15;

/* ?digits@?$numeric_limits@G@std@@2HB -> public: static int const std::numeric_limits<unsigned short>::digits */
const int std_numeric_limits_unsigned_short_digits = 16;

/* ?digits@?$numeric_limits@H@std@@2HB -> public: static int const std::numeric_limits<int>::digits */
const int std_numeric_limits_int_digits = 31;

/* ?digits@?$numeric_limits@I@std@@2HB -> public: static int const std::numeric_limits<unsigned int>::digits */
const int std_numeric_limits_unsigned_int_digits = 32;

/* ?digits@?$numeric_limits@J@std@@2HB -> public: static int const std::numeric_limits<long>::digits */
const int std_numeric_limits_long_digits = 31;

/* ?digits@?$numeric_limits@K@std@@2HB -> public: static int const std::numeric_limits<unsigned long>::digits */
const int std_numeric_limits_unsigned_long_digits = 32;

/* ?digits@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::digits */
const int std_numeric_limits_float_digits = FLT_MANT_DIG;

/* ?digits@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::digits */
const int std_numeric_limits_double_digits = DBL_MANT_DIG;

/* ?digits@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::digits */
const int std_numeric_limits_long_double_digits = LDBL_MANT_DIG;

/* ?digits@?$numeric_limits@_J@std@@2HB -> public: static int const std::numeric_limits<__int64>::digits */
const int std_numeric_limits_int64_digits = 63;

/* ?digits@?$numeric_limits@_K@std@@2HB -> public: static int const std::numeric_limits<unsigned __int64>::digits */
const int std_numeric_limits_unsigned_int64_digits = 64;

/* ?digits@?$numeric_limits@_N@std@@2HB -> public: static int const std::numeric_limits<bool>::digits */
const int std_numeric_limits_bool_digits = 1;

/* ?digits@?$numeric_limits@_W@std@@2HB -> public: static int const std::numeric_limits<wchar_t>::digits */
const int std_numeric_limits_wchar_t_digits = 16;

/* ?digits@_Num_base@std@@2HB -> public: static int const std::_Num_base::digits */
const int std_Num_base_digits = 0;

/* ?has_denorm@_Num_base@std@@2W4float_denorm_style@2@B -> public: static enum std::float_denorm_style const std::_Num_base::has_denorm */
const enum std_float_denorm_style std_Num_base_has_denorm = denorm_absent;

/* ?has_denorm@_Num_float_base@std@@2W4float_denorm_style@2@B -> public: static enum std::float_denorm_style const std::_Num_float_base::has_denorm */
const enum std_float_denorm_style std_Num_float_base_has_denorm = denorm_present;

/* ?has_denorm_loss@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::has_denorm_loss */
const BOOLEAN std_Num_base_has_denorm_loss = FALSE;

/* ?has_denorm_loss@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::has_denorm_loss */
const BOOLEAN std_Num_float_base_has_denorm_loss = TRUE;

/* ?has_infinity@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::has_infinity */
const BOOLEAN std_Num_base_has_infinity = FALSE;

/* ?has_infinity@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::has_infinity */
const BOOLEAN std_Num_float_base_has_infinity = TRUE;

/* ?has_quiet_NaN@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::has_quiet_NaN */
const BOOLEAN std_Num_base_has_quiet_NaN = FALSE;

/* ?has_quiet_NaN@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::has_quiet_NaN */
const BOOLEAN std_Num_float_base_has_quiet_NaN = TRUE;

/* ?has_signaling_NaN@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::has_signaling_NaN */
const BOOLEAN std_Num_base_has_signaling_NaN = FALSE;

/* ?has_signaling_NaN@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::has_signaling_NaN */
const BOOLEAN std_Num_float_base_has_signaling_NaN = TRUE;

/* ?is_bounded@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_bounded */
const BOOLEAN std_Num_base_is_bounded = FALSE;

/* ?is_bounded@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_bounded */
const BOOLEAN std_Num_float_base_is_bounded = TRUE;

/* ?is_bounded@_Num_int_base@std@@2_NB -> public: static bool const std::_Num_int_base::is_bounded */
const BOOLEAN std_Num_int_base_is_bounded = TRUE;

/* ?is_exact@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_exact */
const BOOLEAN std_Num_base_is_exact = FALSE;

/* ?is_exact@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_exact */
const BOOLEAN std_Num_float_base_is_exact = FALSE;

/* ?is_exact@_Num_int_base@std@@2_NB -> public: static bool const std::_Num_int_base::is_exact */
const BOOLEAN std_Num_int_base_is_exact = TRUE;

/* ?is_iec559@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_iec559 */
const BOOLEAN std_Num_base_is_iec559 = FALSE;

/* ?is_iec559@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_iec559 */
const BOOLEAN std_Num_float_base_is_iec559 = TRUE;

/* ?is_integer@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_integer */
const BOOLEAN std_Num_base_is_integer = FALSE;

/* ?is_integer@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_integer */
const BOOLEAN std_Num_float_base_is_integer = FALSE;

/* ?is_integer@_Num_int_base@std@@2_NB -> public: static bool const std::_Num_int_base::is_integer */
const BOOLEAN std_Num_int_base_is_integer = TRUE;

/* ?is_modulo@?$numeric_limits@_N@std@@2_NB -> public: static bool const std::numeric_limits<bool>::is_modulo */
const BOOLEAN std_numeric_limits_bool_is_modulo = FALSE;

/* ?is_modulo@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_modulo */
const BOOLEAN std_Num_base_is_modulo = FALSE;

/* ?is_modulo@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_modulo */
const BOOLEAN std_Num_float_base_is_modulo = FALSE;

/* ?is_modulo@_Num_int_base@std@@2_NB -> public: static bool const std::_Num_int_base::is_modulo */
const BOOLEAN std_Num_int_base_is_modulo = TRUE;

/* ?is_signed@?$numeric_limits@C@std@@2_NB -> public: static bool const std::numeric_limits<signed char>::is_signed */
const BOOLEAN std_numeric_limits_signed_char_is_signed = TRUE;

/* ?is_signed@?$numeric_limits@D@std@@2_NB -> public: static bool const std::numeric_limits<char>::is_signed */
const BOOLEAN std_numeric_limits_char_is_signed = (CHAR_MIN < 0);

/* ?is_signed@?$numeric_limits@E@std@@2_NB -> public: static bool const std::numeric_limits<unsigned char>::is_signed */
const BOOLEAN std_numeric_limits_unsigned_char_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@F@std@@2_NB -> public: static bool const std::numeric_limits<short>::is_signed */
const BOOLEAN std_numeric_limits_short_is_signed = TRUE;

/* ?is_signed@?$numeric_limits@G@std@@2_NB -> public: static bool const std::numeric_limits<unsigned short>::is_signed */
const BOOLEAN std_numeric_limits_unsigned_short_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@H@std@@2_NB -> public: static bool const std::numeric_limits<int>::is_signed */
const BOOLEAN std_numeric_limits_int_is_signed = TRUE;

/* ?is_signed@?$numeric_limits@I@std@@2_NB -> public: static bool const std::numeric_limits<unsigned int>::is_signed */
const BOOLEAN std_numeric_limits_unsigned_int_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@J@std@@2_NB -> public: static bool const std::numeric_limits<long>::is_signed */
const BOOLEAN std_numeric_limits_long_is_signed = TRUE;

/* ?is_signed@?$numeric_limits@K@std@@2_NB -> public: static bool const std::numeric_limits<unsigned long>::is_signed */
const BOOLEAN std_numeric_limits_unsigned_long_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@_J@std@@2_NB -> public: static bool const std::numeric_limits<__int64>::is_signed */
const BOOLEAN std_numeric_limits_int64_is_signed = TRUE;

/* ?is_signed@?$numeric_limits@_K@std@@2_NB -> public: static bool const std::numeric_limits<unsigned __int64>::is_signed */
const BOOLEAN std_numeric_limits_unsigned_int64_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@_N@std@@2_NB -> public: static bool const std::numeric_limits<bool>::is_signed */
const BOOLEAN std_numeric_limits_bool_is_signed = FALSE;

/* ?is_signed@?$numeric_limits@_W@std@@2_NB -> public: static bool const std::numeric_limits<wchar_t>::is_signed */
const BOOLEAN std_numeric_limits_wchar_t_is_signed = FALSE;

/* ?is_signed@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_signed */
const BOOLEAN std_Num_base_is_signed = FALSE;

/* ?is_signed@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_signed */
const BOOLEAN std_Num_float_base_is_signed = TRUE;

/* ?is_specialized@_Num_base@std@@2_NB -> public: static bool const std::_Num_base::is_specialized */
const BOOLEAN std_Num_base_is_specialized = FALSE;

/* ?is_specialized@_Num_float_base@std@@2_NB -> public: static bool const std::_Num_float_base::is_specialized */
const BOOLEAN std_Num_float_base_is_specialized = TRUE;

/* ?is_specialized@_Num_int_base@std@@2_NB -> public: static bool const std::_Num_int_base::is_specialized */
const BOOLEAN std_Num_int_base_is_specialized = TRUE;

/* ?max_exponent10@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::max_exponent10 */
const int std_numeric_limits_float_max_exponent10 = FLT_MAX_10_EXP;

/* ?max_exponent10@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::max_exponent10 */
const int std_numeric_limits_double_max_exponent10 = DBL_MAX_10_EXP;

/* ?max_exponent10@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::max_exponent10 */
const int std_numeric_limits_long_double_max_exponent10 = LDBL_MAX_10_EXP;

/* ?max_exponent10@_Num_base@std@@2HB -> public: static int const std::_Num_base::max_exponent10 */
const int std_Num_base_max_exponent10 = 0;

/* ?max_exponent@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::max_exponent */
const int std_numeric_limits_float_max_exponent = FLT_MAX_EXP;

/* ?max_exponent@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::max_exponent */
const int std_numeric_limits_double_max_exponent = DBL_MAX_EXP;

/* ?max_exponent@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::max_exponent */
const int std_numeric_limits_long_double_max_exponent = LDBL_MAX_EXP;

/* ?max_exponent@_Num_base@std@@2HB -> public: static int const std::_Num_base::max_exponent */
const int std_Num_base_max_exponent = 0;

/* ?min_exponent10@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::min_exponent10 */
const int std_numeric_limits_float_min_exponent10 = FLT_MIN_10_EXP;

/* ?min_exponent10@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::min_exponent10 */
const int std_numeric_limits_double_min_exponent10 = DBL_MIN_10_EXP;

/* ?min_exponent10@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::min_exponent10 */
const int std_numeric_limits_long_double_min_exponent10 = LDBL_MIN_10_EXP;

/* ?min_exponent10@_Num_base@std@@2HB -> public: static int const std::_Num_base::min_exponent10 */
const int std_Num_base_min_exponent10 = 0;

/* ?min_exponent@?$numeric_limits@M@std@@2HB -> public: static int const std::numeric_limits<float>::min_exponent */
const int std_numeric_limits_float_min_exponent = FLT_MIN_EXP;

/* ?min_exponent@?$numeric_limits@N@std@@2HB -> public: static int const std::numeric_limits<double>::min_exponent */
const int std_numeric_limits_double_min_exponent = DBL_MIN_EXP;

/* ?min_exponent@?$numeric_limits@O@std@@2HB -> public: static int const std::numeric_limits<long double>::min_exponent */
const int std_numeric_limits_long_double_min_exponent = LDBL_MIN_EXP;

/* ?min_exponent@_Num_base@std@@2HB -> public: static int const std::_Num_base::min_exponent */
const int std_Num_base_min_exponent = 0;

/* ?radix@_Num_base@std@@2HB -> public: static int const std::_Num_base::radix */
const int std_Num_base_radix = 0;

/* ?radix@_Num_float_base@std@@2HB -> public: static int const std::_Num_float_base::radix */
const int std_Num_float_base_radix = FLT_RADIX;

/* ?radix@_Num_int_base@std@@2HB -> public: static int const std::_Num_int_base::radix */
const int std_Num_int_base_radix = 2;

/* ?round_style@_Num_base@std@@2W4float_round_style@2@B -> public: static enum std::float_round_style const std::_Num_base::round_style */
const enum std_float_round_style std_Num_base_round_style = round_toward_zero;

/* ?round_style@_Num_float_base@std@@2W4float_round_style@2@B -> public: static enum std::float_round_style const std::_Num_float_base::round_style */
const enum std_float_round_style std_Num_float_base_round_style = round_to_nearest;

/* ?tinyness_before@_Num_base@std@@2_NB -> public: static bool const  std::_Num_base::tinyness_before */
const BOOLEAN std_Num_base_tinyness_before = FALSE;

/* ?tinyness_before@_Num_float_base@std@@2_NB -> public: static bool const  std::_Num_float_base::tinyness_before */
const BOOLEAN std_Num_float_base_tinyness_before = TRUE;

/* ?traps@_Num_base@std@@2_NB -> public: static bool const  std::_Num_base::traps */
const BOOLEAN std_Num_base_traps = FALSE;

/* ?traps@_Num_float_base@std@@2_NB -> public: static bool const  std::_Num_float_base::traps */
const BOOLEAN std_Num_float_base_traps = TRUE;

/* ??4?$numeric_limits@C@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<signed char> & __thiscall std::numeric_limits<signed char>::operator=(class std::numeric_limits<signed char> const &) */
/* ??4?$numeric_limits@C@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<signed char> & __ptr64 __cdecl std::numeric_limits<signed char>::operator=(class std::numeric_limits<signed char> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@D@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<char> & __thiscall std::numeric_limits<char>::operator=(class std::numeric_limits<char> const &) */
/* ??4?$numeric_limits@D@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<char> & __ptr64 __cdecl std::numeric_limits<char>::operator=(class std::numeric_limits<char> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@E@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<unsigned char> & __thiscall std::numeric_limits<unsigned char>::operator=(class std::numeric_limits<unsigned char> const &) */
/* ??4?$numeric_limits@E@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<unsigned char> & __ptr64 __cdecl std::numeric_limits<unsigned char>::operator=(class std::numeric_limits<unsigned char> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@F@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<short> & __thiscall std::numeric_limits<short>::operator=(class std::numeric_limits<short> const &) */
/* ??4?$numeric_limits@F@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<short> & __ptr64 __cdecl std::numeric_limits<short>::operator=(class std::numeric_limits<short> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@G@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<unsigned short> & __thiscall std::numeric_limits<unsigned short>::operator=(class std::numeric_limits<unsigned short> const &) */
/* ??4?$numeric_limits@G@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<unsigned short> & __ptr64 __cdecl std::numeric_limits<unsigned short>::operator=(class std::numeric_limits<unsigned short> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@H@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<int> & __thiscall std::numeric_limits<int>::operator=(class std::numeric_limits<int> const &) */
/* ??4?$numeric_limits@H@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<int> & __ptr64 __cdecl std::numeric_limits<int>::operator=(class std::numeric_limits<int> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@I@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<unsigned int> & __thiscall std::numeric_limits<unsigned int>::operator=(class std::numeric_limits<unsigned int> const &) */
/* ??4?$numeric_limits@I@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<unsigned int> & __ptr64 __cdecl std::numeric_limits<unsigned int>::operator=(class std::numeric_limits<unsigned int> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@J@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<long> & __thiscall std::numeric_limits<long>::operator=(class std::numeric_limits<long> const &) */
/* ??4?$numeric_limits@J@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<long> & __ptr64 __cdecl std::numeric_limits<long>::operator=(class std::numeric_limits<long> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@K@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<unsigned long> & __thiscall std::numeric_limits<unsigned long>::operator=(class std::numeric_limits<unsigned long> const &) */
/* ??4?$numeric_limits@K@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<unsigned long> & __ptr64 __cdecl std::numeric_limits<unsigned long>::operator=(class std::numeric_limits<unsigned long> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@M@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<float> & __thiscall std::numeric_limits<float>::operator=(class std::numeric_limits<float> const &) */
/* ??4?$numeric_limits@M@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<float> & __ptr64 __cdecl std::numeric_limits<float>::operator=(class std::numeric_limits<float> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@N@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<double> & __thiscall std::numeric_limits<double>::operator=(class std::numeric_limits<double> const &) */
/* ??4?$numeric_limits@N@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<double> & __ptr64 __cdecl std::numeric_limits<double>::operator=(class std::numeric_limits<double> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@O@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<long double> & __thiscall std::numeric_limits<long double>::operator=(class std::numeric_limits<long double> const &) */
/* ??4?$numeric_limits@O@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<long double> & __ptr64 __cdecl std::numeric_limits<long double>::operator=(class std::numeric_limits<long double> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@_J@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<__int64> & __thiscall std::numeric_limits<__int64>::operator=(class std::numeric_limits<__int64> const &) */
/* ??4?$numeric_limits@_J@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<__int64> & __ptr64 __cdecl std::numeric_limits<__int64>::operator=(class std::numeric_limits<__int64> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@_K@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<unsigned __int64> & __thiscall std::numeric_limits<unsigned __int64>::operator=(class std::numeric_limits<unsigned __int64> const &) */
/* ??4?$numeric_limits@_K@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<unsigned __int64> & __ptr64 __cdecl std::numeric_limits<unsigned __int64>::operator=(class std::numeric_limits<unsigned __int64> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@_N@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<bool> & __thiscall std::numeric_limits<bool>::operator=(class std::numeric_limits<bool> const &) */
/* ??4?$numeric_limits@_N@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<bool> & __ptr64 __cdecl std::numeric_limits<bool>::operator=(class std::numeric_limits<bool> const & __ptr64) __ptr64 */
/* ??4?$numeric_limits@_W@std@@QAEAAV01@ABV01@@Z -> public: class std::numeric_limits<wchar_t> & __thiscall std::numeric_limits<wchar_t>::operator=(class std::numeric_limits<wchar_t> const &) */
/* ??4?$numeric_limits@_W@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::numeric_limits<wchar_t> & __ptr64 __cdecl std::numeric_limits<wchar_t>::operator=(class std::numeric_limits<wchar_t> const & __ptr64) __ptr64 */
/* ??4_Num_base@std@@QAEAAU01@ABU01@@Z -> public: struct std::_Num_base & __thiscall std::_Num_base::operator=(struct std::_Num_base const &) */
/* ??4_Num_base@std@@QEAAAEAU01@AEBU01@@Z -> public: struct std::_Num_base & __ptr64 __cdecl std::_Num_base::operator=(struct std::_Num_base const & __ptr64) __ptr64 */
/* ??4_Num_float_base@std@@QAEAAU01@ABU01@@Z -> public: struct std::_Num_float_base & __thiscall std::_Num_float_base::operator=(struct std::_Num_float_base const &) */
/* ??4_Num_float_base@std@@QEAAAEAU01@AEBU01@@Z -> public: struct std::_Num_float_base & __ptr64 __cdecl std::_Num_float_base::operator=(struct std::_Num_float_base const & __ptr64) __ptr64 */
/* ??4_Num_int_base@std@@QAEAAU01@ABU01@@Z -> public: struct std::_Num_int_base & __thiscall std::_Num_int_base::operator=(struct std::_Num_int_base const &) */
/* ??4_Num_int_base@std@@QEAAAEAU01@AEBU01@@Z -> public: struct std::_Num_int_base & __ptr64 __cdecl std::_Num_int_base::operator=(struct std::_Num_int_base const & __ptr64) __ptr64 */
DEFINE_THISCALL_WRAPPER( std_Num_base_op_assign, 8 )
std_Num_base * __thiscall std_Num_base_op_assign( std_Num_base *this, std_Num_base *right )
{
    return this;
}

/* ?denorm_min@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::denorm_min(void) */
signed char __cdecl std_numeric_limits_signed_char_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::denorm_min(void) */
char __cdecl std_numeric_limits_char_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::denorm_min(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::denorm_min(void) */
short __cdecl std_numeric_limits_short_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::denorm_min(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::denorm_min(void) */
int __cdecl std_numeric_limits_int_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::denorm_min(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::denorm_min(void) */
LONG __cdecl std_numeric_limits_long_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::denorm_min(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::denorm_min(void) */
float __cdecl std_numeric_limits_float_denorm_min(void) { return *(float *)&_FDenorm; }

/* ?denorm_min@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::denorm_min(void) */
double __cdecl std_numeric_limits_double_denorm_min(void) { return *(double *)&_Denorm; }

/* ?denorm_min@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::denorm_min(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_denorm_min(void) { return *(LDOUBLE *)&_LDenorm; }

/* ?denorm_min@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::denorm_min(void) */
__int64 __cdecl std_numeric_limits_int64_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::denorm_min(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::denorm_min(void) */
BOOLEAN __cdecl std_numeric_limits_bool_denorm_min(void) { return 0; }

/* ?denorm_min@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::denorm_min(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_denorm_min(void) { return 0; }

/* ?epsilon@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::epsilon(void) */
signed char __cdecl std_numeric_limits_signed_char_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::epsilon(void) */
char __cdecl std_numeric_limits_char_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::epsilon(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::epsilon(void) */
short __cdecl std_numeric_limits_short_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::epsilon(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::epsilon(void) */
int __cdecl std_numeric_limits_int_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::epsilon(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::epsilon(void) */
LONG __cdecl std_numeric_limits_long_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::epsilon(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::epsilon(void) */
float __cdecl std_numeric_limits_float_epsilon(void) { return _FEps; }

/* ?epsilon@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::epsilon(void) */
double __cdecl std_numeric_limits_double_epsilon(void) { return _Eps; }

/* ?epsilon@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::epsilon(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_epsilon(void) { return _LEps; }

/* ?epsilon@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::epsilon(void) */
__int64 __cdecl std_numeric_limits_int64_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::epsilon(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::epsilon(void) */
BOOLEAN __cdecl std_numeric_limits_bool_epsilon(void) { return 0; }

/* ?epsilon@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::epsilon(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_epsilon(void) { return 0; }

/* ?infinity@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::infinity(void) */
signed char __cdecl std_numeric_limits_signed_char_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::infinity(void) */
char __cdecl std_numeric_limits_char_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::infinity(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::infinity(void) */
short __cdecl std_numeric_limits_short_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::infinity(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::infinity(void) */
int __cdecl std_numeric_limits_int_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::infinity(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::infinity(void) */
LONG __cdecl std_numeric_limits_long_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::infinity(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::infinity(void) */
float __cdecl std_numeric_limits_float_infinity(void) { return *(float *)&_FInf; }

/* ?infinity@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::infinity(void) */
double __cdecl std_numeric_limits_double_infinity(void) { return *(double *)&_Inf; }

/* ?infinity@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::infinity(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_infinity(void) { return *(LDOUBLE *)&_LInf; }

/* ?infinity@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::infinity(void) */
__int64 __cdecl std_numeric_limits_int64_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::infinity(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::infinity(void) */
BOOLEAN __cdecl std_numeric_limits_bool_infinity(void) { return 0; }

/* ?infinity@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::infinity(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_infinity(void) { return 0; }

/* ?max@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::max(void) */
signed char __cdecl std_numeric_limits_signed_char_max(void) { return SCHAR_MAX; }

/* ?max@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::max(void) */
char __cdecl std_numeric_limits_char_max(void) { return CHAR_MAX; }

/* ?max@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::max(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_max(void) { return UCHAR_MAX; }

/* ?max@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::max(void) */
short __cdecl std_numeric_limits_short_max(void) { return SHRT_MAX; }

/* ?max@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::max(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_max(void) { return USHRT_MAX; }

/* ?max@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::max(void) */
int __cdecl std_numeric_limits_int_max(void) { return INT_MAX; }

/* ?max@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::max(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_max(void) { return UINT_MAX; }

/* ?max@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::max(void) */
LONG __cdecl std_numeric_limits_long_max(void) { return LONG_MAX; }

/* ?max@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::max(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_max(void) { return ULONG_MAX; }

/* ?max@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::max(void) */
float __cdecl std_numeric_limits_float_max(void) { return FLT_MAX; }

/* ?max@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::max(void) */
double __cdecl std_numeric_limits_double_max(void) { return DBL_MAX; }

/* ?max@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::max(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_max(void) { return LDBL_MAX; }

/* ?max@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::max(void) */
__int64 __cdecl std_numeric_limits_int64_max(void) { return I64_MAX; }

/* ?max@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::max(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_max(void) { return UI64_MAX; }

/* ?max@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::max(void) */
BOOLEAN __cdecl std_numeric_limits_bool_max(void) { return 1; }

/* ?max@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::max(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_max(void) { return USHRT_MAX; }

/* ?min@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::min(void) */
signed char __cdecl std_numeric_limits_signed_char_min(void) { return SCHAR_MIN; }

/* ?min@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::min(void) */
char __cdecl std_numeric_limits_char_min(void) { return CHAR_MIN; }

/* ?min@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::min(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_min(void) { return 0; }

/* ?min@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::min(void) */
short __cdecl std_numeric_limits_short_min(void) { return SHRT_MIN; }

/* ?min@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::min(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_min(void) { return 0; }

/* ?min@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::min(void) */
int __cdecl std_numeric_limits_int_min(void) { return INT_MIN; }

/* ?min@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::min(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_min(void) { return 0; }

/* ?min@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::min(void) */
LONG __cdecl std_numeric_limits_long_min(void) { return LONG_MIN; }

/* ?min@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::min(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_min(void) { return 0; }

/* ?min@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::min(void) */
float __cdecl std_numeric_limits_float_min(void) { return FLT_MIN; }

/* ?min@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::min(void) */
double __cdecl std_numeric_limits_double_min(void) { return DBL_MIN; }

/* ?min@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::min(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_min(void) { return LDBL_MIN; }

/* ?min@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::min(void) */
__int64 __cdecl std_numeric_limits_int64_min(void) { return I64_MIN; }

/* ?min@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::min(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_min(void) { return 0; }

/* ?min@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::min(void) */
BOOLEAN __cdecl std_numeric_limits_bool_min(void) { return 0; }

/* ?min@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::min(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_min(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::quiet_NaN(void) */
signed char __cdecl std_numeric_limits_signed_char_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::quiet_NaN(void) */
char __cdecl std_numeric_limits_char_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::quiet_NaN(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::quiet_NaN(void) */
short __cdecl std_numeric_limits_short_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::quiet_NaN(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::quiet_NaN(void) */
int __cdecl std_numeric_limits_int_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::quiet_NaN(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::quiet_NaN(void) */
LONG __cdecl std_numeric_limits_long_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::quiet_NaN(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::quiet_NaN(void) */
float __cdecl std_numeric_limits_float_quiet_NaN(void) { return *(float *)&_FNan; }

/* ?quiet_NaN@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::quiet_NaN(void) */
double __cdecl std_numeric_limits_double_quiet_NaN(void) { return *(double *)&_Nan; }

/* ?quiet_NaN@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::quiet_NaN(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_quiet_NaN(void) { return *(LDOUBLE *)&_LNan; }

/* ?quiet_NaN@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::quiet_NaN(void) */
__int64 __cdecl std_numeric_limits_int64_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::quiet_NaN(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::quiet_NaN(void) */
BOOLEAN __cdecl std_numeric_limits_bool_quiet_NaN(void) { return 0; }

/* ?quiet_NaN@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::quiet_NaN(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_quiet_NaN(void) { return 0; }

/* ?round_error@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::round_error(void) */
signed char __cdecl std_numeric_limits_signed_char_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::round_error(void) */
char __cdecl std_numeric_limits_char_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::round_error(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::round_error(void) */
short __cdecl std_numeric_limits_short_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::round_error(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::round_error(void) */
int __cdecl std_numeric_limits_int_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::round_error(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::round_error(void) */
LONG __cdecl std_numeric_limits_long_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::round_error(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::round_error(void) */
float __cdecl std_numeric_limits_float_round_error(void) { return 0.5; }

/* ?round_error@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::round_error(void) */
double __cdecl std_numeric_limits_double_round_error(void) { return 0.5; }

/* ?round_error@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::round_error(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_round_error(void) { return 0.5; }

/* ?round_error@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::round_error(void) */
__int64 __cdecl std_numeric_limits_int64_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::round_error(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::round_error(void) */
BOOLEAN __cdecl std_numeric_limits_bool_round_error(void) { return 0; }

/* ?round_error@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::round_error(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_round_error(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@C@std@@SACXZ -> public: static signed char __cdecl std::numeric_limits<signed char>::signaling_NaN(void) */
signed char __cdecl std_numeric_limits_signed_char_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@D@std@@SADXZ -> public: static char __cdecl std::numeric_limits<char>::signaling_NaN(void) */
char __cdecl std_numeric_limits_char_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@E@std@@SAEXZ -> public: static unsigned char __cdecl std::numeric_limits<unsigned char>::signaling_NaN(void) */
unsigned char __cdecl std_numeric_limits_unsigned_char_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@F@std@@SAFXZ -> public: static short __cdecl std::numeric_limits<short>::signaling_NaN(void) */
short __cdecl std_numeric_limits_short_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@G@std@@SAGXZ -> public: static unsigned short __cdecl std::numeric_limits<unsigned short>::signaling_NaN(void) */
unsigned short __cdecl std_numeric_limits_unsigned_short_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@H@std@@SAHXZ -> public: static int __cdecl std::numeric_limits<int>::signaling_NaN(void) */
int __cdecl std_numeric_limits_int_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@I@std@@SAIXZ -> public: static unsigned int __cdecl std::numeric_limits<unsigned int>::signaling_NaN(void) */
unsigned int __cdecl std_numeric_limits_unsigned_int_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@J@std@@SAJXZ -> public: static long __cdecl std::numeric_limits<long>::signaling_NaN(void) */
LONG __cdecl std_numeric_limits_long_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@K@std@@SAKXZ -> public: static unsigned long __cdecl std::numeric_limits<unsigned long>::signaling_NaN(void) */
ULONG __cdecl std_numeric_limits_unsigned_long_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@M@std@@SAMXZ -> public: static float __cdecl std::numeric_limits<float>::signaling_NaN(void) */
float __cdecl std_numeric_limits_float_signaling_NaN(void) { return *(float *)&_FSnan; }

/* ?signaling_NaN@?$numeric_limits@N@std@@SANXZ -> public: static double __cdecl std::numeric_limits<double>::signaling_NaN(void) */
double __cdecl std_numeric_limits_double_signaling_NaN(void) { return *(double *)&_Snan; }

/* ?signaling_NaN@?$numeric_limits@O@std@@SAOXZ -> public: static long double __cdecl std::numeric_limits<long double>::signaling_NaN(void) */
LDOUBLE __cdecl std_numeric_limits_long_double_signaling_NaN(void) { return *(LDOUBLE *)&_LSnan; }

/* ?signaling_NaN@?$numeric_limits@_J@std@@SA_JXZ -> public: static __int64 __cdecl std::numeric_limits<__int64>::signaling_NaN(void) */
__int64 __cdecl std_numeric_limits_int64_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@_K@std@@SA_KXZ -> public: static unsigned __int64 __cdecl std::numeric_limits<unsigned __int64>::signaling_NaN(void) */
unsigned __int64 __cdecl std_numeric_limits_unsigned_int64_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@_N@std@@SA_NXZ -> public: static bool __cdecl std::numeric_limits<bool>::signaling_NaN(void) */
BOOLEAN __cdecl std_numeric_limits_bool_signaling_NaN(void) { return 0; }

/* ?signaling_NaN@?$numeric_limits@_W@std@@SA_WXZ -> public: static wchar_t __cdecl std::numeric_limits<wchar_t>::signaling_NaN(void) */
WCHAR __cdecl std_numeric_limits_wchar_t_signaling_NaN(void) { return 0; }

/* ??4?$_Ctraits@M@std@@QAEAAV01@ABV01@@Z -> public: class std::_Ctraits<float> & __thiscall std::_Ctraits<float>::operator=(class std::_Ctraits<float> const &) */
/* ??4?$_Ctraits@M@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::_Ctraits<float> & __ptr64 __cdecl std::_Ctraits<float>::operator=(class std::_Ctraits<float> const & __ptr64) __ptr64 */
/* ??4?$_Ctraits@N@std@@QAEAAV01@ABV01@@Z -> public: class std::_Ctraits<double> & __thiscall std::_Ctraits<double>::operator=(class std::_Ctraits<double> const &) */
/* ??4?$_Ctraits@N@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::_Ctraits<double> & __ptr64 __cdecl std::_Ctraits<double>::operator=(class std::_Ctraits<double> const & __ptr64) __ptr64 */
/* ??4?$_Ctraits@O@std@@QAEAAV01@ABV01@@Z -> public: class std::_Ctraits<long double> & __thiscall std::_Ctraits<long double>::operator=(class std::_Ctraits<long double> const &) */
/* ??4?$_Ctraits@O@std@@QEAAAEAV01@AEBV01@@Z -> public: class std::_Ctraits<long double> & __ptr64 __cdecl std::_Ctraits<long double>::operator=(class std::_Ctraits<long double> const & __ptr64) __ptr64 */
DEFINE_THISCALL_WRAPPER( std_Ctraits_op_assign, 8 )
std_Ctraits * __thiscall std_Ctraits_op_assign( std_Ctraits *this, std_Ctraits *right )
{
    return this;
}

/* ?atan2@?$_Ctraits@M@std@@SAMMM@Z -> public: static float __cdecl std::_Ctraits<float>::atan2(float,float) */
float __cdecl std_Ctraits_float_atan2( float y, float x ) { return atan2f( y, x ); }

/* ?atan2@?$_Ctraits@N@std@@SANNN@Z -> public: static double __cdecl std::_Ctraits<double>::atan2(double,double) */
double __cdecl std_Ctraits_double_atan2( double y, double x ) { return atan2( y, x ); }

/* ?atan2@?$_Ctraits@O@std@@SAOOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::atan2(long double,long double) */
LDOUBLE __cdecl std_Ctraits_long_double_atan2( LDOUBLE y, LDOUBLE x ) { return atan2( y, x ); }

/* ?cos@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::cos(float) */
float __cdecl std_Ctraits_float_cos( float x ) { return cosf( x ); }

/* ?cos@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::cos(double) */
double __cdecl std_Ctraits_double_cos( double x ) { return cos( x ); }

/* ?cos@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::cos(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_cos( LDOUBLE x ) { return cos( x ); }

/* ?exp@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::exp(float) */
float __cdecl std_Ctraits_float_exp( float x ) { return expf( x ); }

/* ?exp@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::exp(double) */
double __cdecl std_Ctraits_double_exp( double x ) { return exp( x ); }

/* ?exp@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::exp(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_exp( LDOUBLE x ) { return exp( x ); }

/* ?ldexp@?$_Ctraits@M@std@@SAMMH@Z -> public: static float __cdecl std::_Ctraits<float>::ldexp(float,int) */
float __cdecl std_Ctraits_float_ldexp( float x, int y ) { return ldexpf( x, y ); }

/* ?ldexp@?$_Ctraits@N@std@@SANNH@Z -> public: static double __cdecl std::_Ctraits<double>::ldexp(double,int) */
double __cdecl std_Ctraits_double_ldexp( double x, int y ) { return ldexp( x, y ); }

/* ?ldexp@?$_Ctraits@O@std@@SAOOH@Z -> public: static long double __cdecl std::_Ctraits<long double>::ldexp(long double,int) */
LDOUBLE __cdecl std_Ctraits_long_double_ldexp( LDOUBLE x, int y ) { return ldexp( x, y ); }

/* ?log@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::log(float) */
float __cdecl std_Ctraits_float_log( float x ) { return logf( x ); }

/* ?log@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::log(double) */
double __cdecl std_Ctraits_double_log( double x ) { return log( x ); }

/* ?log@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::log(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_log( LDOUBLE x ) { return log( x ); }

/* ?pow@?$_Ctraits@M@std@@SAMMM@Z -> public: static float __cdecl std::_Ctraits<float>::pow(float,float) */
float __cdecl std_Ctraits_float_pow( float x, float y ) { return powf( x, y ); }

/* ?pow@?$_Ctraits@N@std@@SANNN@Z -> public: static double __cdecl std::_Ctraits<double>::pow(double,double) */
double __cdecl std_Ctraits_double_pow( double x, double y ) { return pow( x, y ); }

/* ?pow@?$_Ctraits@O@std@@SAOOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::pow(long double,long double) */
LDOUBLE __cdecl std_Ctraits_long_double_pow( LDOUBLE x, LDOUBLE y ) { return pow( x, y ); }

/* ?sin@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::sin(float) */
float __cdecl std_Ctraits_float_sin( float x ) { return sinf( x ); }

/* ?sin@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::sin(double) */
double __cdecl std_Ctraits_double_sin( double x ) { return sin( x ); }

/* ?sin@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::sin(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_sin( LDOUBLE x ) { return sin( x ); }

/* ?sqrt@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::sqrt(float) */
float __cdecl std_Ctraits_float_sqrt( float x ) { return sqrtf( x ); }

/* ?sqrt@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::sqrt(double) */
double __cdecl std_Ctraits_double_sqrt( double x ) { return sqrt( x ); }

/* ?sqrt@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::sqrt(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_sqrt( LDOUBLE x ) { return sqrt( x ); }

/* ?tan@?$_Ctraits@M@std@@SAMM@Z -> public: static float __cdecl std::_Ctraits<float>::tan(float) */
float __cdecl std_Ctraits_float_tan( float x ) { return tanf( x ); }

/* ?tan@?$_Ctraits@N@std@@SANN@Z -> public: static double __cdecl std::_Ctraits<double>::tan(double) */
double __cdecl std_Ctraits_double_tan( double x ) { return tan( x ); }

/* ?tan@?$_Ctraits@O@std@@SAOO@Z -> public: static long double __cdecl std::_Ctraits<long double>::tan(long double) */
LDOUBLE __cdecl std_Ctraits_long_double_tan( LDOUBLE x ) { return tan( x ); }
