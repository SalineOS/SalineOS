/*
 * Helper functions for ntdll
 *
 * Copyright 2000 Juergen Schmied
 * Copyright 2010 Marcus Meissner
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

#include <time.h>
#include <math.h>
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include "wine/library.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

#if defined(__GNUC__) && defined(__i386__)
#define DO_FPU(x,y) __asm__ __volatile__( x " %0;fwait" : "=m" (y) : )
#define POP_FPU(x) DO_FPU("fstpl",x)
#endif

LPCSTR debugstr_ObjectAttributes(const OBJECT_ATTRIBUTES *oa)
{
    if (!oa) return "<null>";
    return wine_dbg_sprintf( "{name=%s, attr=0x%08x, hRoot=%p, sd=%p}\n",
                             debugstr_us(oa->ObjectName), oa->Attributes,
                             oa->RootDirectory, oa->SecurityDescriptor );
}

LPCSTR debugstr_us( const UNICODE_STRING *us )
{
    if (!us) return "<null>";
    return debugstr_wn(us->Buffer, us->Length / sizeof(WCHAR));
}

/*********************************************************************
 *                  _ftol   (NTDLL.@)
 *
 * VERSION
 *	[GNUC && i386]
 */
#if defined(__GNUC__) && defined(__i386__)
LONGLONG CDECL NTDLL__ftol(void)
{
	/* don't just do DO_FPU("fistp",retval), because the rounding
	 * mode must also be set to "round towards zero"... */
	double fl;
	POP_FPU(fl);
	return (LONGLONG)fl;
}
#endif /* defined(__GNUC__) && defined(__i386__) */

/*********************************************************************
 *                  _ftol   (NTDLL.@)
 *
 * FIXME
 *	Should be register function
 * VERSION
 *	[!GNUC && i386]
 */
#if !defined(__GNUC__) && defined(__i386__)
LONGLONG CDECL NTDLL__ftol(double fl)
{
	FIXME("should be register function\n");
	return (LONGLONG)fl;
}
#endif /* !defined(__GNUC__) && defined(__i386__) */

/*********************************************************************
 *                  _CIpow   (NTDLL.@)
 * VERSION
 *	[GNUC && i386]
 */
#if defined(__GNUC__) && defined(__i386__)
double CDECL NTDLL__CIpow(void)
{
	double x,y;
	POP_FPU(y);
	POP_FPU(x);
	return pow(x,y);
}
#endif /* defined(__GNUC__) && defined(__i386__) */


/*********************************************************************
 *                  _CIpow   (NTDLL.@)
 *
 * FIXME
 *	Should be register function
 *
 * VERSION
 *	[!GNUC && i386]
 */
#if !defined(__GNUC__) && defined(__i386__)
double CDECL NTDLL__CIpow(double x,double y)
{
	FIXME("should be register function\n");
	return pow(x,y);
}
#endif /* !defined(__GNUC__) && defined(__i386__) */

/*********************************************************************
 *                  wine_get_version   (NTDLL.@)
 */
const char * CDECL NTDLL_wine_get_version(void)
{
    return wine_get_version();
}

/*********************************************************************
 *                  wine_get_build_id   (NTDLL.@)
 */
const char * CDECL NTDLL_wine_get_build_id(void)
{
    return wine_get_build_id();
}

/*********************************************************************
 *                  wine_get_host_version   (NTDLL.@)
 */
void CDECL NTDLL_wine_get_host_version( const char **sysname, const char **release )
{
#ifdef HAVE_SYS_UTSNAME_H
    static struct utsname buf;
    static int init_done;

    if (!init_done)
    {
        uname( &buf );
        init_done = 1;
    }
    if (sysname) *sysname = buf.sysname;
    if (release) *release = buf.release;
#else
    if (sysname) *sysname = "";
    if (release) *release = "";
#endif
}

/*********************************************************************
 *                  abs   (NTDLL.@)
 */
int CDECL NTDLL_abs( int i )
{
    return abs( i );
}

/*********************************************************************
 *                  labs   (NTDLL.@)
 */
LONG CDECL NTDLL_labs( LONG i )
{
    return labs( i );
}

/*********************************************************************
 *                  atan   (NTDLL.@)
 */
double CDECL NTDLL_atan( double d )
{
    return atan( d );
}

/*********************************************************************
 *                  ceil   (NTDLL.@)
 */
double CDECL NTDLL_ceil( double d )
{
    return ceil( d );
}

/*********************************************************************
 *                  cos   (NTDLL.@)
 */
double CDECL NTDLL_cos( double d )
{
    return cos( d );
}

/*********************************************************************
 *                  fabs   (NTDLL.@)
 */
double CDECL NTDLL_fabs( double d )
{
    return fabs( d );
}

/*********************************************************************
 *                  floor   (NTDLL.@)
 */
double CDECL NTDLL_floor( double d )
{
    return floor( d );
}

/*********************************************************************
 *                  log   (NTDLL.@)
 */
double CDECL NTDLL_log( double d )
{
    return log( d );
}

/*********************************************************************
 *                  pow   (NTDLL.@)
 */
double CDECL NTDLL_pow( double x, double y )
{
    return pow( x, y );
}

/*********************************************************************
 *                  sin   (NTDLL.@)
 */
double CDECL NTDLL_sin( double d )
{
    return sin( d );
}

/*********************************************************************
 *                  sqrt   (NTDLL.@)
 */
double CDECL NTDLL_sqrt( double d )
{
    return sqrt( d );
}

/*********************************************************************
 *                  tan   (NTDLL.@)
 */
double CDECL NTDLL_tan( double d )
{
    return tan( d );
}


static void
NTDLL_mergesort( void *arr, void *barr, size_t elemsize, int(__cdecl *compar)(const void *, const void *),
                 size_t left, size_t right )
{
    if(right>left) {
        size_t i, j, k, m;
        m=left+(right-left)/2;
        NTDLL_mergesort( arr, barr, elemsize, compar, left, m);
        NTDLL_mergesort( arr, barr, elemsize, compar, m+1, right);

#define X(a,i) ((char*)a+elemsize*(i))
        for (k=left, i=left, j=m+1; i<=m && j<=right; k++) {
            if (compar(X(arr, i), X(arr,j)) <= 0) {
                memcpy(X(barr,k), X(arr, i), elemsize);
                i++;
            } else {
                memcpy(X(barr,k), X(arr, j), elemsize);
                j++;
            }
        }
        if (i<=m)
            memcpy(X(barr,k), X(arr,i), (m-i+1)*elemsize);
        else
            memcpy(X(barr,k), X(arr,j), (right-j+1)*elemsize);

        memcpy(X(arr, left), X(barr, left), (right-left+1)*elemsize);
    }
#undef X
}

/*********************************************************************
 *                  qsort   (NTDLL.@)
 */
void __cdecl NTDLL_qsort( void *base, size_t nmemb, size_t size,
                          int(__cdecl *compar)(const void *, const void *) )
{
    void *secondarr;
    if (nmemb < 2 || size == 0) return;
    secondarr = RtlAllocateHeap (GetProcessHeap(), 0, nmemb*size);
    NTDLL_mergesort( base, secondarr, size, compar, 0, nmemb-1 );
    RtlFreeHeap (GetProcessHeap(),0, secondarr);
}

/*********************************************************************
 *                  bsearch   (NTDLL.@)
 */
void * __cdecl
NTDLL_bsearch( const void *key, const void *base, size_t nmemb,
               size_t size, int (__cdecl *compar)(const void *, const void *) )
{
    ssize_t min = 0;
    ssize_t max = nmemb - 1;

    while (min <= max)
    {
        ssize_t cursor = (min + max) / 2;
        int ret = compar(key,(const char *)base+(cursor*size));
        if (!ret)
            return (char*)base+(cursor*size);
        if (ret < 0)
            max = cursor - 1;
        else
            min = cursor + 1;
    }
    return NULL;
}


/*********************************************************************
 *                  _lfind   (NTDLL.@)
 */
void * __cdecl _lfind( const void *key, const void *base, unsigned int *nmemb,
                       size_t size, int(__cdecl *compar)(const void *, const void *) )
{
    size_t i, n = *nmemb;

    for (i=0;i<n;i++)
        if (!compar(key,(char*)base+(size*i)))
            return (char*)base+(size*i);
    return NULL;
}
