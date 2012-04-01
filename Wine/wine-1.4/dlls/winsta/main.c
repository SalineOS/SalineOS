/*
 * Winstation library implementation
 *
 * Copyright 2011 Austin English
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

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winsta);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        default:
            break;
    }

    return TRUE;
}

BOOLEAN WINAPI WinStationQueryInformationA( HANDLE server, ULONG logon_id, WINSTATIONINFOCLASS class,
                                            void *info, ULONG len, ULONG *ret_len )
{
    FIXME( "%p %u %u %p %u %p\n", server, logon_id, class, info, len, ret_len );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

BOOLEAN WINAPI WinStationQueryInformationW( HANDLE server, ULONG logon_id, WINSTATIONINFOCLASS class,
                                            void *info, ULONG len, ULONG *ret_len )
{
    FIXME( "%p %u %u %p %u %p\n", server, logon_id, class, info, len, ret_len );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

BOOL WINAPI WinStationGetProcessSid( PVOID a, HANDLE server, DWORD process_id, PFILETIME process_start_time,
                                     PBYTE process_user_sid, PDWORD sid_size)
{
    FIXME( "(%p, %p, %d, %p, %p, %p): stub\n", a, server, process_id, process_start_time, process_user_sid, sid_size);
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}
