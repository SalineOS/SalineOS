/*
 * Implementation of IDirect3DRM Interface
 *
 * Copyright 2010 Christian Costa
 * Copyright 2011 André Hentschel
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

#include "wine/debug.h"

#define COBJMACROS

#include "winbase.h"
#include "wingdi.h"

#include "d3drm_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3drm);

typedef struct {
    IDirect3DRM IDirect3DRM_iface;
    IDirect3DRM2 IDirect3DRM2_iface;
    IDirect3DRM3 IDirect3DRM3_iface;
    LONG ref;
} IDirect3DRMImpl;

static inline IDirect3DRMImpl *impl_from_IDirect3DRM(IDirect3DRM *iface)
{
    return CONTAINING_RECORD(iface, IDirect3DRMImpl, IDirect3DRM_iface);
}

static inline IDirect3DRMImpl *impl_from_IDirect3DRM2(IDirect3DRM2 *iface)
{
    return CONTAINING_RECORD(iface, IDirect3DRMImpl, IDirect3DRM2_iface);
}

static inline IDirect3DRMImpl *impl_from_IDirect3DRM3(IDirect3DRM3 *iface)
{
    return CONTAINING_RECORD(iface, IDirect3DRMImpl, IDirect3DRM3_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI IDirect3DRMImpl_QueryInterface(IDirect3DRM* iface, REFIID riid, void** ppvObject)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    TRACE("(%p/%p)->(%s,%p)\n", iface, This, debugstr_guid(riid), ppvObject);

    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_IDirect3DRM))
    {
        *ppvObject = &This->IDirect3DRM_iface;
    }
    else if(IsEqualGUID(riid, &IID_IDirect3DRM2))
    {
        *ppvObject = &This->IDirect3DRM2_iface;
    }
    else if(IsEqualGUID(riid, &IID_IDirect3DRM3))
    {
        *ppvObject = &This->IDirect3DRM3_iface;
    }
    else
    {
        FIXME("interface %s not implemented\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }

    IDirect3DRM_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI IDirect3DRMImpl_AddRef(IDirect3DRM* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    TRACE("(%p/%p)\n", iface, This);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI IDirect3DRMImpl_Release(IDirect3DRM* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p/%p)\n", iface, This);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

/*** IDirect3DRM methods ***/
static HRESULT WINAPI IDirect3DRMImpl_CreateObject(IDirect3DRM* iface, REFCLSID rclsid, LPUNKNOWN pUnkOuter, REFIID riid, LPVOID *ppvObj)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s,%p,%s,%p): stub\n", iface, This, debugstr_guid(rclsid), pUnkOuter, debugstr_guid(riid), ppvObj);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateFrame(IDirect3DRM* iface, LPDIRECT3DRMFRAME pFrameParent, LPDIRECT3DRMFRAME * ppFrame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    TRACE("(%p/%p)->(%p,%p)\n", iface, This, pFrameParent, ppFrame);

    if (pFrameParent)
        FIXME("(%p/%p): Parent frame not yet supported\n", iface, This);

    return Direct3DRMFrame_create(&IID_IDirect3DRMFrame, (IUnknown**)ppFrame);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateMesh(IDirect3DRM* iface, LPDIRECT3DRMMESH * ppMesh)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppMesh);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateMeshBuilder(IDirect3DRM* iface, LPDIRECT3DRMMESHBUILDER * ppMeshBuilder)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, ppMeshBuilder);

    return Direct3DRMMeshBuilder_create(&IID_IDirect3DRMMeshBuilder, (IUnknown**)ppMeshBuilder);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateFace(IDirect3DRM* iface, LPDIRECT3DRMFACE * ppFace)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppFace);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateAnimation(IDirect3DRM* iface, LPDIRECT3DRMANIMATION * ppAnimation)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppAnimation);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateAnimationSet(IDirect3DRM* iface, LPDIRECT3DRMANIMATIONSET * ppAnimationSet)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppAnimationSet);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateTexture(IDirect3DRM* iface, LPD3DRMIMAGE pImage, LPDIRECT3DRMTEXTURE * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, pImage, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateLight(IDirect3DRM* iface, D3DRMLIGHTTYPE type, D3DCOLOR color, LPDIRECT3DRMLIGHT * ppLight)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%d,%d,%p): stub\n", iface, This, type, color, ppLight);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateLightRGB(IDirect3DRM* iface, D3DRMLIGHTTYPE type,  D3DVALUE red, D3DVALUE green, D3DVALUE blue, LPDIRECT3DRMLIGHT * ppLight)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%d,%f,%f,%f,%p): stub\n", iface, This, type, red, green, blue, ppLight);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_Material(IDirect3DRM* iface, D3DVALUE m, LPDIRECT3DRMMATERIAL * ppMaterial)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%f,%p): stub\n", iface, This, m, ppMaterial);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateDevice(IDirect3DRM* iface, DWORD width, DWORD height, LPDIRECT3DRMDEVICE * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%u,%u,%p): partial stub\n", iface, This, width, height, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateDeviceFromSurface(IDirect3DRM* iface, LPGUID pGUID, LPDIRECTDRAW pDD, LPDIRECTDRAWSURFACE pDDSBack, LPDIRECT3DRMDEVICE * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s,%p,%p,%p): partial stub\n", iface, This, debugstr_guid(pGUID), pDD,
          pDDSBack, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateDeviceFromD3D(IDirect3DRM* iface, LPDIRECT3D pD3D, LPDIRECT3DDEVICE pD3DDev, LPDIRECT3DRMDEVICE * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p,%p): partial stub\n", iface, This, pD3D, pD3DDev, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateDeviceFromClipper(IDirect3DRM* iface, LPDIRECTDRAWCLIPPER pDDClipper, LPGUID pGUID, int width, int height, LPDIRECT3DRMDEVICE * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%s,%d,%d,%p): partial stub\n", iface, This, pDDClipper,
          debugstr_guid(pGUID), width, height, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateTextureFromSurface(IDirect3DRM* iface, LPDIRECTDRAWSURFACE pDDS, LPDIRECT3DRMTEXTURE * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, pDDS, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateShadow(IDirect3DRM* iface, LPDIRECT3DRMVISUAL pVisual, LPDIRECT3DRMLIGHT pLight, D3DVALUE px, D3DVALUE py, D3DVALUE pz, D3DVALUE nx, D3DVALUE ny, D3DVALUE nz, LPDIRECT3DRMVISUAL * ppVisual)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, pVisual, pLight, px, py, pz, nx, ny, nz, ppVisual);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateViewport(IDirect3DRM* iface, LPDIRECT3DRMDEVICE pDevice, LPDIRECT3DRMFRAME pFrame, DWORD xpos, DWORD ypos, DWORD width, DWORD height, LPDIRECT3DRMVIEWPORT * ppViewport)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p,%d,%d,%d,%d,%p): partial stub\n", iface, This, pDevice, pFrame,
          xpos, ypos, width, height, ppViewport);

    return Direct3DRMViewport_create(&IID_IDirect3DRMViewport, (IUnknown**)ppViewport);
}

static HRESULT WINAPI IDirect3DRMImpl_CreateWrap(IDirect3DRM* iface, D3DRMWRAPTYPE type, LPDIRECT3DRMFRAME pFrame, D3DVALUE ox, D3DVALUE oy, D3DVALUE oz, D3DVALUE dx, D3DVALUE dy, D3DVALUE dz, D3DVALUE ux, D3DVALUE uy, D3DVALUE uz, D3DVALUE ou, D3DVALUE ov, D3DVALUE su, D3DVALUE sv, LPDIRECT3DRMWRAP * ppWrap)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%d,%p,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, type, pFrame, ox, oy, oz, dx, dy, dz, ux, uy, uz, ou, ov, su, sv, ppWrap);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_CreateUserVisual(IDirect3DRM* iface, D3DRMUSERVISUALCALLBACK cb, LPVOID pArg, LPDIRECT3DRMUSERVISUAL * ppUserVisual)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p,%p): stub\n", iface, This, cb, pArg, ppUserVisual);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_LoadTexture(IDirect3DRM* iface, const char * filename, LPDIRECT3DRMTEXTURE * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, filename, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_LoadTextureFromResource(IDirect3DRM* iface, HRSRC rs, LPDIRECT3DRMTEXTURE * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, rs, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_SetSearchPath(IDirect3DRM* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_AddSearchPath(IDirect3DRM* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_GetSearchPath(IDirect3DRM* iface, DWORD *size_return, LPSTR path_return)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%s): stub\n", iface, This, size_return, path_return);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_SetDefaultTextureColors(IDirect3DRM* iface, DWORD nb_colors)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_colors);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_SetDefaultTextureShades(IDirect3DRM* iface, DWORD nb_shades)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_shades);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_GetDevices(IDirect3DRM* iface, LPDIRECT3DRMDEVICEARRAY * ppDeviceArray)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppDeviceArray);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_GetNamedObject(IDirect3DRM* iface, const char * pName, LPDIRECT3DRMOBJECT * ppObject)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, pName, ppObject);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_EnumerateObjects(IDirect3DRM* iface, D3DRMOBJECTCALLBACK cb, LPVOID pArg)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, cb, pArg);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_Load(IDirect3DRM* iface, LPVOID pObjSource, LPVOID pObjID, LPIID * ppGUIDs, DWORD nb_GUIDs, D3DRMLOADOPTIONS LOFlags, D3DRMLOADCALLBACK LoadProc, LPVOID pArgLP, D3DRMLOADTEXTURECALLBACK LoadTextureProc, LPVOID pArgLTP, LPDIRECT3DRMFRAME pParentFrame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%p,%p,%p,%d,%d,%p,%p,%p,%p,%p): stub\n", iface, This, pObjSource, pObjID, ppGUIDs, nb_GUIDs, LOFlags, LoadProc, pArgLP, LoadTextureProc, pArgLTP, pParentFrame);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRMImpl_Tick(IDirect3DRM* iface, D3DVALUE tick)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM(iface);

    FIXME("(%p/%p)->(%f): stub\n", iface, This, tick);

    return E_NOTIMPL;
}

static const struct IDirect3DRMVtbl Direct3DRM_Vtbl =
{
    IDirect3DRMImpl_QueryInterface,
    IDirect3DRMImpl_AddRef,
    IDirect3DRMImpl_Release,
    IDirect3DRMImpl_CreateObject,
    IDirect3DRMImpl_CreateFrame,
    IDirect3DRMImpl_CreateMesh,
    IDirect3DRMImpl_CreateMeshBuilder,
    IDirect3DRMImpl_CreateFace,
    IDirect3DRMImpl_CreateAnimation,
    IDirect3DRMImpl_CreateAnimationSet,
    IDirect3DRMImpl_CreateTexture,
    IDirect3DRMImpl_CreateLight,
    IDirect3DRMImpl_CreateLightRGB,
    IDirect3DRMImpl_Material,
    IDirect3DRMImpl_CreateDevice,
    IDirect3DRMImpl_CreateDeviceFromSurface,
    IDirect3DRMImpl_CreateDeviceFromD3D,
    IDirect3DRMImpl_CreateDeviceFromClipper,
    IDirect3DRMImpl_CreateTextureFromSurface,
    IDirect3DRMImpl_CreateShadow,
    IDirect3DRMImpl_CreateViewport,
    IDirect3DRMImpl_CreateWrap,
    IDirect3DRMImpl_CreateUserVisual,
    IDirect3DRMImpl_LoadTexture,
    IDirect3DRMImpl_LoadTextureFromResource,
    IDirect3DRMImpl_SetSearchPath,
    IDirect3DRMImpl_AddSearchPath,
    IDirect3DRMImpl_GetSearchPath,
    IDirect3DRMImpl_SetDefaultTextureColors,
    IDirect3DRMImpl_SetDefaultTextureShades,
    IDirect3DRMImpl_GetDevices,
    IDirect3DRMImpl_GetNamedObject,
    IDirect3DRMImpl_EnumerateObjects,
    IDirect3DRMImpl_Load,
    IDirect3DRMImpl_Tick
};


/*** IUnknown methods ***/
static HRESULT WINAPI IDirect3DRM2Impl_QueryInterface(IDirect3DRM2* iface, REFIID riid,
                                                      void** ppvObject)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);
    return IDirect3DRM_QueryInterface(&This->IDirect3DRM_iface, riid, ppvObject);
}

static ULONG WINAPI IDirect3DRM2Impl_AddRef(IDirect3DRM2* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);
    return IDirect3DRM_AddRef(&This->IDirect3DRM_iface);
}

static ULONG WINAPI IDirect3DRM2Impl_Release(IDirect3DRM2* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);
    return IDirect3DRM_Release(&This->IDirect3DRM_iface);
}

/*** IDirect3DRM2 methods ***/
static HRESULT WINAPI IDirect3DRM2Impl_CreateObject(IDirect3DRM2* iface, REFCLSID rclsid,
                                                    LPUNKNOWN pUnkOuter, REFIID riid,
                                                    LPVOID *ppvObj)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s,%p,%s,%p): stub\n", iface, This, debugstr_guid(rclsid), pUnkOuter,
                                            debugstr_guid(riid), ppvObj);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateFrame(IDirect3DRM2* iface,
                                                   LPDIRECT3DRMFRAME pFrameParent,
                                                   LPDIRECT3DRMFRAME2 * ppFrame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    TRACE("(%p/%p)->(%p,%p)\n", iface, This, pFrameParent, ppFrame);

    if (pFrameParent)
        FIXME("(%p/%p): Parent frame not yet supported\n", iface, This);

    return Direct3DRMFrame_create(&IID_IDirect3DRMFrame2, (IUnknown**)ppFrame);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateMesh(IDirect3DRM2* iface, LPDIRECT3DRMMESH * ppMesh)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppMesh);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateMeshBuilder(IDirect3DRM2* iface,
                                                         LPDIRECT3DRMMESHBUILDER2 * ppMeshBuilder)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, ppMeshBuilder);

    return Direct3DRMMeshBuilder_create(&IID_IDirect3DRMMeshBuilder2, (IUnknown**)ppMeshBuilder);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateFace(IDirect3DRM2* iface, LPDIRECT3DRMFACE * ppFace)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppFace);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateAnimation(IDirect3DRM2* iface,
                                                       LPDIRECT3DRMANIMATION * ppAnimation)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppAnimation);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateAnimationSet(IDirect3DRM2* iface,
                                                          LPDIRECT3DRMANIMATIONSET * ppAnimationSet)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppAnimationSet);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateTexture(IDirect3DRM2* iface, LPD3DRMIMAGE pImage,
                                                     LPDIRECT3DRMTEXTURE2 * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, pImage, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateLight(IDirect3DRM2* iface, D3DRMLIGHTTYPE type,
                                                   D3DCOLOR color, LPDIRECT3DRMLIGHT * ppLight)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%d,%d,%p): stub\n", iface, This, type, color, ppLight);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateLightRGB(IDirect3DRM2* iface, D3DRMLIGHTTYPE type,
                                                      D3DVALUE red, D3DVALUE green, D3DVALUE blue,
                                                      LPDIRECT3DRMLIGHT * ppLight)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%d,%f,%f,%f,%p): stub\n", iface, This, type, red, green, blue, ppLight);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_Material(IDirect3DRM2* iface, D3DVALUE m,
                                                LPDIRECT3DRMMATERIAL * ppMaterial)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%f,%p): stub\n", iface, This, m, ppMaterial);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateDevice(IDirect3DRM2* iface, DWORD width, DWORD height,
                                                    LPDIRECT3DRMDEVICE2 * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%u,%u,%p): partial stub\n", iface, This, width, height, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice2, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateDeviceFromSurface(IDirect3DRM2* iface, LPGUID pGUID,
                                                               LPDIRECTDRAW pDD,
                                                               LPDIRECTDRAWSURFACE pDDSBack,
                                                               LPDIRECT3DRMDEVICE2 * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s,%p,%p,%p): partial stub\n", iface, This, debugstr_guid(pGUID),
          pDD, pDDSBack, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice2, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateDeviceFromD3D(IDirect3DRM2* iface, LPDIRECT3D2 pD3D,
                                                           LPDIRECT3DDEVICE2 pD3DDev,
                                                           LPDIRECT3DRMDEVICE2 * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%p): partial stub\n", iface, This, pD3D, pD3DDev, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice2, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateDeviceFromClipper(IDirect3DRM2* iface,
                                                               LPDIRECTDRAWCLIPPER pDDClipper,
                                                               LPGUID pGUID, int width, int height,
                                                               LPDIRECT3DRMDEVICE2 * ppDevice)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%s,%d,%d,%p): partial stub\n", iface, This, pDDClipper,
          debugstr_guid(pGUID), width, height, ppDevice);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice2, (IUnknown**)ppDevice);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateTextureFromSurface(IDirect3DRM2* iface,
                                                                LPDIRECTDRAWSURFACE pDDS,
                                                                LPDIRECT3DRMTEXTURE2 * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, pDDS, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateShadow(IDirect3DRM2* iface, LPDIRECT3DRMVISUAL pVisual,
                                                    LPDIRECT3DRMLIGHT pLight,
                                                    D3DVALUE px, D3DVALUE py, D3DVALUE pz,
                                                    D3DVALUE nx, D3DVALUE ny, D3DVALUE nz,
                                                    LPDIRECT3DRMVISUAL * ppVisual)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, pVisual, pLight, px, py, pz,
                                                           nx, ny, nz, ppVisual);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateViewport(IDirect3DRM2* iface,
                                                      LPDIRECT3DRMDEVICE pDevice,
                                                      LPDIRECT3DRMFRAME pFrame,
                                                      DWORD xpos, DWORD ypos,
                                                      DWORD width, DWORD height,
                                                      LPDIRECT3DRMVIEWPORT * ppViewport)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%d,%d,%d,%d,%p): partial stub\n", iface, This, pDevice, pFrame,
          xpos, ypos, width, height, ppViewport);

    return Direct3DRMViewport_create(&IID_IDirect3DRMViewport, (IUnknown**)ppViewport);
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateWrap(IDirect3DRM2* iface, D3DRMWRAPTYPE type,
                                                  LPDIRECT3DRMFRAME pFrame,
                                                  D3DVALUE ox, D3DVALUE oy, D3DVALUE oz,
                                                  D3DVALUE dx, D3DVALUE dy, D3DVALUE dz,
                                                  D3DVALUE ux, D3DVALUE uy, D3DVALUE uz,
                                                  D3DVALUE ou, D3DVALUE ov, D3DVALUE su,
                                                  D3DVALUE sv, LPDIRECT3DRMWRAP * ppWrap)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%d,%p,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, type,
          pFrame, ox, oy, oz, dx, dy, dz, ux, uy, uz, ou, ov, su, sv, ppWrap);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateUserVisual(IDirect3DRM2* iface,
                                                        D3DRMUSERVISUALCALLBACK cb, LPVOID pArg,
                                                        LPDIRECT3DRMUSERVISUAL * ppUserVisual)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%p): stub\n", iface, This, cb, pArg, ppUserVisual);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_LoadTexture(IDirect3DRM2* iface, const char * filename,
                                                   LPDIRECT3DRMTEXTURE2 * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, filename, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_LoadTextureFromResource(IDirect3DRM2* iface, HMODULE hModule,
                                                               LPCSTR strName, LPCSTR strType,
                                                               LPDIRECT3DRMTEXTURE2 * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%p,%p): stub\n", iface, This, hModule, strName, strType, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_SetSearchPath(IDirect3DRM2* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_AddSearchPath(IDirect3DRM2* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_GetSearchPath(IDirect3DRM2* iface, DWORD *size_return,
                                                     LPSTR path_return)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%s): stub\n", iface, This, size_return, path_return);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_SetDefaultTextureColors(IDirect3DRM2* iface, DWORD nb_colors)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_colors);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_SetDefaultTextureShades(IDirect3DRM2* iface, DWORD nb_shades)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_shades);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_GetDevices(IDirect3DRM2* iface,
                                                  LPDIRECT3DRMDEVICEARRAY * ppDeviceArray)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppDeviceArray);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_GetNamedObject(IDirect3DRM2* iface, const char * pName,
                                                      LPDIRECT3DRMOBJECT * ppObject)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, pName, ppObject);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_EnumerateObjects(IDirect3DRM2* iface, D3DRMOBJECTCALLBACK cb,
                                                        LPVOID pArg)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, cb, pArg);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_Load(IDirect3DRM2* iface, LPVOID pObjSource, LPVOID pObjID,
                                            LPIID * ppGUIDs, DWORD nb_GUIDs,
                                            D3DRMLOADOPTIONS LOFlags, D3DRMLOADCALLBACK LoadProc,
                                            LPVOID pArgLP, D3DRMLOADTEXTURECALLBACK LoadTextureProc,
                                            LPVOID pArgLTP, LPDIRECT3DRMFRAME pParentFrame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p,%p,%p,%d,%d,%p,%p,%p,%p,%p): stub\n", iface, This, pObjSource, pObjID,
          ppGUIDs, nb_GUIDs, LOFlags, LoadProc, pArgLP, LoadTextureProc, pArgLTP, pParentFrame);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_Tick(IDirect3DRM2* iface, D3DVALUE tick)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%f): stub\n", iface, This, tick);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM2Impl_CreateProgressiveMesh(IDirect3DRM2* iface,
                                                             LPDIRECT3DRMPROGRESSIVEMESH * ppMesh)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM2(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, ppMesh);

    return E_NOTIMPL;
}

static const struct IDirect3DRM2Vtbl Direct3DRM2_Vtbl =
{
    IDirect3DRM2Impl_QueryInterface,
    IDirect3DRM2Impl_AddRef,
    IDirect3DRM2Impl_Release,
    IDirect3DRM2Impl_CreateObject,
    IDirect3DRM2Impl_CreateFrame,
    IDirect3DRM2Impl_CreateMesh,
    IDirect3DRM2Impl_CreateMeshBuilder,
    IDirect3DRM2Impl_CreateFace,
    IDirect3DRM2Impl_CreateAnimation,
    IDirect3DRM2Impl_CreateAnimationSet,
    IDirect3DRM2Impl_CreateTexture,
    IDirect3DRM2Impl_CreateLight,
    IDirect3DRM2Impl_CreateLightRGB,
    IDirect3DRM2Impl_Material,
    IDirect3DRM2Impl_CreateDevice,
    IDirect3DRM2Impl_CreateDeviceFromSurface,
    IDirect3DRM2Impl_CreateDeviceFromD3D,
    IDirect3DRM2Impl_CreateDeviceFromClipper,
    IDirect3DRM2Impl_CreateTextureFromSurface,
    IDirect3DRM2Impl_CreateShadow,
    IDirect3DRM2Impl_CreateViewport,
    IDirect3DRM2Impl_CreateWrap,
    IDirect3DRM2Impl_CreateUserVisual,
    IDirect3DRM2Impl_LoadTexture,
    IDirect3DRM2Impl_LoadTextureFromResource,
    IDirect3DRM2Impl_SetSearchPath,
    IDirect3DRM2Impl_AddSearchPath,
    IDirect3DRM2Impl_GetSearchPath,
    IDirect3DRM2Impl_SetDefaultTextureColors,
    IDirect3DRM2Impl_SetDefaultTextureShades,
    IDirect3DRM2Impl_GetDevices,
    IDirect3DRM2Impl_GetNamedObject,
    IDirect3DRM2Impl_EnumerateObjects,
    IDirect3DRM2Impl_Load,
    IDirect3DRM2Impl_Tick,
    IDirect3DRM2Impl_CreateProgressiveMesh
};


/*** IUnknown methods ***/
static HRESULT WINAPI IDirect3DRM3Impl_QueryInterface(IDirect3DRM3* iface, REFIID riid,
                                                      void** ppvObject)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);
    return IDirect3DRM_QueryInterface(&This->IDirect3DRM_iface, riid, ppvObject);
}

static ULONG WINAPI IDirect3DRM3Impl_AddRef(IDirect3DRM3* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);
    return IDirect3DRM_AddRef(&This->IDirect3DRM_iface);
}

static ULONG WINAPI IDirect3DRM3Impl_Release(IDirect3DRM3* iface)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);
    return IDirect3DRM_Release(&This->IDirect3DRM_iface);
}

/*** IDirect3DRM3 methods ***/
static HRESULT WINAPI IDirect3DRM3Impl_CreateObject(IDirect3DRM3* iface, REFCLSID rclsid,
                                                    LPUNKNOWN unkwn, REFIID riid, LPVOID* object)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s,%p,%s,%p): stub\n", iface, This, debugstr_guid(rclsid), unkwn,
          debugstr_guid(riid), object);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateFrame(IDirect3DRM3* iface,
                                                   LPDIRECT3DRMFRAME3 FrameParent,
                                                   LPDIRECT3DRMFRAME3* Frame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    TRACE("(%p/%p)->(%p,%p)\n", iface, This, FrameParent, Frame);

    if (FrameParent)
        FIXME("(%p/%p): Parent frame not yet supported\n", iface, This);

    return Direct3DRMFrame_create(&IID_IDirect3DRMFrame3, (IUnknown**)Frame);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateMesh(IDirect3DRM3* iface, LPDIRECT3DRMMESH* Mesh)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, Mesh);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateMeshBuilder(IDirect3DRM3* iface,
                                                         LPDIRECT3DRMMESHBUILDER3* ppMeshBuilder)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, ppMeshBuilder);

    return Direct3DRMMeshBuilder_create(&IID_IDirect3DRMMeshBuilder3, (IUnknown**)ppMeshBuilder);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateFace(IDirect3DRM3* iface, LPDIRECT3DRMFACE2* Face)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, Face);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateAnimation(IDirect3DRM3* iface,
                                                       LPDIRECT3DRMANIMATION2* Animation)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, Animation);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateAnimationSet(IDirect3DRM3* iface,
                                                          LPDIRECT3DRMANIMATIONSET2* AnimationSet)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, AnimationSet);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateTexture(IDirect3DRM3* iface, LPD3DRMIMAGE Image,
                                                     LPDIRECT3DRMTEXTURE3* Texture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, Image, Texture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateLight(IDirect3DRM3* iface, D3DRMLIGHTTYPE type,
                                                   D3DCOLOR color, LPDIRECT3DRMLIGHT* Light)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d,%d,%p): stub\n", iface, This, type, color, Light);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateLightRGB(IDirect3DRM3* iface, D3DRMLIGHTTYPE type,
                                                      D3DVALUE red, D3DVALUE green, D3DVALUE blue,
                                                      LPDIRECT3DRMLIGHT* Light)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d,%f,%f,%f,%p): stub\n", iface, This, type, red, green, blue, Light);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_Material(IDirect3DRM3* iface, D3DVALUE m,
                                                LPDIRECT3DRMMATERIAL2* Material)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%f,%p): stub\n", iface, This, m, Material);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateDevice(IDirect3DRM3* iface, DWORD width, DWORD height,
                                                    LPDIRECT3DRMDEVICE3* device)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d,%d,%p): partial stub\n", iface, This, width, height, device);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice3, (IUnknown**)device);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateDeviceFromSurface(IDirect3DRM3* iface, LPGUID pGUID,
                                                               LPDIRECTDRAW dd,
                                                               LPDIRECTDRAWSURFACE back,
                                                               LPDIRECT3DRMDEVICE3* device)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s,%p,%p,%p): partial stub\n", iface, This, debugstr_guid(pGUID), dd, back, device);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice3, (IUnknown**)device);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateDeviceFromD3D(IDirect3DRM3* iface, LPDIRECT3D2 d3d,
                                                           LPDIRECT3DDEVICE2 d3ddev,
                                                           LPDIRECT3DRMDEVICE3* device)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%p): partial stub\n", iface, This, d3d, d3ddev, device);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice3, (IUnknown**)device);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateDeviceFromClipper(IDirect3DRM3* iface,
                                                               LPDIRECTDRAWCLIPPER clipper,
                                                               LPGUID GUID, int width, int height,
                                                               LPDIRECT3DRMDEVICE3* device)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%s,%d,%d,%p): partial stub\n", iface, This, clipper, debugstr_guid(GUID),
          width, height, device);

    return Direct3DRMDevice_create(&IID_IDirect3DRMDevice3, (IUnknown**)device);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateShadow(IDirect3DRM3* iface, LPUNKNOWN Visual1,
                                                    LPDIRECT3DRMLIGHT Light, D3DVALUE px,
                                                    D3DVALUE py, D3DVALUE pz, D3DVALUE nx,
                                                    D3DVALUE ny, D3DVALUE nz,
                                                    LPDIRECT3DRMSHADOW2* Visual2)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, Visual1, Light, px, py, pz,
          nx, ny, nz, Visual2);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateTextureFromSurface(IDirect3DRM3* iface,
                                                                LPDIRECTDRAWSURFACE surface,
                                                                LPDIRECT3DRMTEXTURE3* texture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, surface, texture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateViewport(IDirect3DRM3* iface,
                                                      LPDIRECT3DRMDEVICE3 Device,
                                                      LPDIRECT3DRMFRAME3 frame, DWORD xpos,
                                                      DWORD ypos, DWORD width, DWORD height,
                                                      LPDIRECT3DRMVIEWPORT2* viewport)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%d,%d,%d,%d,%p): stub\n", iface, This, Device, frame, xpos, ypos, width,
          height, viewport);

    return Direct3DRMViewport_create(&IID_IDirect3DRMViewport2, (IUnknown**)viewport);
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateWrap(IDirect3DRM3* iface, D3DRMWRAPTYPE type,
                                                  LPDIRECT3DRMFRAME3 frame,
                                                  D3DVALUE ox, D3DVALUE oy, D3DVALUE oz,
                                                  D3DVALUE dx, D3DVALUE dy, D3DVALUE dz,
                                                  D3DVALUE ux, D3DVALUE uy, D3DVALUE uz,
                                                  D3DVALUE ou, D3DVALUE ov, D3DVALUE su,
                                                  D3DVALUE sv, LPDIRECT3DRMWRAP* wrap)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d,%p,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%p): stub\n", iface, This, type,
          frame, ox, oy, oz, dx, dy, dz, ux, uy, uz, ou, ov, su, sv, wrap);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateUserVisual(IDirect3DRM3* iface,
                                                        D3DRMUSERVISUALCALLBACK cb, LPVOID arg,
                                                        LPDIRECT3DRMUSERVISUAL* UserVisual)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%p): stub\n", iface, This, cb, arg, UserVisual);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_LoadTexture(IDirect3DRM3* iface, const char* filename,
                                                   LPDIRECT3DRMTEXTURE3* Texture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, filename, Texture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_LoadTextureFromResource(IDirect3DRM3* iface, HMODULE mod,
                                                               LPCSTR strName, LPCSTR strType,
                                                               LPDIRECT3DRMTEXTURE3 * ppTexture)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%p,%p): stub\n", iface, This, mod, strName, strType, ppTexture);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_SetSearchPath(IDirect3DRM3* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_AddSearchPath(IDirect3DRM3* iface, LPCSTR path)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, path);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_GetSearchPath(IDirect3DRM3* iface, DWORD* size_return,
                                                     LPSTR path_return)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%s): stub\n", iface, This, size_return, path_return);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_SetDefaultTextureColors(IDirect3DRM3* iface, DWORD nb_colors)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_colors);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_SetDefaultTextureShades(IDirect3DRM3* iface, DWORD nb_shades)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, nb_shades);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_GetDevices(IDirect3DRM3* iface,
                                                  LPDIRECT3DRMDEVICEARRAY* DeviceArray)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, DeviceArray);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_GetNamedObject(IDirect3DRM3* iface, const char* Name,
                                                      LPDIRECT3DRMOBJECT* Object)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s,%p): stub\n", iface, This, Name, Object);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_EnumerateObjects(IDirect3DRM3* iface, D3DRMOBJECTCALLBACK cb,
                                                        LPVOID arg)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p): stub\n", iface, This, cb, arg);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_Load(IDirect3DRM3* iface, LPVOID ObjSource, LPVOID ObjID,
                                            LPIID* GUIDs, DWORD nb_GUIDs, D3DRMLOADOPTIONS LOFlags,
                                            D3DRMLOADCALLBACK LoadProc, LPVOID ArgLP,
                                            D3DRMLOADTEXTURECALLBACK LoadTextureProc, LPVOID ArgLTP,
                                            LPDIRECT3DRMFRAME3 ParentFrame)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p,%p,%p,%d,%d,%p,%p,%p,%p,%p): stub\n", iface, This, ObjSource, ObjID, GUIDs,
          nb_GUIDs, LOFlags, LoadProc, ArgLP, LoadTextureProc, ArgLTP, ParentFrame);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_Tick(IDirect3DRM3* iface, D3DVALUE tick)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%f): stub\n", iface, This, tick);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateProgressiveMesh(IDirect3DRM3* iface,
                                                             LPDIRECT3DRMPROGRESSIVEMESH Mesh)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, Mesh);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_RegisterClient(IDirect3DRM3* iface, REFGUID rguid,
                                                      LPDWORD id)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s, %p): stub\n", iface, This, debugstr_guid(rguid), id);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_UnregisterClient(IDirect3DRM3* iface, REFGUID rguid)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%s): stub\n", iface, This, debugstr_guid(rguid));

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_CreateClippedVisual(IDirect3DRM3* iface,
                                                           LPDIRECT3DRMVISUAL vis,
                                                           LPDIRECT3DRMCLIPPEDVISUAL* clippedvis)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p, %p): stub\n", iface, This, vis, clippedvis);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_SetOptions(IDirect3DRM3* iface, DWORD opt)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%d): stub\n", iface, This, opt);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirect3DRM3Impl_GetOptions(IDirect3DRM3* iface, LPDWORD opt)
{
    IDirect3DRMImpl *This = impl_from_IDirect3DRM3(iface);

    FIXME("(%p/%p)->(%p): stub\n", iface, This, opt);

    return E_NOTIMPL;
}

static const struct IDirect3DRM3Vtbl Direct3DRM3_Vtbl =
{
    IDirect3DRM3Impl_QueryInterface,
    IDirect3DRM3Impl_AddRef,
    IDirect3DRM3Impl_Release,
    IDirect3DRM3Impl_CreateObject,
    IDirect3DRM3Impl_CreateFrame,
    IDirect3DRM3Impl_CreateMesh,
    IDirect3DRM3Impl_CreateMeshBuilder,
    IDirect3DRM3Impl_CreateFace,
    IDirect3DRM3Impl_CreateAnimation,
    IDirect3DRM3Impl_CreateAnimationSet,
    IDirect3DRM3Impl_CreateTexture,
    IDirect3DRM3Impl_CreateLight,
    IDirect3DRM3Impl_CreateLightRGB,
    IDirect3DRM3Impl_Material,
    IDirect3DRM3Impl_CreateDevice,
    IDirect3DRM3Impl_CreateDeviceFromSurface,
    IDirect3DRM3Impl_CreateDeviceFromD3D,
    IDirect3DRM3Impl_CreateDeviceFromClipper,
    IDirect3DRM3Impl_CreateTextureFromSurface,
    IDirect3DRM3Impl_CreateShadow,
    IDirect3DRM3Impl_CreateViewport,
    IDirect3DRM3Impl_CreateWrap,
    IDirect3DRM3Impl_CreateUserVisual,
    IDirect3DRM3Impl_LoadTexture,
    IDirect3DRM3Impl_LoadTextureFromResource,
    IDirect3DRM3Impl_SetSearchPath,
    IDirect3DRM3Impl_AddSearchPath,
    IDirect3DRM3Impl_GetSearchPath,
    IDirect3DRM3Impl_SetDefaultTextureColors,
    IDirect3DRM3Impl_SetDefaultTextureShades,
    IDirect3DRM3Impl_GetDevices,
    IDirect3DRM3Impl_GetNamedObject,
    IDirect3DRM3Impl_EnumerateObjects,
    IDirect3DRM3Impl_Load,
    IDirect3DRM3Impl_Tick,
    IDirect3DRM3Impl_CreateProgressiveMesh,
    IDirect3DRM3Impl_RegisterClient,
    IDirect3DRM3Impl_UnregisterClient,
    IDirect3DRM3Impl_CreateClippedVisual,
    IDirect3DRM3Impl_SetOptions,
    IDirect3DRM3Impl_GetOptions
};

HRESULT Direct3DRM_create(IUnknown** ppObj)
{
    IDirect3DRMImpl* object;

    TRACE("(%p)\n", ppObj);

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DRMImpl));
    if (!object)
    {
        ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    object->IDirect3DRM_iface.lpVtbl = &Direct3DRM_Vtbl;
    object->IDirect3DRM2_iface.lpVtbl = &Direct3DRM2_Vtbl;
    object->IDirect3DRM3_iface.lpVtbl = &Direct3DRM3_Vtbl;
    object->ref = 1;

    *ppObj = (IUnknown*)&object->IDirect3DRM_iface;

    return S_OK;
}
