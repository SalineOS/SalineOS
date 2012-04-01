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
#include <d3d8.h>
#include "wine/test.h"

static HWND create_window(void)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = "d3d8_test_wc";
    RegisterClass(&wc);

    return CreateWindow("d3d8_test_wc", "d3d8_test",
            0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static IDirect3DDevice8 *init_d3d8(HMODULE d3d8_handle)
{
    IDirect3D8 * (__stdcall * d3d8_create)(UINT SDKVersion) = 0;
    IDirect3D8 *d3d8_ptr = 0;
    IDirect3DDevice8 *device_ptr = 0;
    D3DPRESENT_PARAMETERS present_parameters;
    D3DDISPLAYMODE               d3ddm;
    HRESULT hr;

    d3d8_create = (void *)GetProcAddress(d3d8_handle, "Direct3DCreate8");
    ok(d3d8_create != NULL, "Failed to get address of Direct3DCreate8\n");
    if (!d3d8_create) return NULL;

    d3d8_ptr = d3d8_create(D3D_SDK_VERSION);
    if (!d3d8_ptr)
    {
        skip("could not create D3D8\n");
        return NULL;
    }

    IDirect3D8_GetAdapterDisplayMode(d3d8_ptr, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = create_window();
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D8_CreateDevice(d3d8_ptr, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device_ptr);

    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#x\n", hr);
        return NULL;
    }

    return device_ptr;
}

static void test_volume_get_container(IDirect3DDevice8 *device_ptr)
{
    IDirect3DVolumeTexture8 *texture_ptr = 0;
    IDirect3DVolume8 *volume_ptr = 0;
    void *container_ptr;
    HRESULT hr;

    hr = IDirect3DDevice8_CreateVolumeTexture(device_ptr, 128, 128, 128, 1, 0,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture_ptr);
    ok(SUCCEEDED(hr) && texture_ptr != NULL, "CreateVolumeTexture returned: hr %#x, texture_ptr %p. "
        "Expected hr %#x, texture_ptr != %p\n", hr, texture_ptr, D3D_OK, NULL);
    if (!texture_ptr || FAILED(hr)) goto cleanup;

    hr = IDirect3DVolumeTexture8_GetVolumeLevel(texture_ptr, 0, &volume_ptr);
    ok(SUCCEEDED(hr) && volume_ptr != NULL, "GetVolumeLevel returned: hr %#x, volume_ptr %p. "
        "Expected hr %#x, volume_ptr != %p\n", hr, volume_ptr, D3D_OK, NULL);
    if (!volume_ptr || FAILED(hr)) goto cleanup;

    /* These should work... */
    container_ptr = (void *)0x1337c0d3;
    hr = IDirect3DVolume8_GetContainer(volume_ptr, &IID_IUnknown, &container_ptr);
    ok(SUCCEEDED(hr) && container_ptr == texture_ptr, "GetContainer returned: hr %#x, container_ptr %p. "
        "Expected hr %#x, container_ptr %p\n", hr, container_ptr, S_OK, texture_ptr);
    if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr);

    container_ptr = (void *)0x1337c0d3;
    hr = IDirect3DVolume8_GetContainer(volume_ptr, &IID_IDirect3DResource8, &container_ptr);
    ok(SUCCEEDED(hr) && container_ptr == texture_ptr, "GetContainer returned: hr %#x, container_ptr %p. "
        "Expected hr %#x, container_ptr %p\n", hr, container_ptr, S_OK, texture_ptr);
    if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr);

    container_ptr = (void *)0x1337c0d3;
    hr = IDirect3DVolume8_GetContainer(volume_ptr, &IID_IDirect3DBaseTexture8, &container_ptr);
    ok(SUCCEEDED(hr) && container_ptr == texture_ptr, "GetContainer returned: hr %#x, container_ptr %p. "
        "Expected hr %#x, container_ptr %p\n", hr, container_ptr, S_OK, texture_ptr);
    if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr);

    container_ptr = (void *)0x1337c0d3;
    hr = IDirect3DVolume8_GetContainer(volume_ptr, &IID_IDirect3DVolumeTexture8, &container_ptr);
    ok(SUCCEEDED(hr) && container_ptr == texture_ptr, "GetContainer returned: hr %#x, container_ptr %p. "
        "Expected hr %#x, container_ptr %p\n", hr, container_ptr, S_OK, texture_ptr);
    if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr);

    /* ...and this one shouldn't. This should return E_NOINTERFACE and set container_ptr to NULL */
    container_ptr = (void *)0x1337c0d3;
    hr = IDirect3DVolume8_GetContainer(volume_ptr, &IID_IDirect3DVolume8, &container_ptr);
    ok(hr == E_NOINTERFACE && container_ptr == NULL, "GetContainer returned: hr %#x, container_ptr %p. "
        "Expected hr %#x, container_ptr %p\n", hr, container_ptr, E_NOINTERFACE, NULL);
    if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr);

cleanup:
    if (texture_ptr) IDirect3DVolumeTexture8_Release(texture_ptr);
    if (volume_ptr) IDirect3DVolume8_Release(volume_ptr);
}

START_TEST(volume)
{
    HMODULE d3d8_handle;
    IDirect3DDevice8 *device_ptr;
    ULONG refcount;
    D3DCAPS8 caps;

    d3d8_handle = LoadLibraryA("d3d8.dll");
    if (!d3d8_handle)
    {
        skip("Could not load d3d8.dll\n");
        return;
    }

    device_ptr = init_d3d8(d3d8_handle);
    if (!device_ptr) return;

    IDirect3DDevice8_GetDeviceCaps(device_ptr, &caps);
    if(!(caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP)) {
        skip("Volume textures not supported\n");
        return;
    }

    test_volume_get_container(device_ptr);

    refcount = IDirect3DDevice8_Release(device_ptr);
    ok(!refcount, "Device has %u references left\n", refcount);
}
