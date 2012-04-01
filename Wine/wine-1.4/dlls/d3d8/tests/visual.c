/*
 * Copyright (C) 2005 Henri Verbeet
 * Copyright (C) 2007 Stefan Dösinger(for CodeWeavers)
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

/* See comment in dlls/d3d9/tests/visual.c for general guidelines */

#define COBJMACROS
#include <d3d8.h>
#include "wine/test.h"

static HMODULE d3d8_handle = 0;

struct vec3
{
    float x, y, z;
};

struct vec4
{
    float x, y, z, w;
};

static HWND create_window(void)
{
    WNDCLASS wc = {0};
    HWND ret;
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = "d3d8_test_wc";
    RegisterClass(&wc);

    ret = CreateWindow("d3d8_test_wc", "d3d8_test",
                       WS_POPUP | WS_SYSMENU , 20, 20, 640, 480, 0, 0, 0, 0);
    ShowWindow(ret, SW_SHOW);
    return ret;
}

static BOOL color_match(D3DCOLOR c1, D3DCOLOR c2, BYTE max_diff)
{
    if (abs((c1 & 0xff) - (c2 & 0xff)) > max_diff) return FALSE;
    c1 >>= 8; c2 >>= 8;
    if (abs((c1 & 0xff) - (c2 & 0xff)) > max_diff) return FALSE;
    c1 >>= 8; c2 >>= 8;
    if (abs((c1 & 0xff) - (c2 & 0xff)) > max_diff) return FALSE;
    c1 >>= 8; c2 >>= 8;
    if (abs((c1 & 0xff) - (c2 & 0xff)) > max_diff) return FALSE;
    return TRUE;
}

static DWORD getPixelColor(IDirect3DDevice8 *device, UINT x, UINT y)
{
    DWORD ret;
    IDirect3DTexture8 *tex = NULL;
    IDirect3DSurface8 *surf = NULL, *backbuf = NULL;
    HRESULT hr;
    D3DLOCKED_RECT lockedRect;
    RECT rectToLock = {x, y, x+1, y+1};

    hr = IDirect3DDevice8_CreateTexture(device, 640, 480, 1 /* Levels */, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tex);
    if(FAILED(hr) || !tex )  /* This is not a test */
    {
        trace("Can't create an offscreen plain surface to read the render target data, hr=%#08x\n", hr);
        return 0xdeadbeef;
    }
    hr = IDirect3DTexture8_GetSurfaceLevel(tex, 0, &surf);
    if(FAILED(hr) || !tex )  /* This is not a test */
    {
        trace("Can't get surface from texture, hr=%#08x\n", hr);
        ret = 0xdeadbeee;
        goto out;
    }

    hr = IDirect3DDevice8_GetRenderTarget(device, &backbuf);
    if(FAILED(hr))
    {
        trace("Can't get the render target, hr=%#08x\n", hr);
        ret = 0xdeadbeed;
        goto out;
    }
    hr = IDirect3DDevice8_CopyRects(device, backbuf, NULL, 0, surf, NULL);
    if(FAILED(hr))
    {
        trace("Can't read the render target, hr=%#08x\n", hr);
        ret = 0xdeadbeec;
        goto out;
    }

    hr = IDirect3DSurface8_LockRect(surf, &lockedRect, &rectToLock, D3DLOCK_READONLY);
    if(FAILED(hr))
    {
        trace("Can't lock the offscreen surface, hr=%#08x\n", hr);
        ret = 0xdeadbeeb;
        goto out;
    }
    /* Remove the X channel for now. DirectX and OpenGL have different ideas how to treat it apparently, and it isn't
     * really important for these tests
     */
    ret = ((DWORD *) lockedRect.pBits)[0] & 0x00ffffff;
    hr = IDirect3DSurface8_UnlockRect(surf);
    if(FAILED(hr))
    {
        trace("Can't unlock the offscreen surface, hr=%#08x\n", hr);
    }

out:
    if(backbuf) IDirect3DSurface8_Release(backbuf);
    if(surf) IDirect3DSurface8_Release(surf);
    if(tex) IDirect3DTexture8_Release(tex);
    return ret;
}

static IDirect3DDevice8 *init_d3d8(void)
{
    IDirect3D8 * (__stdcall * d3d8_create)(UINT SDKVersion) = 0;
    IDirect3D8 *d3d8_ptr = 0;
    IDirect3DDevice8 *device_ptr = 0;
    D3DPRESENT_PARAMETERS present_parameters;
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

    ZeroMemory(&present_parameters, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = create_window();
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferWidth = 640;
    present_parameters.BackBufferHeight = 480;
    present_parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    hr = IDirect3D8_CreateDevice(d3d8_ptr, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            present_parameters.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device_ptr);
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL || broken(hr == D3DERR_NOTAVAILABLE), "IDirect3D_CreateDevice returned: %#08x\n", hr);

    return device_ptr;
}

struct vertex
{
    float x, y, z;
    DWORD diffuse;
};

struct tvertex
{
    float x, y, z, w;
    DWORD diffuse;
};

struct nvertex
{
    float x, y, z;
    float nx, ny, nz;
    DWORD diffuse;
};

static void lighting_test(IDirect3DDevice8 *device)
{
    HRESULT hr;
    DWORD fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    DWORD nfvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_NORMAL;
    DWORD color;

    float mat[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, 1.0f };

    struct vertex unlitquad[] =
    {
        {-1.0f, -1.0f,   0.1f,                          0xffff0000},
        {-1.0f,  0.0f,   0.1f,                          0xffff0000},
        { 0.0f,  0.0f,   0.1f,                          0xffff0000},
        { 0.0f, -1.0f,   0.1f,                          0xffff0000},
    };
    struct vertex litquad[] =
    {
        {-1.0f,  0.0f,   0.1f,                          0xff00ff00},
        {-1.0f,  1.0f,   0.1f,                          0xff00ff00},
        { 0.0f,  1.0f,   0.1f,                          0xff00ff00},
        { 0.0f,  0.0f,   0.1f,                          0xff00ff00},
    };
    struct nvertex unlitnquad[] =
    {
        { 0.0f, -1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 0.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 1.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 1.0f, -1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
    };
    struct nvertex litnquad[] =
    {
        { 0.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xffffff00},
        { 0.0f,  1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xffffff00},
        { 1.0f,  1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xffffff00},
        { 1.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xffffff00},
    };
    WORD Indices[] = {0, 1, 2, 2, 3, 0};

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffffffff, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    /* Setup some states that may cause issues */
    hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLDMATRIX(0), (D3DMATRIX *) mat);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetTransform returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetTransform(device, D3DTS_VIEW, (D3DMATRIX *)mat);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetTransform returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetTransform(device, D3DTS_PROJECTION, (D3DMATRIX *) mat);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetTransform returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CLIPPING, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_STENCILENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHATESTENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed with %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed with %#08x\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, fvf);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed with %#08x\n", hr);
    if(hr == D3D_OK)
    {
        /* No lights are defined... That means, lit vertices should be entirely black */
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                    2 /*PrimCount */, Indices, D3DFMT_INDEX16, unlitquad, sizeof(unlitquad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, TRUE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                    2 /*PrimCount */, Indices, D3DFMT_INDEX16, litquad, sizeof(litquad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %#08x\n", hr);

        hr = IDirect3DDevice8_SetVertexShader(device, nfvf);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader failed with %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                    2 /*PrimCount */, Indices, D3DFMT_INDEX16, unlitnquad, sizeof(unlitnquad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, TRUE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                    2 /*PrimCount */, Indices, D3DFMT_INDEX16, litnquad, sizeof(litnquad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %#08x\n", hr);

        IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed with %#08x\n", hr);
    }

    color = getPixelColor(device, 160, 360); /* Lower left quad - unlit without normals */
    ok(color == 0x00ff0000, "Unlit quad without normals has color 0x%08x, expected 0x00ff0000.\n", color);
    color = getPixelColor(device, 160, 120); /* Upper left quad - lit without normals */
    ok(color == 0x00000000, "Lit quad without normals has color 0x%08x, expected 0x00000000.\n", color);
    color = getPixelColor(device, 480, 360); /* Lower right quad - unlit with normals */
    ok(color == 0x000000ff, "Unlit quad with normals has color 0x%08x, expected 0x000000ff.\n", color);
    color = getPixelColor(device, 480, 120); /* Upper right quad - lit with normals */
    ok(color == 0x00000000, "Lit quad with normals has color 0x%08x, expected 0x00000000.\n", color);

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %#08x\n", hr);
}

static void clear_test(IDirect3DDevice8 *device)
{
    /* Tests the correctness of clearing parameters */
    HRESULT hr;
    D3DRECT rect[2];
    D3DRECT rect_negneg;
    DWORD color;

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffffffff, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    /* Positive x, negative y */
    rect[0].x1 = 0;
    rect[0].y1 = 480;
    rect[0].x2 = 320;
    rect[0].y2 = 240;

    /* Positive x, positive y */
    rect[1].x1 = 0;
    rect[1].y1 = 0;
    rect[1].x2 = 320;
    rect[1].y2 = 240;
    /* Clear 2 rectangles with one call. Shows that a positive value is returned, but the negative rectangle
     * is ignored, the positive is still cleared afterwards
     */
    hr = IDirect3DDevice8_Clear(device, 2, rect, D3DCLEAR_TARGET, 0xffff0000, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    /* negative x, negative y */
    rect_negneg.x1 = 640;
    rect_negneg.y1 = 240;
    rect_negneg.x2 = 320;
    rect_negneg.y2 = 0;
    hr = IDirect3DDevice8_Clear(device, 1, &rect_negneg, D3DCLEAR_TARGET, 0xff00ff00, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    color = getPixelColor(device, 160, 360); /* lower left quad */
    ok(color == 0x00ffffff, "Clear rectangle 3(pos, neg) has color %08x\n", color);
    color = getPixelColor(device, 160, 120); /* upper left quad */
    ok(color == 0x00ff0000, "Clear rectangle 1(pos, pos) has color %08x\n", color);
    color = getPixelColor(device, 480, 360); /* lower right quad  */
    ok(color == 0x00ffffff, "Clear rectangle 4(NULL) has color %08x\n", color);
    color = getPixelColor(device, 480, 120); /* upper right quad */
    ok(color == 0x00ffffff, "Clear rectangle 4(neg, neg) has color %08x\n", color);

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
}

struct sVertex {
    float x, y, z;
    DWORD diffuse;
    DWORD specular;
};

struct sVertexT {
    float x, y, z, rhw;
    DWORD diffuse;
    DWORD specular;
};

static void fog_test(IDirect3DDevice8 *device)
{
    HRESULT hr;
    DWORD color;
    float start = 0.0, end = 1.0;

    /* Gets full z based fog with linear fog, no fog with specular color */
    struct sVertex untransformed_1[] = {
        {-1,    -1,   0.1f,         0xFFFF0000,     0xFF000000  },
        {-1,     0,   0.1f,         0xFFFF0000,     0xFF000000  },
        { 0,     0,   0.1f,         0xFFFF0000,     0xFF000000  },
        { 0,    -1,   0.1f,         0xFFFF0000,     0xFF000000  },
    };
    /* Ok, I am too lazy to deal with transform matrices */
    struct sVertex untransformed_2[] = {
        {-1,     0,   1.0f,         0xFFFF0000,     0xFF000000  },
        {-1,     1,   1.0f,         0xFFFF0000,     0xFF000000  },
        { 0,     1,   1.0f,         0xFFFF0000,     0xFF000000  },
        { 0,     0,   1.0f,         0xFFFF0000,     0xFF000000  },
    };
    /* Untransformed ones. Give them a different diffuse color to make the test look
     * nicer. It also makes making sure that they are drawn correctly easier.
     */
    struct sVertexT transformed_1[] = {
        {320,    0,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {640,    0,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {640,  240,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {320,  240,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
    };
    struct sVertexT transformed_2[] = {
        {320,  240,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {640,  240,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {640,  480,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
        {320,  480,   1.0f, 1.0f,   0xFFFFFF00,     0xFF000000  },
    };
    WORD Indices[] = {0, 1, 2, 2, 3, 0};

    D3DCAPS8 caps;
    float ident_mat[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    float world_mat1[16] =
    {
        1.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
        0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f, -0.5f, 1.0f
    };
    float world_mat2[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f
    };
    float proj_mat[16] =
    {
        1.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
        0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 1.0f
    };

    struct sVertex far_quad1[] =
    {
        {-1.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
        {-1.0f,  0.0f, 0.5f, 0xffff0000, 0xff000000},
        { 0.0f,  0.0f, 0.5f, 0xffff0000, 0xff000000},
        { 0.0f, -1.0f, 0.5f, 0xffff0000, 0xff000000},
    };
    struct sVertex far_quad2[] =
    {
        {-1.0f, 0.0f, 1.5f, 0xffff0000, 0xff000000},
        {-1.0f, 1.0f, 1.5f, 0xffff0000, 0xff000000},
        { 0.0f, 1.0f, 1.5f, 0xffff0000, 0xff000000},
        { 0.0f, 0.0f, 1.5f, 0xffff0000, 0xff000000},
    };

    memset(&caps, 0, sizeof(caps));
    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetDeviceCaps returned %08x\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff00ff, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear returned %#08x\n", hr);

    /* Setup initial states: No lighting, fog on, fog color */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(hr == D3D_OK, "Turning off lighting returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGENABLE, TRUE);
    ok(hr == D3D_OK, "Turning on fog calculations returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGCOLOR, 0xFF00FF00 /* A nice green */);
    ok(hr == D3D_OK, "Setting fog color returned %#08x\n", hr);

    /* First test: Both table fog and vertex fog off */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    ok(hr == D3D_OK, "Turning off table fog returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    ok(hr == D3D_OK, "Turning off vertex fog returned %#08x\n", hr);

    /* Start = 0, end = 1. Should be default, but set them */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGSTART, *((DWORD *) &start));
    ok(hr == D3D_OK, "Setting fog start returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGEND, *((DWORD *) &end));
    ok(hr == D3D_OK, "Setting fog start returned %#08x\n", hr);

    if(IDirect3DDevice8_BeginScene(device) == D3D_OK)
    {
        hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
        ok( hr == D3D_OK, "SetVertexShader returned %#08x\n", hr);
        /* Untransformed, vertex fog = NONE, table fog = NONE: Read the fog weighting from the specular color */
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                     2 /*PrimCount */, Indices, D3DFMT_INDEX16, untransformed_1,
                                                     sizeof(untransformed_1[0]));
        ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

        /* That makes it use the Z value */
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
        ok(hr == D3D_OK, "Setting fog vertex mode to D3DFOG_LINEAR returned %#08x\n", hr);
        /* Untransformed, vertex fog != none (or table fog != none):
         * Use the Z value as input into the equation
         */
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                     2 /*PrimCount */, Indices, D3DFMT_INDEX16, untransformed_2,
                                                     sizeof(untransformed_2[0]));
        ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

        /* transformed verts */
        hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
        ok( hr == D3D_OK, "SetVertexShader returned %#08x\n", hr);
        /* Transformed, vertex fog != NONE, pixel fog == NONE: Use specular color alpha component */
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                     2 /*PrimCount */, Indices, D3DFMT_INDEX16, transformed_1,
                                                     sizeof(transformed_1[0]));
        ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
        ok( hr == D3D_OK, "Setting fog table mode to D3DFOG_LINEAR returned %#08x\n", hr);
        /* Transformed, table fog != none, vertex anything: Use Z value as input to the fog
         * equation
         */
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 4 /* NumVerts */,
                                                     2 /*PrimCount */, Indices, D3DFMT_INDEX16, transformed_2,
                                                     sizeof(transformed_2[0]));
        ok(SUCCEEDED(hr), "IDirect3DDevice8_DrawIndexedPrimitiveUP returned %#x.\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "EndScene returned %#08x\n", hr);
    }
    else
    {
        ok(FALSE, "BeginScene failed\n");
    }

    color = getPixelColor(device, 160, 360);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0xFF, 0x00, 0x00), 1),
            "Untransformed vertex with no table or vertex fog has color %08x\n", color);
    color = getPixelColor(device, 160, 120);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xFF, 0x00), 1),
            "Untransformed vertex with linear vertex fog has color %08x\n", color);
    color = getPixelColor(device, 480, 120);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0xFF, 0xFF, 0x00), 1),
            "Transformed vertex with linear vertex fog has color %08x\n", color);
    color = getPixelColor(device, 480, 360);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xFF, 0x00), 1),
            "Transformed vertex with linear table fog has color %08x\n", color);

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

    if (caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE)
    {
        /* A simple fog + non-identity world matrix test */
        hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLDMATRIX(0), (D3DMATRIX *) world_mat1);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTransform returned %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
        ok(hr == D3D_OK, "Setting fog table mode to D3DFOG_LINEAR returned %#08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
        ok(hr == D3D_OK, "Turning off vertex fog returned %#08x\n", hr);

        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff00ff, 0.0, 0);
        ok(hr == D3D_OK, "IDirect3DDevice8_Clear returned %#08x\n", hr);

        if (IDirect3DDevice8_BeginScene(device) == D3D_OK)
        {
            hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
            ok(hr == D3D_OK, "SetVertexShader returned %#08x\n", hr);

            hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0, 4,
                    2, Indices, D3DFMT_INDEX16, far_quad1, sizeof(far_quad1[0]));
            ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

            hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0, 4,
                    2, Indices, D3DFMT_INDEX16, far_quad2, sizeof(far_quad2[0]));
            ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

            hr = IDirect3DDevice8_EndScene(device);
            ok(hr == D3D_OK, "EndScene returned %#08x\n", hr);
        }
        else
        {
            ok(FALSE, "BeginScene failed\n");
        }

        color = getPixelColor(device, 160, 360);
        ok(color_match(color, 0x00ff0000, 4), "Unfogged quad has color %08x\n", color);
        color = getPixelColor(device, 160, 120);
        ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xff, 0x00), 1),
                "Fogged out quad has color %08x\n", color);

        IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

        /* Test fog behavior with an orthogonal (but not identity) projection matrix */
        hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLDMATRIX(0), (D3DMATRIX *) world_mat2);
        ok(hr == D3D_OK, "SetTransform returned %#08x\n", hr);
        hr = IDirect3DDevice8_SetTransform(device, D3DTS_PROJECTION, (D3DMATRIX *) proj_mat);
        ok(hr == D3D_OK, "SetTransform returned %#08x\n", hr);

        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff00ff, 0.0, 0);
        ok(hr == D3D_OK, "Clear returned %#08x\n", hr);

        if (IDirect3DDevice8_BeginScene(device) == D3D_OK)
        {
            hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
            ok(hr == D3D_OK, "SetVertexShader returned %#08x\n", hr);

            hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0, 4,
                    2, Indices, D3DFMT_INDEX16, untransformed_1, sizeof(untransformed_1[0]));
            ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

            hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0, 4,
                    2, Indices, D3DFMT_INDEX16, untransformed_2, sizeof(untransformed_2[0]));
            ok(hr == D3D_OK, "DrawIndexedPrimitiveUP returned %#08x\n", hr);

            hr = IDirect3DDevice8_EndScene(device);
            ok(hr == D3D_OK, "EndScene returned %#08x\n", hr);
        }
        else
        {
            ok(FALSE, "BeginScene failed\n");
        }

        color = getPixelColor(device, 160, 360);
        todo_wine ok(color_match(color, 0x00e51900, 4), "Partially fogged quad has color %08x\n", color);
        color = getPixelColor(device, 160, 120);
        ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xff, 0x00), 1),
                "Fogged out quad has color %08x\n", color);

        IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

        hr = IDirect3DDevice8_SetTransform(device, D3DTS_WORLDMATRIX(0), (D3DMATRIX *) ident_mat);
        ok(hr == D3D_OK, "SetTransform returned %#08x\n", hr);
        hr = IDirect3DDevice8_SetTransform(device, D3DTS_PROJECTION, (D3DMATRIX *) ident_mat);
        ok(hr == D3D_OK, "SetTransform returned %#08x\n", hr);
    }
    else
    {
        skip("D3DPRASTERCAPS_FOGTABLE not supported, skipping some fog tests\n");
    }

    /* Turn off the fog master switch to avoid confusing other tests */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_FOGENABLE, FALSE);
    ok(hr == D3D_OK, "Turning off fog calculations returned %#08x\n", hr);
}

static void present_test(IDirect3DDevice8 *device)
{
    struct vertex quad[] =
    {
        {-1.0f, -1.0f,   0.9f,                          0xffff0000},
        {-1.0f,  1.0f,   0.9f,                          0xffff0000},
        { 1.0f, -1.0f,   0.1f,                          0xffff0000},
        { 1.0f,  1.0f,   0.1f,                          0xffff0000},
    };
    HRESULT hr;
    DWORD color;

    /* Does the Present clear the depth stencil? Clear the depth buffer with some value != 0,
    * then call Present. Then clear the color buffer to make sure it has some defined content
    * after the Present with D3DSWAPEFFECT_DISCARD. After that draw a plane that is somewhere cut
    * by the depth value.
    */
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 0.75, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear returned %08x\n", hr);
    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_Present returned %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffffffff, 0.4f, 0);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_Clear returned %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_GREATER);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %08x\n", hr);
    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetFVF returned %08x\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed with %08x\n", hr);
    if(hr == D3D_OK)
    {
        /* No lights are defined... That means, lit vertices should be entirely black */
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2 /*PrimCount */, quad, sizeof(quad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %08x\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed with %08x\n", hr);
    }

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %08x\n", hr);

    color = getPixelColor(device, 512, 240);
    ok(color == 0x00ffffff, "Present failed: Got color 0x%08x, expected 0x00ffffff.\n", color);
    color = getPixelColor(device, 64, 240);
    ok(color == 0x00ff0000, "Present failed: Got color 0x%08x, expected 0x00ff0000.\n", color);

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (%#08x)\n", hr);
}

static void test_rcp_rsq(IDirect3DDevice8 *device)
{
    HRESULT hr;
    DWORD shader;
    DWORD color;
    float constant[4] = {1.0, 1.0, 1.0, 2.0};

    static const float quad[][3] = {
        {-1.0f, -1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f},
    };

    const DWORD rcp_test[] = {
        0xfffe0101,                                         /* vs.1.1 */

        0x0009fffe, 0x30303030, 0x30303030,                 /* Shaders have to have a minimal size. */
        0x30303030, 0x30303030, 0x30303030,                 /* Add a filler comment. Usually D3DX8's*/
        0x30303030, 0x30303030, 0x30303030,                 /* version comment makes the shader big */
        0x00303030,                                         /* enough to make windows happy         */

        0x00000001, 0xc00f0000, 0x90e40000,                 /* mov oPos, v0 */
        0x00000006, 0xd00f0000, 0xa0e40000,                 /* rcp oD0, c0 */
        0x0000ffff                                          /* END */
    };

    const DWORD rsq_test[] = {
        0xfffe0101,                                         /* vs.1.1 */

        0x0009fffe, 0x30303030, 0x30303030,                 /* Shaders have to have a minimal size. */
        0x30303030, 0x30303030, 0x30303030,                 /* Add a filler comment. Usually D3DX8's*/
        0x30303030, 0x30303030, 0x30303030,                 /* version comment makes the shader big */
        0x00303030,                                         /* enough to make windows happy         */

        0x00000001, 0xc00f0000, 0x90e40000,                 /* mov oPos, v0 */
        0x00000007, 0xd00f0000, 0xa0e40000,                 /* rsq oD0, c0 */
        0x0000ffff                                          /* END */
    };

    DWORD decl[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),  /* D3DVSDE_POSITION, Register v0 */
        D3DVSD_END()
    };

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff336699, 0.0f, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    hr = IDirect3DDevice8_CreateVertexShader(device, decl, rcp_test, &shader, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned with %#08x\n", hr);

    IDirect3DDevice8_SetVertexShader(device, shader);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    IDirect3DDevice8_SetVertexShaderConstant(device, 0, constant, 1);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene returned %#08x\n", hr);
    if(SUCCEEDED(hr))
    {
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, &quad[0], 3 * sizeof(float));
        ok(SUCCEEDED(hr), "DrawPrimitiveUP failed (%#08x)\n", hr);
        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene returned %#08x\n", hr);
    }

    color = getPixelColor(device, 320, 240);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x80, 0x80, 0x80), 4),
            "RCP test returned color 0x%08x, expected 0x00808080.\n", color);

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (%#08x)\n", hr);

    IDirect3DDevice8_SetVertexShader(device, 0);
    IDirect3DDevice8_DeleteVertexShader(device, shader);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff996633, 0.0f, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed with %#08x\n", hr);

    hr = IDirect3DDevice8_CreateVertexShader(device, decl, rsq_test, &shader, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned with %#08x\n", hr);

    IDirect3DDevice8_SetVertexShader(device, shader);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    IDirect3DDevice8_SetVertexShaderConstant(device, 0, constant, 1);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene returned %#08x\n", hr);
    if(SUCCEEDED(hr))
    {
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, &quad[0], 3 * sizeof(float));
        ok(SUCCEEDED(hr), "DrawPrimitiveUP failed (%#08x)\n", hr);
        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene returned %#08x\n", hr);
    }

    color = getPixelColor(device, 320, 240);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0xb4, 0xb4, 0xb4), 4),
            "RSQ test returned color 0x%08x, expected 0x00b4b4b4.\n", color);

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (%#08x)\n", hr);

    IDirect3DDevice8_SetVertexShader(device, 0);
    IDirect3DDevice8_DeleteVertexShader(device, shader);
}

static void offscreen_test(IDirect3DDevice8 *device)
{
    HRESULT hr;
    IDirect3DTexture8 *offscreenTexture = NULL;
    IDirect3DSurface8 *backbuffer = NULL, *offscreen = NULL, *depthstencil = NULL;
    DWORD color;

    static const float quad[][5] = {
        {-0.5f, -0.5f, 0.1f, 0.0f, 0.0f},
        {-0.5f,  0.5f, 0.1f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.1f, 1.0f, 0.0f},
        { 0.5f,  0.5f, 0.1f, 1.0f, 1.0f},
    };

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &depthstencil);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetDepthStencilSurface failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff0000, 0.0, 0);
    ok(hr == D3D_OK, "Clear failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_CreateTexture(device, 128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &offscreenTexture);
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL, "Creating the offscreen render target failed, hr = %#08x\n", hr);
    if(!offscreenTexture) {
        trace("Failed to create an X8R8G8B8 offscreen texture, trying R5G6B5\n");
        hr = IDirect3DDevice8_CreateTexture(device, 128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &offscreenTexture);
        ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL, "Creating the offscreen render target failed, hr = %#08x\n", hr);
        if(!offscreenTexture) {
            skip("Cannot create an offscreen render target\n");
            goto out;
        }
    }

    hr = IDirect3DDevice8_GetBackBuffer(device, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(hr == D3D_OK, "Can't get back buffer, hr = %#08x\n", hr);
    if(!backbuffer) {
        goto out;
    }

    hr = IDirect3DTexture8_GetSurfaceLevel(offscreenTexture, 0, &offscreen);
    ok(hr == D3D_OK, "Can't get offscreen surface, hr = %#08x\n", hr);
    if(!offscreen) {
        goto out;
    }

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX1);
    ok(hr == D3D_OK, "SetVertexShader failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    ok(hr == D3D_OK, "SetTextureStageState failed, hr = %#08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    ok(hr == D3D_OK, "SetTextureStageState failed, hr = %#08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MINFILTER, D3DTEXF_NONE);
    ok(SUCCEEDED(hr), "SetTextureStageState D3DSAMP_MINFILTER failed (%#08x)\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MAGFILTER, D3DTEXF_NONE);
    ok(SUCCEEDED(hr), "SetTextureStageState D3DSAMP_MAGFILTER failed (%#08x)\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %08x\n", hr);

    if(IDirect3DDevice8_BeginScene(device) == D3D_OK) {
        hr = IDirect3DDevice8_SetRenderTarget(device, offscreen, depthstencil);
        ok(hr == D3D_OK, "SetRenderTarget failed, hr = %#08x\n", hr);
        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff00ff, 0.0, 0);
        ok(hr == D3D_OK, "Clear failed, hr = %#08x\n", hr);

        /* Draw without textures - Should result in a white quad */
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depthstencil);
        ok(hr == D3D_OK, "SetRenderTarget failed, hr = %#08x\n", hr);
        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *) offscreenTexture);
        ok(hr == D3D_OK, "SetTexture failed, %08x\n", hr);

        /* This time with the texture */
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        IDirect3DDevice8_EndScene(device);
    }

    /* Center quad - should be white */
    color = getPixelColor(device, 320, 240);
    ok(color == 0x00ffffff, "Offscreen failed: Got color 0x%08x, expected 0x00ffffff.\n", color);
    /* Some quad in the cleared part of the texture */
    color = getPixelColor(device, 170, 240);
    ok(color == 0x00ff00ff, "Offscreen failed: Got color 0x%08x, expected 0x00ff00ff.\n", color);
    /* Part of the originally cleared back buffer */
    color = getPixelColor(device, 10, 10);
    ok(color == 0x00ff0000, "Offscreen failed: Got color 0x%08x, expected 0x00ff0000.\n", color);
    if(0) {
        /* Lower left corner of the screen, where back buffer offscreen rendering draws the offscreen texture.
        * It should be red, but the offscreen texture may leave some junk there. Not tested yet. Depending on
        * the offscreen rendering mode this test would succeed or fail
        */
        color = getPixelColor(device, 10, 470);
        ok(color == 0x00ff0000, "Offscreen failed: Got color 0x%08x, expected 0x00ff0000.\n", color);
    }

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

out:
    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_SetTexture returned %#x.\n", hr);

    /* restore things */
    if(backbuffer) {
        hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depthstencil);
        ok(SUCCEEDED(hr), "IDirect3DDevice8_SetRenderTarget returned %#x.\n", hr);
        IDirect3DSurface8_Release(backbuffer);
    }
    if(offscreenTexture) {
        IDirect3DTexture8_Release(offscreenTexture);
    }
    if(offscreen) {
        IDirect3DSurface8_Release(offscreen);
    }
    if(depthstencil) {
        IDirect3DSurface8_Release(depthstencil);
    }
}

static void alpha_test(IDirect3DDevice8 *device)
{
    HRESULT hr;
    IDirect3DTexture8 *offscreenTexture;
    IDirect3DSurface8 *backbuffer = NULL, *offscreen = NULL, *depthstencil = NULL;
    DWORD color;

    struct vertex quad1[] =
    {
        {-1.0f, -1.0f,   0.1f,                          0x4000ff00},
        {-1.0f,  0.0f,   0.1f,                          0x4000ff00},
        { 1.0f, -1.0f,   0.1f,                          0x4000ff00},
        { 1.0f,  0.0f,   0.1f,                          0x4000ff00},
    };
    struct vertex quad2[] =
    {
        {-1.0f,  0.0f,   0.1f,                          0xc00000ff},
        {-1.0f,  1.0f,   0.1f,                          0xc00000ff},
        { 1.0f,  0.0f,   0.1f,                          0xc00000ff},
        { 1.0f,  1.0f,   0.1f,                          0xc00000ff},
    };
    static const float composite_quad[][5] = {
        { 0.0f, -1.0f, 0.1f, 0.0f, 1.0f},
        { 0.0f,  1.0f, 0.1f, 0.0f, 0.0f},
        { 1.0f, -1.0f, 0.1f, 1.0f, 1.0f},
        { 1.0f,  1.0f, 0.1f, 1.0f, 0.0f},
    };

    /* Clear the render target with alpha = 0.5 */
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0x80ff0000, 0.0, 0);
    ok(hr == D3D_OK, "Clear failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_CreateTexture(device, 128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &offscreenTexture);
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL, "Creating the offscreen render target failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &depthstencil);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetDepthStencilSurface failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_GetBackBuffer(device, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(hr == D3D_OK, "Can't get back buffer, hr = %#08x\n", hr);
    if(!backbuffer) {
        goto out;
    }
    hr = IDirect3DTexture8_GetSurfaceLevel(offscreenTexture, 0, &offscreen);
    ok(hr == D3D_OK, "Can't get offscreen surface, hr = %#08x\n", hr);
    if(!offscreen) {
        goto out;
    }

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(hr == D3D_OK, "SetVertexShader failed, hr = %#08x\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    ok(hr == D3D_OK, "SetTextureStageState failed, hr = %#08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    ok(hr == D3D_OK, "SetTextureStageState failed, hr = %#08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MINFILTER, D3DTEXF_NONE);
    ok(SUCCEEDED(hr), "SetTextureStageState D3DSAMP_MINFILTER failed (%#08x)\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MAGFILTER, D3DTEXF_NONE);
    ok(SUCCEEDED(hr), "SetTextureStageState D3DSAMP_MAGFILTER failed (%#08x)\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState returned %08x\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
    if(IDirect3DDevice8_BeginScene(device) == D3D_OK) {

        /* Draw two quads, one with src alpha blending, one with dest alpha blending. */
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad1, sizeof(quad1[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_DESTALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVDESTALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, sizeof(quad2[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        /* Switch to the offscreen buffer, and redo the testing. The offscreen render target
         * doesn't have an alpha channel. DESTALPHA and INVDESTALPHA "don't work" on render
         * targets without alpha channel, they give essentially ZERO and ONE blend factors. */
        hr = IDirect3DDevice8_SetRenderTarget(device, offscreen, 0);
        ok(hr == D3D_OK, "Can't get back buffer, hr = %08x\n", hr);
        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0x80ff0000, 0.0, 0);
        ok(hr == D3D_OK, "Clear failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad1, sizeof(quad1[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_DESTALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVDESTALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, sizeof(quad2[0]));
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depthstencil);
        ok(hr == D3D_OK, "Can't get back buffer, hr = %08x\n", hr);

        /* Render the offscreen texture onto the frame buffer to be able to compare it regularly.
         * Disable alpha blending for the final composition
         */
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX1);
        ok(hr == D3D_OK, "SetVertexShader failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *) offscreenTexture);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, composite_quad, sizeof(float) * 5);
        ok(hr == D3D_OK, "DrawPrimitiveUP failed, hr = %#08x\n", hr);
        hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice7_EndScene failed, hr = %08x\n", hr);
    }

    color = getPixelColor(device, 160, 360);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0xbf, 0x40, 0x00), 1),
       "SRCALPHA on frame buffer returned color %08x, expected 0x00bf4000\n", color);

    color = getPixelColor(device, 160, 120);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x7f, 0x00, 0x80), 2),
       "DSTALPHA on frame buffer returned color %08x, expected 0x007f0080\n", color);

    color = getPixelColor(device, 480, 360);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0xbf, 0x40, 0x00), 1),
       "SRCALPHA on texture returned color %08x, expected 0x00bf4000\n", color);

    color = getPixelColor(device, 480, 120);
    ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0x00, 0xff), 1),
       "DSTALPHA on texture returned color %08x, expected 0x000000ff\n", color);

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

    out:
    /* restore things */
    if(backbuffer) {
        IDirect3DSurface8_Release(backbuffer);
    }
    if(offscreenTexture) {
        IDirect3DTexture8_Release(offscreenTexture);
    }
    if(offscreen) {
        IDirect3DSurface8_Release(offscreen);
    }
    if(depthstencil) {
        IDirect3DSurface8_Release(depthstencil);
    }
}

static void p8_texture_test(IDirect3DDevice8 *device)
{
    IDirect3D8 *d3d = NULL;
    HRESULT hr;
    IDirect3DTexture8 *texture = NULL, *texture2 = NULL;
    D3DLOCKED_RECT lr;
    unsigned char *data;
    DWORD color, red, green, blue;
    PALETTEENTRY table[256];
    D3DCAPS8 caps;
    UINT i;
    float quad[] = {
       -1.0f,      0.0f,    0.1f,    0.0f,   0.0f,
       -1.0f,      1.0f,    0.1f,    0.0f,   1.0f,
        1.0f,      0.0f,    0.1f,    1.0f,   0.0f,
        1.0f,      1.0f,    0.1f,    1.0f,   1.0f,
    };
    float quad2[] = {
       -1.0f,      -1.0f,   0.1f,    0.0f,   0.0f,
       -1.0f,      0.0f,    0.1f,    0.0f,   1.0f,
        1.0f,      -1.0f,   0.1f,    1.0f,   0.0f,
        1.0f,      0.0f,    0.1f,    1.0f,   1.0f,
    };

    IDirect3DDevice8_GetDirect3D(device, &d3d);

    if(IDirect3D8_CheckDeviceFormat(d3d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0,
       D3DRTYPE_TEXTURE, D3DFMT_P8) != D3D_OK) {
           skip("D3DFMT_P8 textures not supported\n");
           goto out;
    }

    hr = IDirect3DDevice8_CreateTexture(device, 1, 1, 1, 0, D3DFMT_P8,
                                        D3DPOOL_MANAGED, &texture2);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateTexture failed, hr = %08x\n", hr);
    if(!texture2) {
        skip("Failed to create D3DFMT_P8 texture\n");
        goto out;
    }

    memset(&lr, 0, sizeof(lr));
    hr = IDirect3DTexture8_LockRect(texture2, 0, &lr, NULL, 0);
    ok(hr == D3D_OK, "IDirect3DTexture8_LockRect failed, hr = %08x\n", hr);
    data = lr.pBits;
    *data = 1;

    hr = IDirect3DTexture8_UnlockRect(texture2, 0);
    ok(hr == D3D_OK, "IDirect3DTexture8_UnlockRect failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_CreateTexture(device, 1, 1, 1, 0, D3DFMT_P8,
                                        D3DPOOL_MANAGED, &texture);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateTexture failed, hr = %08x\n", hr);
    if(!texture) {
        skip("Failed to create D3DFMT_P8 texture\n");
        goto out;
    }

    memset(&lr, 0, sizeof(lr));
    hr = IDirect3DTexture8_LockRect(texture, 0, &lr, NULL, 0);
    ok(hr == D3D_OK, "IDirect3DTexture8_LockRect failed, hr = %08x\n", hr);
    data = lr.pBits;
    *data = 1;

    hr = IDirect3DTexture8_UnlockRect(texture, 0);
    ok(hr == D3D_OK, "IDirect3DTexture8_UnlockRect failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);

    /* The first part of the test should work both with and without D3DPTEXTURECAPS_ALPHAPALETTE;
       alpha of every entry is set to 1.0, which MS says is required when there's no
       D3DPTEXTURECAPS_ALPHAPALETTE capability */
    for (i = 0; i < 256; i++) {
        table[i].peRed = table[i].peGreen = table[i].peBlue = 0;
        table[i].peFlags = 0xff;
    }
    table[1].peRed = 0xff;
    hr = IDirect3DDevice8_SetPaletteEntries(device, 0, table);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetPaletteEntries failed, hr = %08x\n", hr);

    table[1].peRed = 0;
    table[1].peBlue = 0xff;
    hr = IDirect3DDevice8_SetPaletteEntries(device, 1, table);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetPaletteEntries failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed, hr = %08x\n", hr);
    if(SUCCEEDED(hr)) {
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX1);
        ok(hr == D3D_OK, "SetVertexShader failed, hr = %#08x\n", hr);

        hr = IDirect3DDevice8_SetCurrentTexturePalette(device, 0);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetCurrentTexturePalette failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *) texture2);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, 5 * sizeof(float));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *) texture);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, 5 * sizeof(float));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetCurrentTexturePalette(device, 1);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetCurrentTexturePalette failed, hr = %08x\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, 5 * sizeof(float));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed, hr = %08x\n", hr);
    }

    color = getPixelColor(device, 32, 32);
    red   = (color & 0x00ff0000) >> 16;
    green = (color & 0x0000ff00) >>  8;
    blue  = (color & 0x000000ff) >>  0;
    ok(red == 0xff && blue == 0 && green == 0,
       "got color %08x, expected 0x00ff0000\n", color);

    color = getPixelColor(device, 32, 320);
    red   = (color & 0x00ff0000) >> 16;
    green = (color & 0x0000ff00) >>  8;
    blue  = (color & 0x000000ff) >>  0;
    ok(red == 0 && blue == 0xff && green == 0,
    "got color %08x, expected 0x000000ff\n", color);

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice8_Present failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 0.0, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed, hr = %08x\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed, hr = %08x\n", hr);
    if(SUCCEEDED(hr)) {
        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *) texture2);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, 5 * sizeof(float));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed, hr = %08x\n", hr);
    }


    color = getPixelColor(device, 32, 32);
    red   = (color & 0x00ff0000) >> 16;
    green = (color & 0x0000ff00) >>  8;
    blue  = (color & 0x000000ff) >>  0;
    ok(red == 0 && blue == 0xff && green == 0,
    "got color %08x, expected 0x000000ff\n", color);

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice8_Present failed, hr = %08x\n", hr);

    /* Test palettes with alpha */
    IDirect3DDevice8_GetDeviceCaps(device, &caps);
    if (!(caps.TextureCaps & D3DPTEXTURECAPS_ALPHAPALETTE)) {
        skip("no D3DPTEXTURECAPS_ALPHAPALETTE capability, tests with alpha in palette will be skipped\n");
    } else {
        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 0.0, 0);
        ok(hr == D3D_OK, "IDirect3DDevice8_Clear failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);

        for (i = 0; i < 256; i++) {
            table[i].peRed = table[i].peGreen = table[i].peBlue = 0;
            table[i].peFlags = 0xff;
        }
        table[1].peRed = 0xff;
        table[1].peFlags = 0x80;
        hr = IDirect3DDevice8_SetPaletteEntries(device, 0, table);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetPaletteEntries failed, hr = %08x\n", hr);

        table[1].peRed = 0;
        table[1].peBlue = 0xff;
        table[1].peFlags = 0x80;
        hr = IDirect3DDevice8_SetPaletteEntries(device, 1, table);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetPaletteEntries failed, hr = %08x\n", hr);

        hr = IDirect3DDevice8_BeginScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed, hr = %08x\n", hr);
        if(SUCCEEDED(hr)) {
            hr = IDirect3DDevice8_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);
            hr = IDirect3DDevice8_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);

            hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX1);
            ok(hr == D3D_OK, "SetVertexShader failed, hr = %#08x\n", hr);

            hr = IDirect3DDevice8_SetCurrentTexturePalette(device, 0);
            ok(hr == D3D_OK, "IDirect3DDevice8_SetCurrentTexturePalette failed, hr = %08x\n", hr);

            hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, 5 * sizeof(float));
            ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

            hr = IDirect3DDevice8_SetCurrentTexturePalette(device, 1);
            ok(hr == D3D_OK, "IDirect3DDevice8_SetCurrentTexturePalette failed, hr = %08x\n", hr);

            hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, 5 * sizeof(float));
            ok(hr == D3D_OK, "IDirect3DDevice8_DrawPrimitiveUP failed, hr = %08x\n", hr);

            hr = IDirect3DDevice8_EndScene(device);
            ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed, hr = %08x\n", hr);
        }

        color = getPixelColor(device, 32, 32);
        red   = (color & 0x00ff0000) >> 16;
        green = (color & 0x0000ff00) >>  8;
        blue  = (color & 0x000000ff) >>  0;
        ok(red >= 0x7e && red <= 0x81 && blue == 0 && green == 0,
        "got color %08x, expected 0x00800000 or near\n", color);

        color = getPixelColor(device, 32, 320);
        red   = (color & 0x00ff0000) >> 16;
        green = (color & 0x0000ff00) >>  8;
        blue  = (color & 0x000000ff) >>  0;
        ok(red == 0 && blue >= 0x7e && blue <= 0x81 && green == 0,
        "got color %08x, expected 0x00000080 or near\n", color);

        hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
        ok(hr == D3D_OK, "IDirect3DDevice8_Present failed, hr = %08x\n", hr);
    }

    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture failed, hr = %08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState failed, hr = %08x\n", hr);

out:
    if(texture) IDirect3DTexture8_Release(texture);
    if(texture2) IDirect3DTexture8_Release(texture2);
    IDirect3D8_Release(d3d);
}

static void texop_test(IDirect3DDevice8 *device)
{
    IDirect3DTexture8 *texture = NULL;
    D3DLOCKED_RECT locked_rect;
    D3DCOLOR color;
    D3DCAPS8 caps;
    HRESULT hr;
    unsigned int i;

    static const struct {
        float x, y, z;
        D3DCOLOR diffuse;
        float s, t;
    } quad[] = {
        {-1.0f, -1.0f, 0.1f, D3DCOLOR_ARGB(0x55, 0xff, 0x00, 0x00), -1.0f, -1.0f},
        {-1.0f,  1.0f, 0.1f, D3DCOLOR_ARGB(0x55, 0xff, 0x00, 0x00), -1.0f,  1.0f},
        { 1.0f, -1.0f, 0.1f, D3DCOLOR_ARGB(0x55, 0xff, 0x00, 0x00),  1.0f, -1.0f},
        { 1.0f,  1.0f, 0.1f, D3DCOLOR_ARGB(0x55, 0xff, 0x00, 0x00),  1.0f,  1.0f}
    };

    static const struct {
        D3DTEXTUREOP op;
        const char *name;
        DWORD caps_flag;
        D3DCOLOR result;
    } test_data[] = {
        {D3DTOP_SELECTARG1,                "SELECTARG1",                D3DTEXOPCAPS_SELECTARG1,                D3DCOLOR_ARGB(0x00, 0x00, 0xff, 0x00)},
        {D3DTOP_SELECTARG2,                "SELECTARG2",                D3DTEXOPCAPS_SELECTARG2,                D3DCOLOR_ARGB(0x00, 0x33, 0x33, 0x33)},
        {D3DTOP_MODULATE,                  "MODULATE",                  D3DTEXOPCAPS_MODULATE,                  D3DCOLOR_ARGB(0x00, 0x00, 0x33, 0x00)},
        {D3DTOP_MODULATE2X,                "MODULATE2X",                D3DTEXOPCAPS_MODULATE2X,                D3DCOLOR_ARGB(0x00, 0x00, 0x66, 0x00)},
        {D3DTOP_MODULATE4X,                "MODULATE4X",                D3DTEXOPCAPS_MODULATE4X,                D3DCOLOR_ARGB(0x00, 0x00, 0xcc, 0x00)},
        {D3DTOP_ADD,                       "ADD",                       D3DTEXOPCAPS_ADD,                       D3DCOLOR_ARGB(0x00, 0x33, 0xff, 0x33)},

        {D3DTOP_ADDSIGNED,                 "ADDSIGNED",                 D3DTEXOPCAPS_ADDSIGNED,                 D3DCOLOR_ARGB(0x00, 0x00, 0xb2, 0x00)},
        {D3DTOP_ADDSIGNED2X,               "ADDSIGNED2X",               D3DTEXOPCAPS_ADDSIGNED2X,               D3DCOLOR_ARGB(0x00, 0x00, 0xff, 0x00)},

        {D3DTOP_SUBTRACT,                  "SUBTRACT",                  D3DTEXOPCAPS_SUBTRACT,                  D3DCOLOR_ARGB(0x00, 0x00, 0xcc, 0x00)},
        {D3DTOP_ADDSMOOTH,                 "ADDSMOOTH",                 D3DTEXOPCAPS_ADDSMOOTH,                 D3DCOLOR_ARGB(0x00, 0x33, 0xff, 0x33)},
        {D3DTOP_BLENDDIFFUSEALPHA,         "BLENDDIFFUSEALPHA",         D3DTEXOPCAPS_BLENDDIFFUSEALPHA,         D3DCOLOR_ARGB(0x00, 0x22, 0x77, 0x22)},
        {D3DTOP_BLENDTEXTUREALPHA,         "BLENDTEXTUREALPHA",         D3DTEXOPCAPS_BLENDTEXTUREALPHA,         D3DCOLOR_ARGB(0x00, 0x14, 0xad, 0x14)},
        {D3DTOP_BLENDFACTORALPHA,          "BLENDFACTORALPHA",          D3DTEXOPCAPS_BLENDFACTORALPHA,          D3DCOLOR_ARGB(0x00, 0x07, 0xe4, 0x07)},
        {D3DTOP_BLENDTEXTUREALPHAPM,       "BLENDTEXTUREALPHAPM",       D3DTEXOPCAPS_BLENDTEXTUREALPHAPM,       D3DCOLOR_ARGB(0x00, 0x14, 0xff, 0x14)},
        {D3DTOP_BLENDCURRENTALPHA,         "BLENDCURRENTALPHA",         D3DTEXOPCAPS_BLENDCURRENTALPHA,         D3DCOLOR_ARGB(0x00, 0x22, 0x77, 0x22)},
        {D3DTOP_MODULATEALPHA_ADDCOLOR,    "MODULATEALPHA_ADDCOLOR",    D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR,    D3DCOLOR_ARGB(0x00, 0x1f, 0xff, 0x1f)},
        {D3DTOP_MODULATECOLOR_ADDALPHA,    "MODULATECOLOR_ADDALPHA",    D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA,    D3DCOLOR_ARGB(0x00, 0x99, 0xcc, 0x99)},
        {D3DTOP_MODULATEINVALPHA_ADDCOLOR, "MODULATEINVALPHA_ADDCOLOR", D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR, D3DCOLOR_ARGB(0x00, 0x14, 0xff, 0x14)},
        {D3DTOP_MODULATEINVCOLOR_ADDALPHA, "MODULATEINVCOLOR_ADDALPHA", D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA, D3DCOLOR_ARGB(0x00, 0xcc, 0x99, 0xcc)},
        /* BUMPENVMAP & BUMPENVMAPLUMINANCE have their own tests */
        {D3DTOP_DOTPRODUCT3,               "DOTPRODUCT2",               D3DTEXOPCAPS_DOTPRODUCT3,               D3DCOLOR_ARGB(0x00, 0x99, 0x99, 0x99)},
        {D3DTOP_MULTIPLYADD,               "MULTIPLYADD",               D3DTEXOPCAPS_MULTIPLYADD,               D3DCOLOR_ARGB(0x00, 0xff, 0x33, 0x00)},
        {D3DTOP_LERP,                      "LERP",                      D3DTEXOPCAPS_LERP,                      D3DCOLOR_ARGB(0x00, 0x00, 0x33, 0x33)},
    };

    memset(&caps, 0, sizeof(caps));
    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "GetDeviceCaps failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX0);
    ok(SUCCEEDED(hr), "SetVertexShader failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_CreateTexture(device, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_CreateTexture failed with 0x%08x\n", hr);
    hr = IDirect3DTexture8_LockRect(texture, 0, &locked_rect, NULL, 0);
    ok(SUCCEEDED(hr), "LockRect failed with 0x%08x\n", hr);
    *((DWORD *)locked_rect.pBits) = D3DCOLOR_ARGB(0x99, 0x00, 0xff, 0x00);
    hr = IDirect3DTexture8_UnlockRect(texture, 0);
    ok(SUCCEEDED(hr), "LockRect failed with 0x%08x\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLORARG0, D3DTA_DIFFUSE);
    ok(SUCCEEDED(hr), "SetTextureStageState failed with 0x%08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    ok(SUCCEEDED(hr), "SetTextureStageState failed with 0x%08x\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    ok(SUCCEEDED(hr), "SetTextureStageState failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    ok(SUCCEEDED(hr), "SetTextureStageState failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed with 0x%08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_TEXTUREFACTOR, 0xdd333333);
    ok(SUCCEEDED(hr), "SetRenderState failed with 0x%08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    ok(SUCCEEDED(hr), "SetRenderState failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
    ok(SUCCEEDED(hr), "IDirect3DDevice9_Clear failed with 0x%08x\n", hr);

    for (i = 0; i < sizeof(test_data) / sizeof(*test_data); ++i)
    {
        if (!(caps.TextureOpCaps & test_data[i].caps_flag))
        {
            skip("tex operation %s not supported\n", test_data[i].name);
            continue;
        }

        hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_COLOROP, test_data[i].op);
        ok(SUCCEEDED(hr), "SetTextureStageState (%s) failed with 0x%08x\n", test_data[i].name, hr);

        hr = IDirect3DDevice8_BeginScene(device);
        ok(SUCCEEDED(hr), "BeginScene failed with 0x%08x\n", hr);

        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
        ok(SUCCEEDED(hr), "DrawPrimitiveUP failed with 0x%08x\n", hr);

        hr = IDirect3DDevice8_EndScene(device);
        ok(SUCCEEDED(hr), "EndScene failed with 0x%08x\n", hr);

        color = getPixelColor(device, 320, 240);
        ok(color_match(color, test_data[i].result, 3), "Operation %s returned color 0x%08x, expected 0x%08x\n",
                test_data[i].name, color, test_data[i].result);

        hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
        ok(SUCCEEDED(hr), "Present failed with 0x%08x\n", hr);

        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        ok(SUCCEEDED(hr), "IDirect3DDevice9_Clear failed with 0x%08x\n", hr);
    }

    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed with 0x%08x\n", hr);
    if (texture) IDirect3DTexture8_Release(texture);
}

/* This test tests depth clamping / clipping behaviour:
 *   - With software vertex processing, depth values are clamped to the
 *     minimum / maximum z value when D3DRS_CLIPPING is disabled, and clipped
 *     when D3DRS_CLIPPING is enabled. Pretransformed vertices behave the
 *     same as regular vertices here.
 *   - With hardware vertex processing, D3DRS_CLIPPING seems to be ignored.
 *     Normal vertices are always clipped. Pretransformed vertices are
 *     clipped when D3DPMISCCAPS_CLIPTLVERTS is set, clamped when it isn't.
 *   - The viewport's MinZ/MaxZ is irrelevant for this.
 */
static void depth_clamp_test(IDirect3DDevice8 *device)
{
    const struct tvertex quad1[] =
    {
        {  0.0f,   0.0f,  5.0f, 1.0f, 0xff002b7f},
        {640.0f,   0.0f,  5.0f, 1.0f, 0xff002b7f},
        {  0.0f, 480.0f,  5.0f, 1.0f, 0xff002b7f},
        {640.0f, 480.0f,  5.0f, 1.0f, 0xff002b7f},
    };
    const struct tvertex quad2[] =
    {
        {  0.0f, 300.0f, 10.0f, 1.0f, 0xfff9e814},
        {640.0f, 300.0f, 10.0f, 1.0f, 0xfff9e814},
        {  0.0f, 360.0f, 10.0f, 1.0f, 0xfff9e814},
        {640.0f, 360.0f, 10.0f, 1.0f, 0xfff9e814},
    };
    const struct tvertex quad3[] =
    {
        {112.0f, 108.0f,  5.0f, 1.0f, 0xffffffff},
        {208.0f, 108.0f,  5.0f, 1.0f, 0xffffffff},
        {112.0f, 204.0f,  5.0f, 1.0f, 0xffffffff},
        {208.0f, 204.0f,  5.0f, 1.0f, 0xffffffff},
    };
    const struct tvertex quad4[] =
    {
        { 42.0f,  41.0f, 10.0f, 1.0f, 0xffffffff},
        {112.0f,  41.0f, 10.0f, 1.0f, 0xffffffff},
        { 42.0f, 108.0f, 10.0f, 1.0f, 0xffffffff},
        {112.0f, 108.0f, 10.0f, 1.0f, 0xffffffff},
    };
    const struct vertex quad5[] =
    {
        { -0.5f,   0.5f, 10.0f,       0xff14f914},
        {  0.5f,   0.5f, 10.0f,       0xff14f914},
        { -0.5f,  -0.5f, 10.0f,       0xff14f914},
        {  0.5f,  -0.5f, 10.0f,       0xff14f914},
    };
    const struct vertex quad6[] =
    {
        { -1.0f,   0.5f, 10.0f,       0xfff91414},
        {  1.0f,   0.5f, 10.0f,       0xfff91414},
        { -1.0f,  0.25f, 10.0f,       0xfff91414},
        {  1.0f,  0.25f, 10.0f,       0xfff91414},
    };

    D3DVIEWPORT8 vp;
    D3DCOLOR color;
    D3DCAPS8 caps;
    HRESULT hr;

    vp.X = 0;
    vp.Y = 0;
    vp.Width = 640;
    vp.Height = 480;
    vp.MinZ = 0.0;
    vp.MaxZ = 7.5;

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "Failed to get device caps, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetViewport(device, &vp);
    ok(SUCCEEDED(hr), "SetViewport failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff00ff00, 1.0, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CLIPPING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    ok(SUCCEEDED(hr), "SetVertexSahder failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad1, sizeof(*quad1));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, sizeof(*quad2));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CLIPPING, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad3, sizeof(*quad3));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad4, sizeof(*quad4));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CLIPPING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad5, sizeof(*quad5));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_CLIPPING, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad6, sizeof(*quad6));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    if (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS)
    {
        color = getPixelColor(device, 75, 75);
        ok(color_match(color, 0x0000ff00, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 150, 150);
        ok(color_match(color, 0x0000ff00, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 240);
        ok(color_match(color, 0x0000ff00, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 330);
        ok(color_match(color, 0x0000ff00, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 330);
        ok(color_match(color, 0x0000ff00, 1), "color 0x%08x.\n", color);
    }
    else
    {
        color = getPixelColor(device, 75, 75);
        ok(color_match(color, 0x00ffffff, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 150, 150);
        ok(color_match(color, 0x00ffffff, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 240);
        ok(color_match(color, 0x00002b7f, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 330);
        ok(color_match(color, 0x00f9e814, 1), "color 0x%08x.\n", color);
        color = getPixelColor(device, 320, 330);
        ok(color_match(color, 0x00f9e814, 1), "color 0x%08x.\n", color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);

    vp.MinZ = 0.0;
    vp.MaxZ = 1.0;
    hr = IDirect3DDevice8_SetViewport(device, &vp);
    ok(SUCCEEDED(hr), "SetViewport failed, hr %#x.\n", hr);
}

static void depth_buffer_test(IDirect3DDevice8 *device)
{
    static const struct vertex quad1[] =
    {
        { -1.0,  1.0, 0.33f, 0xff00ff00},
        {  1.0,  1.0, 0.33f, 0xff00ff00},
        { -1.0, -1.0, 0.33f, 0xff00ff00},
        {  1.0, -1.0, 0.33f, 0xff00ff00},
    };
    static const struct vertex quad2[] =
    {
        { -1.0,  1.0, 0.50f, 0xffff00ff},
        {  1.0,  1.0, 0.50f, 0xffff00ff},
        { -1.0, -1.0, 0.50f, 0xffff00ff},
        {  1.0, -1.0, 0.50f, 0xffff00ff},
    };
    static const struct vertex quad3[] =
    {
        { -1.0,  1.0, 0.66f, 0xffff0000},
        {  1.0,  1.0, 0.66f, 0xffff0000},
        { -1.0, -1.0, 0.66f, 0xffff0000},
        {  1.0, -1.0, 0.66f, 0xffff0000},
    };
    static const DWORD expected_colors[4][4] =
    {
        {0x000000ff, 0x000000ff, 0x0000ff00, 0x00ff0000},
        {0x000000ff, 0x000000ff, 0x0000ff00, 0x00ff0000},
        {0x0000ff00, 0x0000ff00, 0x0000ff00, 0x00ff0000},
        {0x00ff0000, 0x00ff0000, 0x00ff0000, 0x00ff0000},
    };

    IDirect3DSurface8 *backbuffer, *rt1, *rt2, *rt3;
    IDirect3DSurface8 *depth_stencil;
    unsigned int i, j;
    D3DVIEWPORT8 vp;
    D3DCOLOR color;
    HRESULT hr;

    vp.X = 0;
    vp.Y = 0;
    vp.Width = 640;
    vp.Height = 480;
    vp.MinZ = 0.0;
    vp.MaxZ = 1.0;

    hr = IDirect3DDevice8_SetViewport(device, &vp);
    ok(SUCCEEDED(hr), "SetViewport failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &depth_stencil);
    ok(SUCCEEDED(hr), "GetDepthStencilSurface failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetRenderTarget(device, &backbuffer);
    ok(SUCCEEDED(hr), "GetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateRenderTarget(device, 320, 240, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt1);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateRenderTarget(device, 480, 360, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt2);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateRenderTarget(device, 640, 480, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt3);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt3, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff0000ff, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff0000ff, 1.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt1, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt2, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad2, sizeof(*quad2));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(depth_stencil);
    IDirect3DSurface8_Release(backbuffer);
    IDirect3DSurface8_Release(rt3);
    IDirect3DSurface8_Release(rt2);
    IDirect3DSurface8_Release(rt1);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad1, sizeof(*quad1));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad3, sizeof(*quad3));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            unsigned int x = 80 * ((2 * j) + 1);
            unsigned int y = 60 * ((2 * i) + 1);
            color = getPixelColor(device, x, y);
            ok(color_match(color, expected_colors[i][j], 0),
                    "Expected color 0x%08x at %u,%u, got 0x%08x.\n", expected_colors[i][j], x, y, color);
        }
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);
}

/* Test that partial depth copies work the way they're supposed to. The clear
 * on rt2 only needs a partial copy of the onscreen depth/stencil buffer, and
 * the following draw should only copy back the part that was modified. */
static void depth_buffer2_test(IDirect3DDevice8 *device)
{
    static const struct vertex quad[] =
    {
        { -1.0,  1.0, 0.66f, 0xffff0000},
        {  1.0,  1.0, 0.66f, 0xffff0000},
        { -1.0, -1.0, 0.66f, 0xffff0000},
        {  1.0, -1.0, 0.66f, 0xffff0000},
    };

    IDirect3DSurface8 *backbuffer, *rt1, *rt2;
    IDirect3DSurface8 *depth_stencil;
    unsigned int i, j;
    D3DVIEWPORT8 vp;
    D3DCOLOR color;
    HRESULT hr;

    vp.X = 0;
    vp.Y = 0;
    vp.Width = 640;
    vp.Height = 480;
    vp.MinZ = 0.0;
    vp.MaxZ = 1.0;

    hr = IDirect3DDevice8_SetViewport(device, &vp);
    ok(SUCCEEDED(hr), "SetViewport failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CreateRenderTarget(device, 640, 480, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt1);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateRenderTarget(device, 480, 360, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt2);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &depth_stencil);
    ok(SUCCEEDED(hr), "GetDepthStencilSurface failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetRenderTarget(device, &backbuffer);
    ok(SUCCEEDED(hr), "GetRenderTarget failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt1, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff0000ff, 1.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff00ff00, 0.5f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt2, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, backbuffer, depth_stencil);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(depth_stencil);
    IDirect3DSurface8_Release(backbuffer);
    IDirect3DSurface8_Release(rt2);
    IDirect3DSurface8_Release(rt1);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            unsigned int x = 80 * ((2 * j) + 1);
            unsigned int y = 60 * ((2 * i) + 1);
            color = getPixelColor(device, x, y);
            ok(color_match(color, D3DCOLOR_ARGB(0x00, 0x00, 0xff, 0x00), 0),
                    "Expected color 0x0000ff00 %u,%u, got 0x%08x.\n", x, y, color);
        }
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);
}

static void intz_test(IDirect3DDevice8 *device)
{
    static const DWORD ps_code[] =
    {
        0xffff0101,                                                             /* ps_1_1                       */
        0x00000051, 0xa00f0000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, /* def c0, 1.0, 0.0, 0.0, 0.0   */
        0x00000051, 0xa00f0001, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, /* def c1, 0.0, 1.0, 0.0, 0.0   */
        0x00000051, 0xa00f0002, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, /* def c2, 0.0, 0.0, 1.0, 0.0   */
        0x00000042, 0xb00f0000,                                                 /* tex t0                       */
        0x00000042, 0xb00f0001,                                                 /* tex t1                       */
        0x00000008, 0xb0070001, 0xa0e40000, 0xb0e40001,                         /* dp3 t1.xyz, c0, t1           */
        0x00000005, 0x80070000, 0xa0e40001, 0xb0e40001,                         /* mul r0.xyz, c1, t1           */
        0x00000004, 0x80070000, 0xa0e40000, 0xb0e40000, 0x80e40000,             /* mad r0.xyz, c0, t0, r0       */
        0x40000001, 0x80080000, 0xa0aa0002,                                     /* +mov r0.w, c2.z              */
        0x0000ffff,                                                             /* end                          */
    };
    struct
    {
        float x, y, z;
        float s0, t0, p0;
        float s1, t1, p1, q1;
    }
    quad[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f},
        {  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f},
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},
        {  1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f},
    },
    half_quad_1[] =
    {
        { -1.0f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f},
        {  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f},
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},
        {  1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f},
    },
    half_quad_2[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f},
        {  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f},
        { -1.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},
        {  1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f},
    };
    struct
    {
        UINT x, y;
        D3DCOLOR color;
    }
    expected_colors[] =
    {
        { 80, 100, D3DCOLOR_ARGB(0x00, 0x20, 0x40, 0x00)},
        {240, 100, D3DCOLOR_ARGB(0x00, 0x60, 0xbf, 0x00)},
        {400, 100, D3DCOLOR_ARGB(0x00, 0x9f, 0x40, 0x00)},
        {560, 100, D3DCOLOR_ARGB(0x00, 0xdf, 0xbf, 0x00)},
        { 80, 450, D3DCOLOR_ARGB(0x00, 0x20, 0x40, 0x00)},
        {240, 450, D3DCOLOR_ARGB(0x00, 0x60, 0xbf, 0x00)},
        {400, 450, D3DCOLOR_ARGB(0x00, 0x9f, 0x40, 0x00)},
        {560, 450, D3DCOLOR_ARGB(0x00, 0xdf, 0xbf, 0x00)},
    };

    IDirect3DSurface8 *original_ds, *original_rt, *rt;
    IDirect3DTexture8 *texture;
    IDirect3DSurface8 *ds;
    IDirect3D8 *d3d8;
    D3DCAPS8 caps;
    HRESULT hr;
    DWORD ps;
    UINT i;

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "GetDeviceCaps failed, hr %#x.\n", hr);
    if (caps.PixelShaderVersion < D3DPS_VERSION(1, 1))
    {
        skip("No pixel shader 1.1 support, skipping INTZ test.\n");
        return;
    }
    if (caps.TextureCaps & D3DPTEXTURECAPS_POW2)
    {
        skip("No unconditional NP2 texture support, skipping INTZ test.\n");
        return;
    }

    hr = IDirect3DDevice8_GetDirect3D(device, &d3d8);
    ok(SUCCEEDED(hr), "GetDirect3D failed, hr %#x.\n", hr);

    hr = IDirect3D8_CheckDeviceFormat(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
            D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, MAKEFOURCC('I','N','T','Z'));
    if (FAILED(hr))
    {
        skip("No INTZ support, skipping INTZ test.\n");
        return;
    }

    IDirect3D8_Release(d3d8);

    hr = IDirect3DDevice8_GetRenderTarget(device, &original_rt);
    ok(SUCCEEDED(hr), "GetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &original_ds);
    ok(SUCCEEDED(hr), "GetDepthStencilSurface failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CreateTexture(device, 640, 480, 1,
            D3DUSAGE_DEPTHSTENCIL, MAKEFOURCC('I','N','T','Z'), D3DPOOL_DEFAULT, &texture);
    ok(SUCCEEDED(hr), "CreateTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateRenderTarget(device, 640, 480, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreatePixelShader(device, ps_code, &ps);
    ok(SUCCEEDED(hr), "CreatePixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX2
            | D3DFVF_TEXCOORDSIZE3(0) | D3DFVF_TEXCOORDSIZE4(1));
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_ALWAYS);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_TEXTURETRANSFORMFLAGS,
            D3DTTFF_COUNT4 | D3DTTFF_PROJECTED);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    hr = IDirect3DTexture8_GetSurfaceLevel(texture, 0, &ds);
    ok(SUCCEEDED(hr), "GetSurfaceLevel failed, hr %#x.\n", hr);

    /* Render offscreen, using the INTZ texture as depth buffer */
    hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Setup the depth/stencil surface. */
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, NULL);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(ds);
    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, ps);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Read the depth values back. */
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(expected_colors) / sizeof(*expected_colors); ++i)
    {
        D3DCOLOR color = getPixelColor(device, expected_colors[i].x, expected_colors[i].y);
        ok(color_match(color, expected_colors[i].color, 1),
                "Expected color 0x%08x at (%u, %u), got 0x%08x.\n",
                expected_colors[i].color, expected_colors[i].x, expected_colors[i].y, color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    IDirect3DTexture8_Release(texture);

    /* Render onscreen while using the INTZ texture as depth buffer */
    hr = IDirect3DDevice8_CreateTexture(device, 640, 480, 1,
            D3DUSAGE_DEPTHSTENCIL, MAKEFOURCC('I','N','T','Z'), D3DPOOL_DEFAULT, &texture);
    ok(SUCCEEDED(hr), "CreateTexture failed, hr %#x.\n", hr);
    hr = IDirect3DTexture8_GetSurfaceLevel(texture, 0, &ds);
    ok(SUCCEEDED(hr), "GetSurfaceLevel failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, NULL);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(ds);
    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, ps);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Read the depth values back. */
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(expected_colors) / sizeof(*expected_colors); ++i)
    {
        D3DCOLOR color = getPixelColor(device, expected_colors[i].x, expected_colors[i].y);
        ok(color_match(color, expected_colors[i].color, 1),
                "Expected color 0x%08x at (%u, %u), got 0x%08x.\n",
                expected_colors[i].color, expected_colors[i].x, expected_colors[i].y, color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    IDirect3DTexture8_Release(texture);

    /* Render offscreen, then onscreen, and finally check the INTZ texture in both areas */
    hr = IDirect3DDevice8_CreateTexture(device, 640, 480, 1,
            D3DUSAGE_DEPTHSTENCIL, MAKEFOURCC('I','N','T','Z'), D3DPOOL_DEFAULT, &texture);
    ok(SUCCEEDED(hr), "CreateTexture failed, hr %#x.\n", hr);
    hr = IDirect3DTexture8_GetSurfaceLevel(texture, 0, &ds);
    ok(SUCCEEDED(hr), "GetSurfaceLevel failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
    ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, half_quad_1, sizeof(*half_quad_1));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, half_quad_2, sizeof(*half_quad_2));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, NULL);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(ds);
    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, ps);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Read the depth values back. */
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(expected_colors) / sizeof(*expected_colors); ++i)
    {
        D3DCOLOR color = getPixelColor(device, expected_colors[i].x, expected_colors[i].y);
        ok(color_match(color, expected_colors[i].color, 1),
                "Expected color 0x%08x at (%u, %u), got 0x%08x.\n",
                expected_colors[i].color, expected_colors[i].x, expected_colors[i].y, color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, original_ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(original_ds);
    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    IDirect3DTexture8_Release(texture);
    hr = IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DeletePixelShader(device, ps);
    ok(SUCCEEDED(hr), "DeletePixelShader failed, hr %#x.\n", hr);

    IDirect3DSurface8_Release(original_rt);
    IDirect3DSurface8_Release(rt);
}

static void shadow_test(IDirect3DDevice8 *device)
{
    static const DWORD ps_code[] =
    {
        0xffff0101,                                                             /* ps_1_1                       */
        0x00000051, 0xa00f0000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, /* def c0, 1.0, 0.0, 0.0, 0.0   */
        0x00000051, 0xa00f0001, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, /* def c1, 0.0, 1.0, 0.0, 0.0   */
        0x00000051, 0xa00f0002, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, /* def c2, 0.0, 0.0, 1.0, 0.0   */
        0x00000042, 0xb00f0000,                                                 /* tex t0                       */
        0x00000042, 0xb00f0001,                                                 /* tex t1                       */
        0x00000008, 0xb0070001, 0xa0e40000, 0xb0e40001,                         /* dp3 t1.xyz, c0, t1           */
        0x00000005, 0x80070000, 0xa0e40001, 0xb0e40001,                         /* mul r0.xyz, c1, t1           */
        0x00000004, 0x80070000, 0xa0e40000, 0xb0e40000, 0x80e40000,             /* mad r0.xyz, c0, t0, r0       */
        0x40000001, 0x80080000, 0xa0aa0002,                                     /* +mov r0.w, c2.z              */
        0x0000ffff,                                                             /* end                          */
    };
    struct
    {
        D3DFORMAT format;
        const char *name;
    }
    formats[] =
    {
        {D3DFMT_D16_LOCKABLE,   "D3DFMT_D16_LOCKABLE"},
        {D3DFMT_D32,            "D3DFMT_D32"},
        {D3DFMT_D15S1,          "D3DFMT_D15S1"},
        {D3DFMT_D24S8,          "D3DFMT_D24S8"},
        {D3DFMT_D24X8,          "D3DFMT_D24X8"},
        {D3DFMT_D24X4S4,        "D3DFMT_D24X4S4"},
        {D3DFMT_D16,            "D3DFMT_D16"},
    };
    struct
    {
        float x, y, z;
        float s0, t0, p0;
        float s1, t1, p1, q1;
    }
    quad[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {  1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    };
    struct
    {
        UINT x, y;
        D3DCOLOR color;
    }
    expected_colors[] =
    {
        {400,  60, D3DCOLOR_ARGB(0x00, 0x00, 0x00, 0x00)},
        {560, 180, D3DCOLOR_ARGB(0x00, 0xff, 0x00, 0x00)},
        {560, 300, D3DCOLOR_ARGB(0x00, 0xff, 0x00, 0x00)},
        {400, 420, D3DCOLOR_ARGB(0x00, 0xff, 0xff, 0x00)},
        {240, 420, D3DCOLOR_ARGB(0x00, 0xff, 0xff, 0x00)},
        { 80, 300, D3DCOLOR_ARGB(0x00, 0x00, 0x00, 0x00)},
        { 80, 180, D3DCOLOR_ARGB(0x00, 0x00, 0x00, 0x00)},
        {240,  60, D3DCOLOR_ARGB(0x00, 0x00, 0x00, 0x00)},
    };

    IDirect3DSurface8 *original_ds, *original_rt, *rt;
    IDirect3D8 *d3d8;
    D3DCAPS8 caps;
    HRESULT hr;
    DWORD ps;
    UINT i;

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "GetDeviceCaps failed, hr %#x.\n", hr);
    if (caps.PixelShaderVersion < D3DPS_VERSION(1, 1))
    {
        skip("No pixel shader 1.1 support, skipping shadow test.\n");
        return;
    }

    hr = IDirect3DDevice8_GetDirect3D(device, &d3d8);
    ok(SUCCEEDED(hr), "GetDirect3D failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetRenderTarget(device, &original_rt);
    ok(SUCCEEDED(hr), "GetRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &original_ds);
    ok(SUCCEEDED(hr), "GetDepthStencilSurface failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CreateRenderTarget(device, 1024, 1024, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, FALSE, &rt);
    ok(SUCCEEDED(hr), "CreateRenderTarget failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreatePixelShader(device, ps_code, &ps);
    ok(SUCCEEDED(hr), "CreatePixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX2
            | D3DFVF_TEXCOORDSIZE3(0) | D3DFVF_TEXCOORDSIZE4(1));
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_ALWAYS);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_TEXTURETRANSFORMFLAGS,
            D3DTTFF_COUNT4 | D3DTTFF_PROJECTED);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(formats) / sizeof(*formats); ++i)
    {
        D3DFORMAT format = formats[i].format;
        IDirect3DTexture8 *texture;
        IDirect3DSurface8 *ds;
        unsigned int j;

        hr = IDirect3D8_CheckDeviceFormat(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
                D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format);
        if (FAILED(hr)) continue;

        hr = IDirect3DDevice8_CreateTexture(device, 1024, 1024, 1,
                D3DUSAGE_DEPTHSTENCIL, format, D3DPOOL_DEFAULT, &texture);
        ok(SUCCEEDED(hr), "CreateTexture failed, hr %#x.\n", hr);

        hr = IDirect3DTexture8_GetSurfaceLevel(texture, 0, &ds);
        ok(SUCCEEDED(hr), "GetSurfaceLevel failed, hr %#x.\n", hr);

        hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
        ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);

        IDirect3DDevice8_SetPixelShader(device, 0);
        ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

        /* Setup the depth/stencil surface. */
        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
        ok(SUCCEEDED(hr), "Clear failed, hr %#x.\n", hr);

        hr = IDirect3DDevice8_BeginScene(device);
        ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
        ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_EndScene(device);
        ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

        hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, NULL);
        ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
        IDirect3DSurface8_Release(ds);

        hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
        ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
        ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);

        hr = IDirect3DDevice8_SetPixelShader(device, ps);
        ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

        /* Do the actual shadow mapping. */
        hr = IDirect3DDevice8_BeginScene(device);
        ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
        ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_EndScene(device);
        ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

        hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
        ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
        hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
        ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
        IDirect3DTexture8_Release(texture);

        for (j = 0; j < sizeof(expected_colors) / sizeof(*expected_colors); ++j)
        {
            D3DCOLOR color = getPixelColor(device, expected_colors[j].x, expected_colors[j].y);
            ok(color_match(color, expected_colors[j].color, 0),
                    "Expected color 0x%08x at (%u, %u) for format %s, got 0x%08x.\n",
                    expected_colors[j].color, expected_colors[j].x, expected_colors[j].y,
                    formats[i].name, color);
        }

        hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
        ok(SUCCEEDED(hr), "Present failed, hr %#x.\n", hr);
    }

    hr = IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DeletePixelShader(device, ps);
    ok(SUCCEEDED(hr), "DeletePixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, original_ds);
    ok(SUCCEEDED(hr), "SetRenderTarget failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(original_ds);

    IDirect3DSurface8_Release(original_rt);
    IDirect3DSurface8_Release(rt);

    IDirect3D8_Release(d3d8);
}

static void multisample_copy_rects_test(IDirect3DDevice8 *device)
{
    IDirect3DSurface8 *original_ds, *original_rt, *ds, *ds_plain, *rt, *readback;
    RECT src_rect = {64, 64, 128, 128};
    POINT dst_point = {96, 96};
    D3DLOCKED_RECT locked_rect;
    IDirect3D8 *d3d8;
    D3DCOLOR color;
    HRESULT hr;

    hr = IDirect3DDevice8_GetDirect3D(device, &d3d8);
    ok(SUCCEEDED(hr), "Failed to get d3d8 interface, hr %#x.\n", hr);
    hr = IDirect3D8_CheckDeviceMultiSampleType(d3d8, D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES);
    IDirect3D8_Release(d3d8);
    if (FAILED(hr))
    {
        skip("Multisampling not supported for D3DFMT_A8R8G8B8, skipping multisampled CopyRects test.\n");
        return;
    }

    hr = IDirect3DDevice8_CreateRenderTarget(device, 256, 256, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_2_SAMPLES, FALSE, &rt);
    ok(SUCCEEDED(hr), "Failed to create render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateDepthStencilSurface(device, 256, 256, D3DFMT_D24S8,
            D3DMULTISAMPLE_2_SAMPLES, &ds);
    ok(SUCCEEDED(hr), "Failed to create depth stencil surface, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateDepthStencilSurface(device, 256, 256, D3DFMT_D24S8,
            D3DMULTISAMPLE_NONE, &ds_plain);
    ok(SUCCEEDED(hr), "Failed to create depth stencil surface, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateImageSurface(device, 256, 256, D3DFMT_A8R8G8B8, &readback);
    ok(SUCCEEDED(hr), "Failed to create readback surface, hr %#x.\n", hr);

    hr = IDirect3DDevice8_GetRenderTarget(device, &original_rt);
    ok(SUCCEEDED(hr), "Failed to get render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &original_ds);
    ok(SUCCEEDED(hr), "Failed to get depth/stencil, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff00ff00, 1.0f, 0);
    ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CopyRects(device, rt, NULL, 0, readback, NULL);
    ok(SUCCEEDED(hr), "Failed to read render target back, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CopyRects(device, ds, NULL, 0, ds_plain, NULL);
    todo_wine ok(hr == D3DERR_INVALIDCALL, "Depth buffer copy, hr %#x, expected %#x.\n", hr, D3DERR_INVALIDCALL);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET, 0xffff0000, 0.0, 0);
    ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CopyRects(device, rt, &src_rect, 1, readback, &dst_point);
    ok(SUCCEEDED(hr), "Failed to read render target back, hr %#x.\n", hr);

    hr = IDirect3DSurface8_LockRect(readback, &locked_rect, NULL, D3DLOCK_READONLY);
    ok(SUCCEEDED(hr), "Failed to lock readback surface, hr %#x.\n", hr);

    color = *(DWORD *)((BYTE *)locked_rect.pBits + 31 * locked_rect.Pitch + 31 * 4);
    ok(color == 0xff00ff00, "Got unexpected color 0x%08x.\n", color);

    color = *(DWORD *)((BYTE *)locked_rect.pBits + 127 * locked_rect.Pitch + 127 * 4);
    ok(color == 0xffff0000, "Got unexpected color 0x%08x.\n", color);

    hr = IDirect3DSurface8_UnlockRect(readback);
    ok(SUCCEEDED(hr), "Failed to unlock readback surface, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, original_ds);
    ok(SUCCEEDED(hr), "Failed to restore original render target, hr %#x.\n", hr);

    IDirect3DSurface8_Release(original_ds);
    IDirect3DSurface8_Release(original_rt);
    IDirect3DSurface8_Release(readback);
    IDirect3DSurface8_Release(ds_plain);
    IDirect3DSurface8_Release(ds);
    IDirect3DSurface8_Release(rt);
}

static void resz_test(IDirect3DDevice8 *device)
{
    IDirect3DSurface8 *rt, *original_rt, *ds, *original_ds, *intz_ds;
    IDirect3D8 *d3d8;
    D3DCAPS8 caps;
    HRESULT hr;
    unsigned int i;
    static const DWORD ps_code[] =
    {
        0xffff0101,                                                             /* ps_1_1                       */
        0x00000051, 0xa00f0000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, /* def c0, 1.0, 0.0, 0.0, 0.0   */
        0x00000051, 0xa00f0001, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, /* def c1, 0.0, 1.0, 0.0, 0.0   */
        0x00000051, 0xa00f0002, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, /* def c2, 0.0, 0.0, 1.0, 0.0   */
        0x00000042, 0xb00f0000,                                                 /* tex t0                       */
        0x00000042, 0xb00f0001,                                                 /* tex t1                       */
        0x00000008, 0xb0070001, 0xa0e40000, 0xb0e40001,                         /* dp3 t1.xyz, c0, t1           */
        0x00000005, 0x80070000, 0xa0e40001, 0xb0e40001,                         /* mul r0.xyz, c1, t1           */
        0x00000004, 0x80070000, 0xa0e40000, 0xb0e40000, 0x80e40000,             /* mad r0.xyz, c0, t0, r0       */
        0x40000001, 0x80080000, 0xa0aa0002,                                     /* +mov r0.w, c2.z              */
        0x0000ffff,                                                             /* end                          */
    };
    struct
    {
        float x, y, z;
        float s0, t0, p0;
        float s1, t1, p1, q1;
    }
    quad[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f},
        {  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f},
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},
        {  1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f},
    };
    struct
    {
        UINT x, y;
        D3DCOLOR color;
    }
    expected_colors[] =
    {
        { 80, 100, D3DCOLOR_ARGB(0x00, 0x20, 0x40, 0x00)},
        {240, 100, D3DCOLOR_ARGB(0x00, 0x60, 0xbf, 0x00)},
        {400, 100, D3DCOLOR_ARGB(0x00, 0x9f, 0x40, 0x00)},
        {560, 100, D3DCOLOR_ARGB(0x00, 0xdf, 0xbf, 0x00)},
        { 80, 450, D3DCOLOR_ARGB(0x00, 0x20, 0x40, 0x00)},
        {240, 450, D3DCOLOR_ARGB(0x00, 0x60, 0xbf, 0x00)},
        {400, 450, D3DCOLOR_ARGB(0x00, 0x9f, 0x40, 0x00)},
        {560, 450, D3DCOLOR_ARGB(0x00, 0xdf, 0xbf, 0x00)},
    };
    IDirect3DTexture8 *texture;
    DWORD ps, value;

    hr = IDirect3DDevice8_GetDirect3D(device, &d3d8);
    ok(SUCCEEDED(hr), "Failed to get d3d8 interface, hr %#x.\n", hr);
    hr = IDirect3D8_CheckDeviceMultiSampleType(d3d8, D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, TRUE, D3DMULTISAMPLE_2_SAMPLES);
    if (FAILED(hr))
    {
        skip("Multisampling not supported for D3DFMT_A8R8G8B8, skipping RESZ test.\n");
        return;
    }
    hr = IDirect3D8_CheckDeviceMultiSampleType(d3d8, D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL, D3DFMT_D24S8, TRUE, D3DMULTISAMPLE_2_SAMPLES);
    if (FAILED(hr))
    {
        skip("Multisampling not supported for D3DFMT_D24S8, skipping RESZ test.\n");
        return;
    }
    hr = IDirect3D8_CheckDeviceFormat(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
            D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, MAKEFOURCC('I','N','T','Z'));
    if (FAILED(hr))
    {
        skip("No INTZ support, skipping RESZ test.\n");
        return;
    }
    hr = IDirect3D8_CheckDeviceFormat(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
            D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, MAKEFOURCC('R','E','S','Z'));
    if (FAILED(hr))
    {
        skip("No RESZ support, skipping RESZ test.\n");
        return;
    }
    IDirect3D8_Release(d3d8);

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "GetDeviceCaps failed, hr %#x.\n", hr);
    if (caps.TextureCaps & D3DPTEXTURECAPS_POW2)
    {
        skip("No unconditional NP2 texture support, skipping INTZ test.\n");
        return;
    }

    hr = IDirect3DDevice8_GetRenderTarget(device, &original_rt);
    ok(SUCCEEDED(hr), "Failed to get render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &original_ds);
    ok(SUCCEEDED(hr), "Failed to get depth/stencil, hr %#x.\n", hr);

    hr = IDirect3DDevice8_CreateRenderTarget(device, 640, 480, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_2_SAMPLES, FALSE, &rt);
    ok(SUCCEEDED(hr), "Failed to create render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_CreateDepthStencilSurface(device, 640, 480, D3DFMT_D24S8,
            D3DMULTISAMPLE_2_SAMPLES, &ds);

    hr = IDirect3DDevice8_CreateTexture(device, 640, 480, 1,
            D3DUSAGE_DEPTHSTENCIL, MAKEFOURCC('I','N','T','Z'), D3DPOOL_DEFAULT, &texture);
    ok(SUCCEEDED(hr), "CreateTexture failed, hr %#x.\n", hr);
    hr = IDirect3DTexture8_GetSurfaceLevel(texture, 0, &intz_ds);
    ok(SUCCEEDED(hr), "GetSurfaceLevel failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, intz_ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff00ffff, 1.0f, 0);
    ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);
    IDirect3DSurface8_Release(intz_ds);
    hr = IDirect3DDevice8_CreatePixelShader(device, ps_code, &ps);
    ok(SUCCEEDED(hr), "CreatePixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_TEX2
            | D3DFVF_TEXCOORDSIZE3(0) | D3DFVF_TEXCOORDSIZE4(1));
    ok(SUCCEEDED(hr), "SetVertexShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZFUNC, D3DCMP_ALWAYS);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MINFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_MIPFILTER, D3DTEXF_POINT);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTextureStageState(device, 1, D3DTSS_TEXTURETRANSFORMFLAGS,
            D3DTTFF_COUNT4 | D3DTTFF_PROJECTED);
    ok(SUCCEEDED(hr), "SetTextureStageState failed, hr %#x.\n", hr);

    /* Render offscreen (multisampled), blit the depth buffer into the INTZ texture and then check its contents. */
    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff00ff00, 1.0f, 0);
    ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);

    /* The destination depth texture has to be bound to sampler 0 */
    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);

    /* the ATI "spec" says you have to do a dummy draw to ensure correct commands ordering */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, 0);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, 0xf);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);

    /* The actual multisampled depth buffer resolve happens here */
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_POINTSIZE, 0x7fa05000);
    ok(SUCCEEDED(hr), "SetRenderState (multisampled depth buffer resolve) failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_GetRenderState(device, D3DRS_POINTSIZE, &value);
    ok(SUCCEEDED(hr) && value == 0x7fa05000, "GetRenderState failed, hr %#x, value %#x.\n", hr, value);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, NULL);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, ps);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Read the depth values back. */
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(expected_colors) / sizeof(*expected_colors); ++i)
    {
        D3DCOLOR color = getPixelColor(device, expected_colors[i].x, expected_colors[i].y);
        ok(color_match(color, expected_colors[i].color, 1),
                "Expected color 0x%08x at (%u, %u), got 0x%08x.\n",
                expected_colors[i].color, expected_colors[i].x, expected_colors[i].y, color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);

    /* Test edge cases - try with no texture at all */
    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderTarget(device, rt, ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_POINTSIZE, 0x7fa05000);
    ok(SUCCEEDED(hr), "SetRenderState (multisampled depth buffer resolve) failed, hr %#x.\n", hr);

    /* With a non-multisampled depth buffer */
    IDirect3DSurface8_Release(ds);
    IDirect3DSurface8_Release(rt);
    hr = IDirect3DDevice8_CreateDepthStencilSurface(device, 640, 480, D3DFMT_D24S8,
            D3DMULTISAMPLE_NONE, &ds);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTexture(device, 0, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, 0);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_COLORWRITEENABLE, 0xf);
    ok(SUCCEEDED(hr), "SetRenderState failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_POINTSIZE, 0x7fa05000);
    ok(SUCCEEDED(hr), "SetRenderState (multisampled depth buffer resolve) failed, hr %#x.\n", hr);

    hr = IDirect3DDevice8_SetTexture(device, 1, (IDirect3DBaseTexture8 *)texture);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetPixelShader(device, ps);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);

    /* Read the depth values back. */
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "BeginScene failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
    ok(SUCCEEDED(hr), "DrawPrimitiveUP failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "EndScene failed, hr %#x.\n", hr);

    for (i = 0; i < sizeof(expected_colors) / sizeof(*expected_colors); ++i)
    {
        D3DCOLOR color = getPixelColor(device, expected_colors[i].x, expected_colors[i].y);
        ok(color_match(color, expected_colors[i].color, 1),
                "Expected color 0x%08x at (%u, %u), got 0x%08x.\n",
                expected_colors[i].color, expected_colors[i].x, expected_colors[i].y, color);
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Present failed (0x%08x)\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, original_rt, original_ds);
    ok(SUCCEEDED(hr), "Failed to set render target, hr %#x.\n", hr);
    IDirect3DSurface8_Release(ds);
    hr = IDirect3DDevice8_SetTexture(device, 0, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetTexture(device, 1, NULL);
    ok(SUCCEEDED(hr), "SetTexture failed, hr %#x.\n", hr);
    IDirect3DTexture8_Release(texture);
    hr = IDirect3DDevice8_SetPixelShader(device, 0);
    ok(SUCCEEDED(hr), "SetPixelShader failed, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DeletePixelShader(device, ps);
    ok(SUCCEEDED(hr), "DeletePixelShader failed, hr %#x.\n", hr);
    IDirect3DSurface8_Release(original_ds);
    IDirect3DSurface8_Release(original_rt);
}

static void zenable_test(IDirect3DDevice8 *device)
{
    static const struct
    {
        struct vec4 position;
        D3DCOLOR diffuse;
    }
    tquad[] =
    {
        {{  0.0f, 480.0f, -0.5f, 1.0f}, 0xff00ff00},
        {{  0.0f,   0.0f, -0.5f, 1.0f}, 0xff00ff00},
        {{640.0f, 480.0f,  1.5f, 1.0f}, 0xff00ff00},
        {{640.0f,   0.0f,  1.5f, 1.0f}, 0xff00ff00},
    };
    D3DCOLOR color;
    D3DCAPS8 caps;
    HRESULT hr;
    UINT x, y;
    UINT i, j;

    hr = IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, D3DZB_FALSE);
    ok(SUCCEEDED(hr), "Failed to disable z-buffering, hr %#x.\n", hr);
    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    ok(SUCCEEDED(hr), "Failed to set FVF, hr %#x.\n", hr);

    hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffff0000, 0.0f, 0);
    ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);
    hr = IDirect3DDevice8_BeginScene(device);
    ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
    hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, tquad, sizeof(*tquad));
    ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
    hr = IDirect3DDevice8_EndScene(device);
    ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);

    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            x = 80 * ((2 * j) + 1);
            y = 60 * ((2 * i) + 1);
            color = getPixelColor(device, x, y);
            ok(color_match(color, 0x0000ff00, 1),
                    "Expected color 0x0000ff00 at %u, %u, got 0x%08x.\n", x, y, color);
        }
    }

    hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
    ok(SUCCEEDED(hr), "Failed to present backbuffer, hr %#x.\n", hr);

    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(SUCCEEDED(hr), "Failed to get device caps, hr %#x.\n", hr);

    if (caps.PixelShaderVersion >= D3DPS_VERSION(1, 1)
            && caps.VertexShaderVersion >= D3DVS_VERSION(1, 1))
    {
        static const DWORD vs_code[] =
        {
            0xfffe0101,                                 /* vs_1_1           */
            0x00000001, 0xc00f0000, 0x90e40000,         /* mov oPos, v0     */
            0x00000001, 0xd00f0000, 0x90e40000,         /* mov oD0, v0      */
            0x0000ffff
        };
        static const DWORD ps_code[] =
        {
            0xffff0101,                                 /* ps_1_1           */
            0x00000001, 0x800f0000, 0x90e40000,         /* mov r0, v0       */
            0x0000ffff                                  /* end              */
        };
        static const struct vec3 quad[] =
        {
            {-1.0f, -1.0f, -0.5f},
            {-1.0f,  1.0f, -0.5f},
            { 1.0f, -1.0f,  1.5f},
            { 1.0f,  1.0f,  1.5f},
        };
        static const D3DCOLOR expected[] =
        {
            0x00ff0000, 0x0060df60, 0x009fdf9f, 0x00ff0000,
            0x00ff0000, 0x00609f60, 0x009f9f9f, 0x00ff0000,
            0x00ff0000, 0x00606060, 0x009f609f, 0x00ff0000,
            0x00ff0000, 0x00602060, 0x009f209f, 0x00ff0000,
        };
        static const DWORD decl[] =
        {
            D3DVSD_STREAM(0),
            D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),
            D3DVSD_END()
        };
        DWORD vs, ps;

        hr = IDirect3DDevice8_CreateVertexShader(device, decl, vs_code, &vs, 0);
        ok(SUCCEEDED(hr), "Failed to create vertex shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_CreatePixelShader(device, ps_code, &ps);
        ok(SUCCEEDED(hr), "Failed to create pixel shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_SetVertexShader(device, vs);
        ok(SUCCEEDED(hr), "Failed to set vertex shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_SetPixelShader(device, ps);
        ok(SUCCEEDED(hr), "Failed to set pixel shader, hr %#x.\n", hr);

        hr = IDirect3DDevice8_Clear(device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffff0000, 0.0f, 0);
        ok(SUCCEEDED(hr), "Failed to clear render target, hr %#x.\n", hr);
        hr = IDirect3DDevice8_BeginScene(device);
        ok(SUCCEEDED(hr), "Failed to begin scene, hr %#x.\n", hr);
        hr = IDirect3DDevice8_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(*quad));
        ok(SUCCEEDED(hr), "Failed to draw, hr %#x.\n", hr);
        hr = IDirect3DDevice8_EndScene(device);
        ok(SUCCEEDED(hr), "Failed to end scene, hr %#x.\n", hr);

        for (i = 0; i < 4; ++i)
        {
            for (j = 0; j < 4; ++j)
            {
                x = 80 * ((2 * j) + 1);
                y = 60 * ((2 * i) + 1);
                color = getPixelColor(device, x, y);
                ok(color_match(color, expected[i * 4 + j], 1),
                        "Expected color 0x%08x at %u, %u, got 0x%08x.\n", expected[i * 4 + j], x, y, color);
            }
        }

        hr = IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);
        ok(SUCCEEDED(hr), "Failed to present backbuffer, hr %#x.\n", hr);

        hr = IDirect3DDevice8_SetPixelShader(device, 0);
        ok(SUCCEEDED(hr), "Failed to set pixel shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_SetVertexShader(device, 0);
        ok(SUCCEEDED(hr), "Failed to set vertex shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_DeletePixelShader(device, ps);
        ok(SUCCEEDED(hr), "Failed to delete pixel shader, hr %#x.\n", hr);
        hr = IDirect3DDevice8_DeleteVertexShader(device, vs);
        ok(SUCCEEDED(hr), "Failed to delete vertex shader, hr %#x.\n", hr);
    }
}

START_TEST(visual)
{
    IDirect3DDevice8 *device_ptr;
    HRESULT hr;
    DWORD color;
    D3DCAPS8 caps;

    d3d8_handle = LoadLibraryA("d3d8.dll");
    if (!d3d8_handle)
    {
        win_skip("Could not load d3d8.dll\n");
        return;
    }

    device_ptr = init_d3d8();
    if (!device_ptr)
    {
        win_skip("Could not initialize direct3d\n");
        return;
    }

    IDirect3DDevice8_GetDeviceCaps(device_ptr, &caps);

    /* Check for the reliability of the returned data */
    hr = IDirect3DDevice8_Clear(device_ptr, 0, NULL, D3DCLEAR_TARGET, 0xffff0000, 0.0, 0);
    if(FAILED(hr))
    {
        skip("Clear failed, can't assure correctness of the test results\n");
        goto cleanup;
    }

    color = getPixelColor(device_ptr, 1, 1);
    if(color !=0x00ff0000)
    {
        skip("Sanity check returned an incorrect color(%08x), can't assure the correctness of the tests\n", color);
        goto cleanup;
    }
    IDirect3DDevice8_Present(device_ptr, NULL, NULL, NULL, NULL);

    hr = IDirect3DDevice8_Clear(device_ptr, 0, NULL, D3DCLEAR_TARGET, 0xff00ddee, 0.0, 0);
    if(FAILED(hr))
    {
        skip("Clear failed, can't assure correctness of the test results\n");
        goto cleanup;
    }

    color = getPixelColor(device_ptr, 639, 479);
    if(color != 0x0000ddee)
    {
        skip("Sanity check returned an incorrect color(%08x), can't assure the correctness of the tests\n", color);
        goto cleanup;
    }
    IDirect3DDevice8_Present(device_ptr, NULL, NULL, NULL, NULL);

    /* Now run the real test */
    depth_clamp_test(device_ptr);
    lighting_test(device_ptr);
    clear_test(device_ptr);
    fog_test(device_ptr);
    present_test(device_ptr);
    offscreen_test(device_ptr);
    alpha_test(device_ptr);

    if (caps.VertexShaderVersion >= D3DVS_VERSION(1, 1))
    {
        test_rcp_rsq(device_ptr);
    }
    else
    {
        skip("No vs.1.1 support\n");
    }

    p8_texture_test(device_ptr);
    texop_test(device_ptr);
    depth_buffer_test(device_ptr);
    depth_buffer2_test(device_ptr);
    intz_test(device_ptr);
    shadow_test(device_ptr);
    multisample_copy_rects_test(device_ptr);
    zenable_test(device_ptr);
    resz_test(device_ptr);

cleanup:
    if(device_ptr) {
        D3DDEVICE_CREATION_PARAMETERS creation_parameters;
        ULONG refcount;

        IDirect3DDevice8_GetCreationParameters(device_ptr, &creation_parameters);
        DestroyWindow(creation_parameters.hFocusWindow);
        refcount = IDirect3DDevice8_Release(device_ptr);
        ok(!refcount, "Device has %u references left\n", refcount);
    }
}
