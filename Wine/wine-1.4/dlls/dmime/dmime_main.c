/* DirectMusicInteractiveEngine Main
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 * Copyright (C) 2003-2004 Raphael Junqueira
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dmime_private.h"
#include "rpcproxy.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmime);

static HINSTANCE instance;
LONG DMIME_refCount = 0;

typedef struct {
        IClassFactory IClassFactory_iface;
        HRESULT WINAPI (*fnCreateInstance)(REFIID riid, void **ppv, IUnknown *pUnkOuter);
} IClassFactoryImpl;

/******************************************************************
 *      IClassFactory implementation
 */
static inline IClassFactoryImpl *impl_from_IClassFactory(IClassFactory *iface)
{
        return CONTAINING_RECORD(iface, IClassFactoryImpl, IClassFactory_iface);
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
        if (ppv == NULL)
                return E_POINTER;

        if (IsEqualGUID(&IID_IUnknown, riid))
                TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
        else if (IsEqualGUID(&IID_IClassFactory, riid))
                TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
        else {
                FIXME("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppv);
                *ppv = NULL;
                return E_NOINTERFACE;
        }

        *ppv = iface;
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
        DMIME_LockModule();

        return 2; /* non-heap based object */
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
        DMIME_UnlockModule();

        return 1; /* non-heap based object */
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *pUnkOuter,
        REFIID riid, void **ppv)
{
        IClassFactoryImpl *This = impl_from_IClassFactory(iface);

        TRACE ("(%p, %s, %p)\n", pUnkOuter, debugstr_dmguid(riid), ppv);

        return This->fnCreateInstance(riid, ppv, pUnkOuter);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL dolock)
{
        TRACE("(%d)\n", dolock);

        if (dolock)
                DMIME_LockModule();
        else
                DMIME_UnlockModule();

        return S_OK;
}

static const IClassFactoryVtbl classfactory_vtbl = {
        ClassFactory_QueryInterface,
        ClassFactory_AddRef,
        ClassFactory_Release,
        ClassFactory_CreateInstance,
        ClassFactory_LockServer
};


static IClassFactoryImpl Performance_CF = {{&classfactory_vtbl},
                                           DMUSIC_CreateDirectMusicPerformanceImpl};
static IClassFactoryImpl Segment_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicSegmentImpl};
static IClassFactoryImpl SegmentState_CF = {{&classfactory_vtbl},
                                            DMUSIC_CreateDirectMusicSegmentStateImpl};
static IClassFactoryImpl Graph_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicGraphImpl};
static IClassFactoryImpl TempoTrack_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicTempoTrack};
static IClassFactoryImpl SeqTrack_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicSeqTrack};
static IClassFactoryImpl SysExTrack_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicSysExTrack};
static IClassFactoryImpl TimeSigTrack_CF = {{&classfactory_vtbl},
                                            DMUSIC_CreateDirectMusicTimeSigTrack};
static IClassFactoryImpl ParamControlTrack_CF = {{&classfactory_vtbl},
                                                 DMUSIC_CreateDirectMusicParamControlTrack};
static IClassFactoryImpl MarkerTrack_CF = {{&classfactory_vtbl},
                                           DMUSIC_CreateDirectMusicMarkerTrack};
static IClassFactoryImpl LyricsTrack_CF = {{&classfactory_vtbl},
                                           DMUSIC_CreateDirectMusicLyricsTrack};
static IClassFactoryImpl SegTriggerTrack_CF = {{&classfactory_vtbl},
                                               DMUSIC_CreateDirectMusicSegTriggerTrack};
static IClassFactoryImpl AudioPath_CF = {{&classfactory_vtbl},
                                         DMUSIC_CreateDirectMusicAudioPathImpl};
static IClassFactoryImpl WaveTrack_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicWaveTrack};

/******************************************************************
 *		DllMain
 *
 *
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
                instance = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
		/* FIXME: Initialisation */
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		/* FIXME: Cleanup */
	}

	return TRUE;
}


/******************************************************************
 *		DllCanUnloadNow (DMIME.1)
 *
 *
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
	return DMIME_refCount != 0 ? S_FALSE : S_OK;
}


/******************************************************************
 *		DllGetClassObject (DMIME.@)
 *
 *
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_dmguid(rclsid), debugstr_dmguid(riid), ppv);
    if (IsEqualCLSID (rclsid, &CLSID_DirectMusicPerformance) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &Performance_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicSegment) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &Segment_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicSegmentState) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &SegmentState_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicGraph) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &Graph_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicTempoTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &TempoTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicSeqTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &SeqTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicSysExTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &SysExTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicTimeSigTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &TimeSigTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicParamControlTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &ParamControlTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicMarkerTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &MarkerTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicLyricsTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &LyricsTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicSegTriggerTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &SegTriggerTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicAudioPath) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &AudioPath_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicWaveTrack) && IsEqualIID (riid, &IID_IClassFactory)) {
                *ppv = &WaveTrack_CF;
		IClassFactory_AddRef((IClassFactory*)*ppv);
		return S_OK;
	} 
	
    WARN("(%s, %s, %p): no interface found.\n", debugstr_dmguid(rclsid), debugstr_dmguid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *		DllRegisterServer (DMIME.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( instance );
}

/***********************************************************************
 *		DllUnregisterServer (DMIME.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( instance );
}
