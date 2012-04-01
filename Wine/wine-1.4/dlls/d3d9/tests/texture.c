/*
 * Copyright (C) 2006 Henri Verbeet
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

static HWND create_window(void)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = "d3d9_test_wc";
    RegisterClass(&wc);

    return CreateWindow("d3d9_test_wc", "d3d9_test",
            0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static IDirect3DDevice9 *init_d3d9(HMODULE d3d9_handle)
{
    IDirect3D9 * (__stdcall * d3d9_create)(UINT SDKVersion) = 0;
    IDirect3D9 *d3d9_ptr = 0;
    IDirect3DDevice9 *device_ptr = 0;
    D3DPRESENT_PARAMETERS present_parameters;
    HRESULT hr;

    d3d9_create = (void *)GetProcAddress(d3d9_handle, "Direct3DCreate9");
    ok(d3d9_create != NULL, "Failed to get address of Direct3DCreate9\n");
    if (!d3d9_create) return NULL;

    d3d9_ptr = d3d9_create(D3D_SDK_VERSION);
    if (!d3d9_ptr)
    {
        skip("could not create D3D9\n");
        return NULL;
    }

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = create_window();
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

    hr = IDirect3D9_CreateDevice(d3d9_ptr, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device_ptr);

    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D9_CreateDevice returned %#x\n", hr);
        return NULL;
    }

    return device_ptr;
}

static void test_texture_stage_state(IDirect3DDevice9 *device_ptr, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD expected)
{
    DWORD value;

    HRESULT hr = IDirect3DDevice9_GetTextureStageState(device_ptr, stage, type, &value);
    ok(SUCCEEDED(hr) && value == expected, "GetTextureStageState (stage %#x, type %#x) returned: hr %#x, value %#x. "
        "Expected hr %#x, value %#x\n", stage, type, hr, value, D3D_OK, expected);
}

/* Test the default texture stage state values */
static void test_texture_stage_states(IDirect3DDevice9 *device_ptr, int num_stages)
{
    int i;
    for (i = 0; i < num_stages; ++i)
    {
        test_texture_stage_state(device_ptr, i, D3DTSS_COLOROP, i ? D3DTOP_DISABLE : D3DTOP_MODULATE);
        test_texture_stage_state(device_ptr, i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        test_texture_stage_state(device_ptr, i, D3DTSS_COLORARG2, D3DTA_CURRENT);
        test_texture_stage_state(device_ptr, i, D3DTSS_ALPHAOP, i ? D3DTOP_DISABLE : D3DTOP_SELECTARG1);
        test_texture_stage_state(device_ptr, i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        test_texture_stage_state(device_ptr, i, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVMAT00, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVMAT01, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVMAT10, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVMAT11, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_TEXCOORDINDEX, i);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVLSCALE, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_BUMPENVLOFFSET, 0);
        test_texture_stage_state(device_ptr, i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        test_texture_stage_state(device_ptr, i, D3DTSS_COLORARG0, D3DTA_CURRENT);
        test_texture_stage_state(device_ptr, i, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
        test_texture_stage_state(device_ptr, i, D3DTSS_RESULTARG, D3DTA_CURRENT);
        test_texture_stage_state(device_ptr, i, D3DTSS_CONSTANT, 0);
    }
}

static void test_cube_texture_from_pool(IDirect3DDevice9 *device_ptr, DWORD caps, D3DPOOL pool, BOOL need_cap)
{
    IDirect3DCubeTexture9 *texture_ptr = NULL;
    HRESULT hr;

    hr = IDirect3DDevice9_CreateCubeTexture(device_ptr, 512, 1, 0, D3DFMT_X8R8G8B8, pool, &texture_ptr, NULL);

    if((caps & D3DPTEXTURECAPS_CUBEMAP) || !need_cap)
        ok(SUCCEEDED(hr), "hr=0x%.8x\n", hr);
    else
        ok(hr == D3DERR_INVALIDCALL, "hr=0x%.8x\n", hr);

    if(texture_ptr) IDirect3DCubeTexture9_Release(texture_ptr);
}

static void test_cube_texture_mipmap_gen(IDirect3DDevice9 *device_ptr)
{
    IDirect3DCubeTexture9 *texture_ptr = NULL;
    IDirect3D9 *d3d9;
    HRESULT hr;

    hr = IDirect3DDevice9_GetDirect3D(device_ptr, &d3d9);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetDirect3D returned 0x%08x\n", hr);

    hr = IDirect3D9_CheckDeviceFormat(d3d9, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
                                      D3DUSAGE_AUTOGENMIPMAP,
                                      D3DRTYPE_CUBETEXTURE, D3DFMT_X8R8G8B8);
    if(FAILED(hr))
    {
        skip("No cube mipmap generation support\n");
        return;
    }

    /* testing shows that autogenmipmap and rendertarget are mutually exclusive options */
    hr = IDirect3DDevice9_CreateCubeTexture(device_ptr, 64, 0, (D3DUSAGE_RENDERTARGET |
                                            D3DUSAGE_AUTOGENMIPMAP), D3DFMT_X8R8G8B8,
                                            D3DPOOL_DEFAULT, &texture_ptr, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture returned 0x%08x, expected 0x%08x\n",
       hr, D3D_OK);
    if (texture_ptr) IDirect3DCubeTexture9_Release(texture_ptr);
    texture_ptr = NULL;

    hr = IDirect3DDevice9_CreateCubeTexture(device_ptr, 64, 0,
                                            D3DUSAGE_AUTOGENMIPMAP, D3DFMT_X8R8G8B8,
                                            D3DPOOL_MANAGED, &texture_ptr, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture failed (0x%08x)\n", hr);
    if (texture_ptr) IDirect3DCubeTexture9_Release(texture_ptr);
    texture_ptr = NULL;
}

static void test_cube_textures(IDirect3DDevice9 *device_ptr, DWORD caps)
{
    test_cube_texture_from_pool(device_ptr, caps, D3DPOOL_DEFAULT, TRUE);
    test_cube_texture_from_pool(device_ptr, caps, D3DPOOL_MANAGED, TRUE);
    test_cube_texture_from_pool(device_ptr, caps, D3DPOOL_SYSTEMMEM, TRUE);
    test_cube_texture_from_pool(device_ptr, caps, D3DPOOL_SCRATCH, FALSE);
    test_cube_texture_mipmap_gen(device_ptr);
}

static void test_mipmap_gen(IDirect3DDevice9 *device)
{
    HRESULT hr;
    IDirect3D9 *d3d9;
    IDirect3DTexture9 *texture = NULL;
    IDirect3DSurface9 *surface;
    DWORD levels;
    D3DSURFACE_DESC desc;
    int i;
    D3DLOCKED_RECT lr;

    hr = IDirect3DDevice9_GetDirect3D(device, &d3d9);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetDirect3D returned %#x\n", hr);

    hr = IDirect3D9_CheckDeviceFormat(d3d9, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
                                      D3DUSAGE_AUTOGENMIPMAP,
                                      D3DRTYPE_TEXTURE, D3DFMT_X8R8G8B8);
    if(FAILED(hr))
    {
        skip("No mipmap generation support\n");
        return;
    }

    /* testing shows that autogenmipmap and rendertarget are mutually exclusive options */
    hr = IDirect3DDevice9_CreateTexture(device, 64, 64, 0, (D3DUSAGE_RENDERTARGET |
                                        D3DUSAGE_AUTOGENMIPMAP), D3DFMT_X8R8G8B8,
                                        D3DPOOL_DEFAULT, &texture, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture returned 0x%08x, expected 0x%08x\n",
       hr, D3D_OK);
    if (texture) IDirect3DTexture9_Release(texture);
    texture = NULL;

    hr = IDirect3DDevice9_CreateTexture(device, 64, 64, 0, D3DUSAGE_AUTOGENMIPMAP,
                                        D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture failed(%08x)\n", hr);

    if (SUCCEEDED(hr))
    {
        D3DTEXTUREFILTERTYPE fltt;
        fltt = IDirect3DTexture9_GetAutoGenFilterType(texture);
        ok(D3DTEXF_LINEAR == fltt /* || broken(D3DTEXF_POINT == fltt)*/,
           "GetAutoGenFilterType returned default %d\n", fltt);
        hr = IDirect3DTexture9_SetAutoGenFilterType(texture, D3DTEXF_NONE);
        todo_wine ok(hr == D3DERR_INVALIDCALL, "SetAutoGenFilterType D3DTEXF_NONE returned %08x\n", hr);
        hr = IDirect3DTexture9_SetAutoGenFilterType(texture, D3DTEXF_ANISOTROPIC);
        ok(hr == D3D_OK, "SetAutoGenFilterType D3DTEXF_ANISOTROPIC returned %08x\n", hr);
        fltt = IDirect3DTexture9_GetAutoGenFilterType(texture);
        ok(D3DTEXF_ANISOTROPIC == fltt, "GetAutoGenFilterType returned %d\n", fltt);
        hr = IDirect3DTexture9_SetAutoGenFilterType(texture, D3DTEXF_LINEAR);
        ok(hr == D3D_OK, "SetAutoGenFilterType D3DTEXF_LINEAR returned %08x\n", hr);
    }
    levels = IDirect3DTexture9_GetLevelCount(texture);
    ok(levels == 1, "Got %d levels, expected 1\n", levels);

    for(i = 0; i < 6 /* 64 = 2 ^ 6 */; i++)
    {
        surface = NULL;
        hr = IDirect3DTexture9_GetSurfaceLevel(texture, i, &surface);
        ok(hr == (i == 0 ? D3D_OK : D3DERR_INVALIDCALL),
           "GetSurfaceLevel on level %d returned %#x\n", i, hr);
        if(surface) IDirect3DSurface9_Release(surface);

        hr = IDirect3DTexture9_GetLevelDesc(texture, i, &desc);
        ok(hr == (i == 0 ? D3D_OK : D3DERR_INVALIDCALL),
           "GetLevelDesc on level %d returned %#x\n", i, hr);

        hr = IDirect3DTexture9_LockRect(texture, i, &lr, NULL, 0);
        ok(hr == (i == 0 ? D3D_OK : D3DERR_INVALIDCALL),
           "LockRect on level %d returned %#x\n", i, hr);
        if(SUCCEEDED(hr))
        {
            hr = IDirect3DTexture9_UnlockRect(texture, i);
            ok(hr == D3D_OK, "Unlock returned %08x\n", hr);
        }
    }
    IDirect3DTexture9_Release(texture);

    hr = IDirect3DDevice9_CreateTexture(device, 64, 64, 2 /* levels */, D3DUSAGE_AUTOGENMIPMAP,
                                        D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_CreateTexture(levels = 2) returned %08x\n", hr);
    hr = IDirect3DDevice9_CreateTexture(device, 64, 64, 6 /* levels */, D3DUSAGE_AUTOGENMIPMAP,
                                        D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice9_CreateTexture(levels = 6) returned %08x\n", hr);

    hr = IDirect3DDevice9_CreateTexture(device, 64, 64, 1 /* levels */, D3DUSAGE_AUTOGENMIPMAP,
                                        D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &texture, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture(levels = 1) returned %08x\n", hr);
    levels = IDirect3DTexture9_GetLevelCount(texture);
    ok(levels == 1, "Got %d levels, expected 1\n", levels);
    IDirect3DTexture9_Release(texture);
}

static void test_filter(IDirect3DDevice9 *device) {
    HRESULT hr;
    IDirect3DTexture9 *texture;
    IDirect3D9 *d3d9;
    DWORD passes = 0;
    unsigned int i;
    struct filter_tests {
        DWORD magfilter, minfilter, mipfilter;
        BOOL has_texture;
        HRESULT result;
    } tests[] = {
        { D3DTEXF_NONE,   D3DTEXF_NONE,   D3DTEXF_NONE,   FALSE, D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_POINT,  D3DTEXF_NONE,   D3DTEXF_NONE,   FALSE, D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_NONE,   D3DTEXF_POINT,  D3DTEXF_NONE,   FALSE, D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_POINT,  D3DTEXF_POINT,  D3DTEXF_NONE,   FALSE, D3D_OK },
        { D3DTEXF_POINT,  D3DTEXF_POINT,  D3DTEXF_POINT,  FALSE, D3D_OK },

        { D3DTEXF_NONE,   D3DTEXF_NONE,   D3DTEXF_NONE,   TRUE,  D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_POINT,  D3DTEXF_NONE,   D3DTEXF_NONE,   TRUE,  D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_POINT,  D3DTEXF_POINT,  D3DTEXF_NONE,   TRUE,  D3D_OK },
        { D3DTEXF_POINT,  D3DTEXF_POINT,  D3DTEXF_POINT,  TRUE,  D3D_OK },

        { D3DTEXF_NONE,   D3DTEXF_NONE,   D3DTEXF_NONE,   TRUE,  D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_LINEAR, D3DTEXF_NONE,   D3DTEXF_NONE,   TRUE,  D3DERR_UNSUPPORTEDTEXTUREFILTER },
        { D3DTEXF_LINEAR, D3DTEXF_POINT,  D3DTEXF_NONE,   TRUE,  E_FAIL },
        { D3DTEXF_POINT,  D3DTEXF_LINEAR, D3DTEXF_NONE,   TRUE,  E_FAIL },
        { D3DTEXF_POINT,  D3DTEXF_POINT,  D3DTEXF_LINEAR, TRUE,  E_FAIL },

    };

    hr = IDirect3DDevice9_GetDirect3D(device, &d3d9);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetDirect3D(levels = 1) returned %08x\n", hr);
    hr = IDirect3D9_CheckDeviceFormat(d3d9, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0,
                                      D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F);
    if(FAILED(hr)) {
        skip("D3DFMT_A32B32G32R32F not supported\n");
        goto out;
    }
    hr = IDirect3D9_CheckDeviceFormat(d3d9, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER,
                                     D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F);
    if(SUCCEEDED(hr)) {
        skip("D3DFMT_A32B32G32R32F supports filtering\n");
        goto out;
    }

    hr = IDirect3DDevice9_CreateTexture(device, 128, 128, 0, 0, D3DFMT_A32B32G32R32F,
                                        D3DPOOL_MANAGED, &texture, 0);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture returned %08x\n", hr);

    /* Needed for ValidateDevice */
    hr = IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ | D3DFVF_TEX1);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetFVF returned %08x\n", hr);

    for(i = 0; i < (sizeof(tests) / sizeof(tests[0])); i++) {
        if(tests[i].has_texture) {
            hr = IDirect3DDevice9_SetTexture(device, 0, (IDirect3DBaseTexture9 *) texture);
            ok(hr == D3D_OK, "IDirect3DDevice9_SetTexture returned %08x\n", hr);
        } else {
            hr = IDirect3DDevice9_SetTexture(device, 0, NULL);
            ok(hr == D3D_OK, "IDirect3DDevice9_SetTexture returned %08x\n", hr);
        }

        hr = IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MAGFILTER, tests[i].magfilter);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetSamplerState returned %08x\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MINFILTER, tests[i].minfilter);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetSamplerState returned %08x\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, 0, D3DSAMP_MIPFILTER, tests[i].mipfilter);
        ok(hr == D3D_OK, "IDirect3DDevice9_SetSamplerState returned %08x\n", hr);

        passes = 0xdeadbeef;
        hr = IDirect3DDevice9_ValidateDevice(device, &passes);
        ok(hr == tests[i].result, "ValidateDevice failed: Texture %s, min %u, mag %u, mip %u. Got %08x, expected %08x\n",
                                   tests[i].has_texture ? "TRUE" : "FALSE", tests[i].magfilter, tests[i].minfilter,
                                   tests[i].mipfilter, hr, tests[i].result);
        if(SUCCEEDED(hr)) {
            ok(passes != 0, "ValidateDevice succeeded, passes is %u\n", passes);
        } else {
            ok(passes == 0xdeadbeef, "ValidateDevice failed, passes is %u\n", passes);
        }
    }

    hr = IDirect3DDevice9_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_SetTexture returned %#x.\n", hr);
    IDirect3DTexture9_Release(texture);

    out:
    IDirect3D9_Release(d3d9);
}

static void test_gettexture(IDirect3DDevice9 *device) {
    HRESULT hr;
    IDirect3DBaseTexture9 *texture = (IDirect3DBaseTexture9 *) 0xdeadbeef;

    hr = IDirect3DDevice9_SetTexture(device, 0, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_SetTexture failed, hr = 0x%08x\n", hr);
    hr = IDirect3DDevice9_GetTexture(device, 0, &texture);
    ok(hr == D3D_OK, "IDirect3DDevice9_GetTexture failed, hr = 0x%08x\n", hr);
    ok(texture == NULL, "Texture returned is %p, expected NULL\n", texture);
}

static void test_lod(IDirect3DDevice9 *device)
{
    HRESULT hr;
    DWORD ret;
    IDirect3DTexture9 *texture;

    hr = IDirect3DDevice9_CreateTexture(device, 128, 128, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                        &texture, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice9_CreateTexture failed with %08x\n", hr);

    /* SetLOD is only supported on D3DPOOL_MANAGED textures, but it doesn't return a HRESULT,
     * so it can't return a normal error. Instead, the call is simply ignored
     */
    ret = IDirect3DTexture9_SetLOD(texture, 0);
    ok(ret == 0, "IDirect3DTexture9_SetLOD returned %u, expected 0\n", ret);
    ret = IDirect3DTexture9_SetLOD(texture, 1);
    ok(ret == 0, "IDirect3DTexture9_SetLOD returned %u, expected 0\n", ret);
    ret = IDirect3DTexture9_SetLOD(texture, 2);
    ok(ret == 0, "IDirect3DTexture9_SetLOD returned %u, expected 0\n", ret);
    ret = IDirect3DTexture9_GetLOD(texture);
    ok(ret == 0, "IDirect3DTexture9_GetLOD returned %u, expected 0\n", ret);

    IDirect3DTexture9_Release(texture);
}

START_TEST(texture)
{
    D3DCAPS9 caps;
    HMODULE d3d9_handle;
    IDirect3DDevice9 *device_ptr;
    ULONG refcount;

    d3d9_handle = LoadLibraryA("d3d9.dll");
    if (!d3d9_handle)
    {
        skip("Could not load d3d9.dll\n");
        return;
    }

    device_ptr = init_d3d9(d3d9_handle);
    if (!device_ptr) return;

    IDirect3DDevice9_GetDeviceCaps(device_ptr, &caps);

    test_texture_stage_states(device_ptr, caps.MaxTextureBlendStages);
    test_cube_textures(device_ptr, caps.TextureCaps);
    test_mipmap_gen(device_ptr);
    test_filter(device_ptr);
    test_gettexture(device_ptr);
    test_lod(device_ptr);

    refcount = IDirect3DDevice9_Release(device_ptr);
    ok(!refcount, "Device has %u references left\n", refcount);
}
