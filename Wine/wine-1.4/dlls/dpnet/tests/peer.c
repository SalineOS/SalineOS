/*
 * Copyright 2011 Louis Lenders
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

#define INITGUID
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>

#include <dplay8.h>
#include "wine/test.h"

static char *show_guid(const GUID *guid, char *buf)
{
    sprintf(buf,
        "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        guid->Data1, guid->Data2, guid->Data3,
        guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
        guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

    return buf;
}

static IDirectPlay8Peer* peer = NULL;

static HRESULT WINAPI DirectPlayMessageHandler(PVOID context, DWORD message_id, PVOID buffer)
{
    return S_OK;
}

static void test_init_dp(void)
{
    HRESULT hr;

    hr = CoInitialize(0);
    ok(hr == S_OK, "CoInitialize failed with %x\n", hr);

    hr = CoCreateInstance(&CLSID_DirectPlay8Peer, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlay8Peer, (void **)&peer);
    ok(hr == S_OK, "CoCreateInstance failed with 0x%x\n", hr);

    hr = IDirectPlay8Peer_Initialize(peer, NULL, DirectPlayMessageHandler, 0);
    ok(hr == S_OK, "IDirectPlay8Peer_Initialize failed with %x\n", hr);
}

static void test_enum_service_providers(void)
{
    DPN_SERVICE_PROVIDER_INFO *serv_prov_info;
    DWORD items, size;
    DWORD i;
    HRESULT hr;
    char guid[39] ;

    size = 0;
    items = 0;

    hr = IDirectPlay8Peer_EnumServiceProviders(peer, NULL, NULL, NULL, &size, &items, 0);
    ok(hr == DPNERR_BUFFERTOOSMALL, "IDirectPlay8Peer_EnumServiceProviders failed with %x\n", hr);
    ok(size != 0, "size is unexpectedly 0\n");

    serv_prov_info = HeapAlloc(GetProcessHeap(), 0, size);

    hr = IDirectPlay8Peer_EnumServiceProviders(peer, NULL, NULL, serv_prov_info, &size, &items, 0);
    ok(hr == S_OK, "IDirectPlay8Peer_EnumServiceProviders failed with %x\n", hr);
    ok(items != 0, "Found unexpectedly no service providers\n");

    trace("number of items found: %d\n", items);

    for (i=0;i<items;i++)
    {
        trace("Found Service Provider: %s\n", wine_dbgstr_w(serv_prov_info->pwszName));
        trace("Found guid: %s\n", show_guid(&serv_prov_info->guid, guid));

        serv_prov_info++;
    }

    serv_prov_info -= items; /* set pointer back */
    ok(HeapFree(GetProcessHeap(), 0, serv_prov_info), "Failed freeing server provider info\n");

    size = 0;
    items = 0;

    hr = IDirectPlay8Peer_EnumServiceProviders(peer, &CLSID_DP8SP_TCPIP, NULL, NULL, &size, &items, 0);
    ok(hr == DPNERR_BUFFERTOOSMALL, "IDirectPlay8Peer_EnumServiceProviders failed with %x\n", hr);
    ok(size != 0, "size is unexpectedly 0\n");

    serv_prov_info = HeapAlloc(GetProcessHeap(), 0, size);

    hr = IDirectPlay8Peer_EnumServiceProviders(peer, &CLSID_DP8SP_TCPIP, NULL, serv_prov_info, &size, &items, 0);
    ok(hr == S_OK, "IDirectPlay8Peer_EnumServiceProviders failed with %x\n", hr);
    ok(items != 0, "Found unexpectedly no adapter\n");


    for (i=0;i<items;i++)
    {
        trace("Found adapter: %s\n", wine_dbgstr_w(serv_prov_info->pwszName));
        trace("Found adapter guid: %s\n", show_guid(&serv_prov_info->guid, guid));

        serv_prov_info++;
    }

    serv_prov_info -= items; /* set pointer back */
    ok(HeapFree(GetProcessHeap(), 0, serv_prov_info), "Failed freeing server provider info\n");
}

static void test_get_sp_caps(void)
{
    DPN_SP_CAPS caps;
    HRESULT hr;

    memset(&caps, 0, sizeof(DPN_SP_CAPS));

    hr = IDirectPlay8Peer_GetSPCaps(peer, &CLSID_DP8SP_TCPIP, &caps, 0);
    ok(hr == DPNERR_INVALIDPARAM, "GetSPCaps unexpectedly returned %x\n", hr);

    caps.dwSize = sizeof(DPN_SP_CAPS);

    hr = IDirectPlay8Peer_GetSPCaps(peer, &CLSID_DP8SP_TCPIP, &caps, 0);
    ok(hr == DPN_OK, "GetSPCaps failed with %x\n", hr);

    ok((caps.dwFlags &
        (DPNSPCAPS_SUPPORTSDPNSRV | DPNSPCAPS_SUPPORTSBROADCAST | DPNSPCAPS_SUPPORTSALLADAPTERS)) ==
       (DPNSPCAPS_SUPPORTSDPNSRV | DPNSPCAPS_SUPPORTSBROADCAST | DPNSPCAPS_SUPPORTSALLADAPTERS),
       "unexpected flags %x\n", caps.dwFlags);
    ok(caps.dwNumThreads >= 3, "got %d\n", caps.dwNumThreads);
    ok(caps.dwDefaultEnumCount == 5, "expected 5, got %d\n", caps.dwDefaultEnumCount);
    ok(caps.dwDefaultEnumRetryInterval == 1500, "expected 1500, got %d\n", caps.dwDefaultEnumRetryInterval);
    ok(caps.dwDefaultEnumTimeout == 1500, "expected 1500, got %d\n", caps.dwDefaultEnumTimeout);
    ok(caps.dwMaxEnumPayloadSize == 983, "expected 983, got %d\n", caps.dwMaxEnumPayloadSize);
    ok(caps.dwBuffersPerThread == 1, "expected 1, got %d\n", caps.dwBuffersPerThread);
    ok(caps.dwSystemBufferSize == 8192, "expected 8192, got %d\n", caps.dwSystemBufferSize);
}

static void test_cleanup_dp(void)
{
    HRESULT hr;

    hr = IDirectPlay8Peer_Close(peer, 0);
    ok(hr == S_OK, "IDirectPlay8Peer_Close failed with %x\n", hr);

    hr = IDirectPlay8Peer_Release(peer);
    ok(hr == S_OK, "IDirectPlay8Peer_Release failed with %x\n", hr);

    CoUninitialize();
}

START_TEST(peer)
{
    test_init_dp();
    test_enum_service_providers();
    test_get_sp_caps();
    test_cleanup_dp();
}
