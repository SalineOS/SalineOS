/*
 * Copyright (C) 2006-2007 Stefan Dösinger(For CodeWeavers)
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

#define COBJMACROS
#include <d3d9.h>
#include "wine/test.h"

static IDirect3D9 *(WINAPI *pDirect3DCreate9)(UINT);

struct queryInfo
{
    D3DQUERYTYPE type;      /* Query to test */
    BOOL foundSupported;    /* If at least one windows driver has been found supporting this query */
    BOOL foundUnsupported;  /* If at least one windows driver has been found which does not support this query */
};

/* When running running this test on windows reveals any differences regarding known supported / unsupported queries,
 * change this table.
 *
 * When marking a query known supported or known unsupported please write one card which supports / does not support
 * the query.
 */
static struct queryInfo queries[] =
{
    {D3DQUERYTYPE_VCACHE,               TRUE /* geforce 6600 */,    TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_RESOURCEMANAGER,      FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_VERTEXSTATS,          FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_EVENT,                TRUE /* geforce 2 mx */,    TRUE /* ati mach64 */   },
    {D3DQUERYTYPE_OCCLUSION,            TRUE /* radeon M9 */,       TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_TIMESTAMP,            TRUE /* geforce 6600 */,    TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_TIMESTAMPDISJOINT,    TRUE /* geforce 6600 */,    TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_TIMESTAMPFREQ,        TRUE /* geforce 6600 */,    TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_PIPELINETIMINGS,      FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_INTERFACETIMINGS,     FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_VERTEXTIMINGS,        FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_PIXELTIMINGS,         FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_BANDWIDTHTIMINGS,     FALSE,                      TRUE /* geforce 2 mx */ },
    {D3DQUERYTYPE_CACHEUTILIZATION,     FALSE,                      TRUE /* geforce 2 mx */ },
};

static const char *queryName(D3DQUERYTYPE type)
{
    switch(type)
    {
        case D3DQUERYTYPE_VCACHE:               return "D3DQUERYTYPE_VCACHE";
        case D3DQUERYTYPE_RESOURCEMANAGER:      return "D3DQUERYTYPE_RESOURCEMANAGER";
        case D3DQUERYTYPE_VERTEXSTATS:          return "D3DQUERYTYPE_VERTEXSTATS";
        case D3DQUERYTYPE_EVENT:                return "D3DQUERYTYPE_EVENT";
        case D3DQUERYTYPE_OCCLUSION:            return "D3DQUERYTYPE_OCCLUSION";
        case D3DQUERYTYPE_TIMESTAMP:            return "D3DQUERYTYPE_TIMESTAMP";
        case D3DQUERYTYPE_TIMESTAMPDISJOINT:    return "D3DQUERYTYPE_TIMESTAMPDISJOINT";
        case D3DQUERYTYPE_TIMESTAMPFREQ:        return "D3DQUERYTYPE_TIMESTAMPFREQ";
        case D3DQUERYTYPE_PIPELINETIMINGS:      return "D3DQUERYTYPE_PIPELINETIMINGS";
        case D3DQUERYTYPE_INTERFACETIMINGS:     return "D3DQUERYTYPE_INTERFACETIMINGS";
        case D3DQUERYTYPE_VERTEXTIMINGS:        return "D3DQUERYTYPE_VERTEXTIMINGS";
        case D3DQUERYTYPE_PIXELTIMINGS:         return "D3DQUERYTYPE_PIXELTIMINGS";
        case D3DQUERYTYPE_BANDWIDTHTIMINGS:     return "D3DQUERYTYPE_BANDWIDTHTIMINGS";
        case D3DQUERYTYPE_CACHEUTILIZATION:     return "D3DQUERYTYPE_CACHEUTILIZATION";
        default: return "Unexpected query type";
    }
}

static void test_query_support(IDirect3D9 *pD3d, HWND hwnd)
{

    HRESULT               hr;

    IDirect3DDevice9      *pDevice = NULL;
    D3DPRESENT_PARAMETERS d3dpp;
    D3DDISPLAYMODE        d3ddm;
    unsigned int i;
    IDirect3DQuery9       *pQuery = NULL;
    BOOL supported;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(SUCCEEDED(hr) || hr == D3DERR_NOTAVAILABLE, "Failed to create IDirect3D9Device (%08x)\n", hr);
    if (FAILED(hr))
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    for(i = 0; i < sizeof(queries) / sizeof(queries[0]); i++)
    {
        hr = IDirect3DDevice9_CreateQuery(pDevice, queries[i].type, NULL);
        ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE,
           "IDirect3DDevice9_CreateQuery returned unexpected return value %08x for query %s\n", hr, queryName(queries[i].type));

        supported = (hr == D3D_OK ? TRUE : FALSE);
        trace("query %s is %s\n", queryName(queries[i].type), supported ? "supported" : "not supported");

        ok(!(supported == TRUE && queries[i].foundSupported == FALSE),
            "Query %s is supported on this system, but was not found supported before\n",
            queryName(queries[i].type));
        ok(!(supported == FALSE && queries[i].foundUnsupported == FALSE),
            "Query %s is not supported on this system, but was found to be supported on all other systems tested before\n",
            queryName(queries[i].type));

        hr = IDirect3DDevice9_CreateQuery(pDevice, queries[i].type, &pQuery);
        ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE,
           "IDirect3DDevice9_CreateQuery returned unexpected return value %08x for query %s\n", hr, queryName(queries[i].type));
        ok(!(supported && !pQuery), "Query %s was claimed to be supported, but can't be created\n", queryName(queries[i].type));
        ok(!(!supported && pQuery), "Query %s was claimed not to be supported, but can be created\n", queryName(queries[i].type));
        if(pQuery)
        {
            IDirect3DQuery9_Release(pQuery);
            pQuery = NULL;
        }
    }

cleanup:
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice9_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
}

static void test_occlusion_query_states(IDirect3D9 *pD3d, HWND hwnd)
{

    HRESULT               hr;

    IDirect3DDevice9      *pDevice = NULL;
    D3DPRESENT_PARAMETERS d3dpp;
    D3DDISPLAYMODE        d3ddm;
    IDirect3DQuery9       *pQuery = NULL;
    BYTE *data = NULL;
    float point[3] = {0.0, 0.0, 0.0};
    unsigned int count = 0;

    IDirect3D9_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D9_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(SUCCEEDED(hr) || hr == D3DERR_NOTAVAILABLE, "Failed to create IDirect3D9Device (%08x)\n", hr);
    if (FAILED(hr))
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice9_CreateQuery(pDevice, D3DQUERYTYPE_OCCLUSION, &pQuery);
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE,
       "IDirect3DDevice9_CreateQuery returned unexpected return value %08x\n", hr);
    if(!pQuery) {
        skip("Occlusion queries not supported\n");
        goto cleanup;
    }

    data = HeapAlloc(GetProcessHeap(), 0, IDirect3DQuery9_GetDataSize(pQuery));

    hr = IDirect3DQuery9_GetData(pQuery, NULL, 0, D3DGETDATA_FLUSH);
    ok(hr == S_OK, "IDirect3DQuery9_GetData(NULL) on a new query returned %08x\n", hr);
    hr = IDirect3DQuery9_GetData(pQuery, data, IDirect3DQuery9_GetDataSize(pQuery), D3DGETDATA_FLUSH);
    ok(hr == S_OK, "IDirect3DQuery9_GetData on a new query returned %08x\n", hr);

    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_END);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_END) on a new not yet started query returned %08x\n", hr);

    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_BEGIN);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_BEGIN) on a new not yet started query returned %08x\n", hr);

    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_BEGIN);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DQUERY_BEGIN) on a started query returned %08x\n", hr);

    *((DWORD *)data) = 0x12345678;
    hr = IDirect3DQuery9_GetData(pQuery, NULL, 0, D3DGETDATA_FLUSH);
    ok(hr == S_FALSE || hr == D3D_OK, "IDirect3DQuery9_GetData(NULL) on a started query returned %08x\n", hr);
    hr = IDirect3DQuery9_GetData(pQuery, data, IDirect3DQuery9_GetDataSize(pQuery), D3DGETDATA_FLUSH);
    ok(hr == S_FALSE || hr == D3D_OK, "IDirect3DQuery9_GetData on a started query returned %08x\n", hr);
    if (hr == D3D_OK)
    {
        DWORD value = *((DWORD *)data);
        ok(value == 0, "The unfinished query returned %u, expected 0\n", value);
    }

    hr = IDirect3DDevice9_SetFVF(pDevice, D3DFVF_XYZ);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetFVF returned %08x\n", hr);
    hr = IDirect3DDevice9_BeginScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice9_BeginScene returned %08x\n", hr);
    if(SUCCEEDED(hr)) {
        hr = IDirect3DDevice9_DrawPrimitiveUP(pDevice, D3DPT_POINTLIST, 1, point, 3 * sizeof(float));
        ok(hr == D3D_OK, "IDirect3DDevice9_DrawPrimitiveUP returned %08x\n", hr);
        hr = IDirect3DDevice9_EndScene(pDevice);
        ok(hr == D3D_OK, "IDirect3DDevice9_EndScene returned %08x\n", hr);
    }

    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_END);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_END) on a started query returned %08x\n", hr);

    hr = S_FALSE;
    while(hr == S_FALSE && count < 500) {
        hr = IDirect3DQuery9_GetData(pQuery, NULL, 0, D3DGETDATA_FLUSH);
        ok(hr == S_OK || hr == S_FALSE, "IDirect3DQuery9_GetData on a ended query returned %08x\n", hr);
        count++;
        if(hr == S_FALSE) Sleep(10);
    }
    ok(hr == S_OK, "Occlusion query did not finish\n");

    hr = IDirect3DQuery9_GetData(pQuery, data, IDirect3DQuery9_GetDataSize(pQuery), D3DGETDATA_FLUSH);
    ok(hr == S_OK, "IDirect3DQuery9_GetData on a ended query returned %08x\n", hr);
    hr = IDirect3DQuery9_GetData(pQuery, data, IDirect3DQuery9_GetDataSize(pQuery), D3DGETDATA_FLUSH);
    ok(hr == S_OK, "IDirect3DQuery9_GetData a 2nd time on a ended query returned %08x\n", hr);

    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_BEGIN);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_BEGIN) on a new not yet started query returned %08x\n", hr);
    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_END);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_END) on a started query returned %08x\n", hr);
    hr = IDirect3DQuery9_Issue(pQuery, D3DISSUE_END);
    ok(hr == D3D_OK, "IDirect3DQuery9_Issue(D3DISSUE_END) on a ended query returned %08x\n", hr);

cleanup:
    HeapFree(GetProcessHeap(), 0, data);
    if (pQuery) IDirect3DQuery9_Release(pQuery);
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice9_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
}

START_TEST(query)
{
    HMODULE d3d9_handle = LoadLibraryA( "d3d9.dll" );
    if (!d3d9_handle)
    {
        skip("Could not load d3d9.dll\n");
        return;
    }

    pDirect3DCreate9 = (void *)GetProcAddress( d3d9_handle, "Direct3DCreate9" );
    ok(pDirect3DCreate9 != NULL, "Failed to get address of Direct3DCreate9\n");
    if (pDirect3DCreate9)
    {
        IDirect3D9            *pD3d = NULL;
        HWND                  hwnd = NULL;
        WNDCLASS wc = {0};
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = "d3d9_test_wc";
        RegisterClass(&wc);

        pD3d = pDirect3DCreate9( D3D_SDK_VERSION );
        if(!pD3d)
        {
            skip("Failed to create Direct3D9 object, not running tests\n");
            goto out;
        }
        hwnd = CreateWindow( wc.lpszClassName, "d3d9_test",
                WS_SYSMENU | WS_POPUP, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
        if(!hwnd)
        {
            skip("Failed to create window\n");
            goto out;
        }

        test_query_support(pD3d, hwnd);
        test_occlusion_query_states(pD3d, hwnd);

        out:
        if(pD3d) IDirect3D9_Release(pD3d);
        if(hwnd) DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, GetModuleHandleA(NULL));
    }
}
