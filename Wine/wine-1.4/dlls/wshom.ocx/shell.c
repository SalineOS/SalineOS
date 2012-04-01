/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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

#include "wshom_private.h"
#include "wshom.h"

#include "shlobj.h"

#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(wshom);

static const char *debugstr_variant(const VARIANT *v)
{
    if(!v)
        return "(null)";

    switch(V_VT(v)) {
    case VT_EMPTY:
        return "{VT_EMPTY}";
    case VT_NULL:
        return "{VT_NULL}";
    case VT_I4:
        return wine_dbg_sprintf("{VT_I4: %d}", V_I4(v));
    case VT_R8:
        return wine_dbg_sprintf("{VT_R8: %lf}", V_R8(v));
    case VT_BSTR:
        return wine_dbg_sprintf("{VT_BSTR: %s}", debugstr_w(V_BSTR(v)));
    case VT_DISPATCH:
        return wine_dbg_sprintf("{VT_DISPATCH: %p}", V_DISPATCH(v));
    case VT_BOOL:
        return wine_dbg_sprintf("{VT_BOOL: %x}", V_BOOL(v));
    case VT_UNKNOWN:
        return wine_dbg_sprintf("{VT_UNKNOWN: %p}", V_UNKNOWN(v));
    case VT_UINT:
        return wine_dbg_sprintf("{VT_UINT: %u}", V_UINT(v));
    case VT_BSTR|VT_BYREF:
        return wine_dbg_sprintf("{VT_BSTR|VT_BYREF: ptr %p, data %s}",
            V_BSTRREF(v), debugstr_w(V_BSTRREF(v) ? *V_BSTRREF(v) : NULL));
    default:
        return wine_dbg_sprintf("{vt %d}", V_VT(v));
    }
}

static IWshShell3 WshShell3;

typedef struct
{
    IWshCollection IWshCollection_iface;
    LONG ref;
} WshCollection;

typedef struct
{
    IWshShortcut IWshShortcut_iface;
    LONG ref;

    IShellLinkW *link;
    BSTR path_link;
} WshShortcut;

static inline WshCollection *impl_from_IWshCollection( IWshCollection *iface )
{
    return CONTAINING_RECORD(iface, WshCollection, IWshCollection_iface);
}

static inline WshShortcut *impl_from_IWshShortcut( IWshShortcut *iface )
{
    return CONTAINING_RECORD(iface, WshShortcut, IWshShortcut_iface);
}

static HRESULT WINAPI WshCollection_QueryInterface(IWshCollection *iface, REFIID riid, void **ppv)
{
    WshCollection *This = impl_from_IWshCollection(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(riid, &IID_IUnknown)  ||
        IsEqualGUID(riid, &IID_IDispatch) ||
        IsEqualGUID(riid, &IID_IWshCollection))
    {
        *ppv = iface;
    }else {
        FIXME("Unknown iface %s\n", debugstr_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI WshCollection_AddRef(IWshCollection *iface)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref = %d\n", This, ref);
    return ref;
}

static ULONG WINAPI WshCollection_Release(IWshCollection *iface)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p) ref = %d\n", This, ref);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

static HRESULT WINAPI WshCollection_GetTypeInfoCount(IWshCollection *iface, UINT *pctinfo)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    TRACE("(%p)->(%p)\n", This, pctinfo);
    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI WshCollection_GetTypeInfo(IWshCollection *iface, UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return get_typeinfo(IWshCollection_tid, ppTInfo);
}

static HRESULT WINAPI WshCollection_GetIDsOfNames(IWshCollection *iface, REFIID riid, LPOLESTR *rgszNames,
        UINT cNames, LCID lcid, DISPID *rgDispId)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hr = get_typeinfo(IWshCollection_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshCollection_Invoke(IWshCollection *iface, DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IWshCollection_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &This->IWshCollection_iface, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshCollection_Item(IWshCollection *iface, VARIANT *index, VARIANT *value)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    static const WCHAR allusersdesktopW[] = {'A','l','l','U','s','e','r','s','D','e','s','k','t','o','p',0};
    static const WCHAR allusersprogramsW[] = {'A','l','l','U','s','e','r','s','P','r','o','g','r','a','m','s',0};
    static const WCHAR desktopW[] = {'D','e','s','k','t','o','p',0};
    PIDLIST_ABSOLUTE pidl;
    WCHAR pathW[MAX_PATH];
    int kind = 0;
    BSTR folder;
    HRESULT hr;

    TRACE("(%p)->(%s %p)\n", This, debugstr_variant(index), value);

    if (V_VT(index) != VT_BSTR)
    {
        FIXME("only BSTR index supported, got %d\n", V_VT(index));
        return E_NOTIMPL;
    }

    folder = V_BSTR(index);
    if (!strcmpiW(folder, desktopW))
        kind = CSIDL_DESKTOP;
    else if (!strcmpiW(folder, allusersdesktopW))
        kind = CSIDL_COMMON_DESKTOPDIRECTORY;
    else if (!strcmpiW(folder, allusersprogramsW))
        kind = CSIDL_COMMON_PROGRAMS;
    else
    {
        FIXME("folder kind %s not supported\n", debugstr_w(folder));
        return E_NOTIMPL;
    }

    hr = SHGetSpecialFolderLocation(NULL, kind, &pidl);
    if (hr != S_OK) return hr;

    if (SHGetPathFromIDListW(pidl, pathW))
    {
        V_VT(value) = VT_BSTR;
        V_BSTR(value) = SysAllocString(pathW);
        hr = V_BSTR(value) ? S_OK : E_OUTOFMEMORY;
    }
    else
        hr = E_FAIL;

    CoTaskMemFree(pidl);

    return hr;
}

static HRESULT WINAPI WshCollection_Count(IWshCollection *iface, LONG *count)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    FIXME("(%p)->(%p): stub\n", This, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshCollection_get_length(IWshCollection *iface, LONG *count)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    FIXME("(%p)->(%p): stub\n", This, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshCollection__NewEnum(IWshCollection *iface, IUnknown *Enum)
{
    WshCollection *This = impl_from_IWshCollection(iface);
    FIXME("(%p)->(%p): stub\n", This, Enum);
    return E_NOTIMPL;
}

static const IWshCollectionVtbl WshCollectionVtbl = {
    WshCollection_QueryInterface,
    WshCollection_AddRef,
    WshCollection_Release,
    WshCollection_GetTypeInfoCount,
    WshCollection_GetTypeInfo,
    WshCollection_GetIDsOfNames,
    WshCollection_Invoke,
    WshCollection_Item,
    WshCollection_Count,
    WshCollection_get_length,
    WshCollection__NewEnum
};

static HRESULT WshCollection_Create(IWshCollection **collection)
{
    WshCollection *This;

    This = HeapAlloc(GetProcessHeap(), 0, sizeof(*This));
    if (!This) return E_OUTOFMEMORY;

    This->IWshCollection_iface.lpVtbl = &WshCollectionVtbl;
    This->ref = 1;

    *collection = &This->IWshCollection_iface;

    return S_OK;
}

/* IWshShortcut */
static HRESULT WINAPI WshShortcut_QueryInterface(IWshShortcut *iface, REFIID riid, void **ppv)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(riid, &IID_IUnknown)  ||
        IsEqualGUID(riid, &IID_IDispatch) ||
        IsEqualGUID(riid, &IID_IWshShortcut))
    {
        *ppv = iface;
    }else {
        FIXME("Unknown iface %s\n", debugstr_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI WshShortcut_AddRef(IWshShortcut *iface)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref = %d\n", This, ref);
    return ref;
}

static ULONG WINAPI WshShortcut_Release(IWshShortcut *iface)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    LONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p) ref = %d\n", This, ref);

    if (!ref)
    {
        SysFreeString(This->path_link);
        IShellLinkW_Release(This->link);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI WshShortcut_GetTypeInfoCount(IWshShortcut *iface, UINT *pctinfo)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    TRACE("(%p)->(%p)\n", This, pctinfo);
    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI WshShortcut_GetTypeInfo(IWshShortcut *iface, UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return get_typeinfo(IWshShortcut_tid, ppTInfo);
}

static HRESULT WINAPI WshShortcut_GetIDsOfNames(IWshShortcut *iface, REFIID riid, LPOLESTR *rgszNames,
        UINT cNames, LCID lcid, DISPID *rgDispId)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hr = get_typeinfo(IWshShortcut_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshShortcut_Invoke(IWshShortcut *iface, DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IWshShortcut_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &This->IWshShortcut_iface, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshShortcut_get_FullName(IWshShortcut *iface, BSTR *name)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, name);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_get_Arguments(IWshShortcut *iface, BSTR *Arguments)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, Arguments);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_Arguments(IWshShortcut *iface, BSTR Arguments)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%s): stub\n", This, debugstr_w(Arguments));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_get_Description(IWshShortcut *iface, BSTR *Description)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, Description);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_Description(IWshShortcut *iface, BSTR Description)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(Description));
    return IShellLinkW_SetDescription(This->link, Description);
}

static HRESULT WINAPI WshShortcut_get_Hotkey(IWshShortcut *iface, BSTR *Hotkey)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, Hotkey);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_Hotkey(IWshShortcut *iface, BSTR Hotkey)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%s): stub\n", This, debugstr_w(Hotkey));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_get_IconLocation(IWshShortcut *iface, BSTR *IconPath)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, IconPath);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_IconLocation(IWshShortcut *iface, BSTR IconPath)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%s): stub\n", This, debugstr_w(IconPath));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_RelativePath(IWshShortcut *iface, BSTR rhs)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%s): stub\n", This, debugstr_w(rhs));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_get_TargetPath(IWshShortcut *iface, BSTR *Path)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, Path);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_TargetPath(IWshShortcut *iface, BSTR Path)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(Path));
    return IShellLinkW_SetPath(This->link, Path);
}

static HRESULT WINAPI WshShortcut_get_WindowStyle(IWshShortcut *iface, int *ShowCmd)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, ShowCmd);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_WindowStyle(IWshShortcut *iface, int ShowCmd)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%d): stub\n", This, ShowCmd);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_get_WorkingDirectory(IWshShortcut *iface, BSTR *WorkingDirectory)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%p): stub\n", This, WorkingDirectory);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_put_WorkingDirectory(IWshShortcut *iface, BSTR WorkingDirectory)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    TRACE("(%p)->(%s): stub\n", This, debugstr_w(WorkingDirectory));
    return IShellLinkW_SetWorkingDirectory(This->link, WorkingDirectory);
}

static HRESULT WINAPI WshShortcut_Load(IWshShortcut *iface, BSTR PathLink)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    FIXME("(%p)->(%s): stub\n", This, debugstr_w(PathLink));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShortcut_Save(IWshShortcut *iface)
{
    WshShortcut *This = impl_from_IWshShortcut(iface);
    IPersistFile *file;
    HRESULT hr;

    TRACE("(%p)\n", This);

    IShellLinkW_QueryInterface(This->link, &IID_IPersistFile, (void**)&file);
    hr = IPersistFile_Save(file, This->path_link, TRUE);
    IPersistFile_Release(file);

    return hr;
}

static const IWshShortcutVtbl WshShortcutVtbl = {
    WshShortcut_QueryInterface,
    WshShortcut_AddRef,
    WshShortcut_Release,
    WshShortcut_GetTypeInfoCount,
    WshShortcut_GetTypeInfo,
    WshShortcut_GetIDsOfNames,
    WshShortcut_Invoke,
    WshShortcut_get_FullName,
    WshShortcut_get_Arguments,
    WshShortcut_put_Arguments,
    WshShortcut_get_Description,
    WshShortcut_put_Description,
    WshShortcut_get_Hotkey,
    WshShortcut_put_Hotkey,
    WshShortcut_get_IconLocation,
    WshShortcut_put_IconLocation,
    WshShortcut_put_RelativePath,
    WshShortcut_get_TargetPath,
    WshShortcut_put_TargetPath,
    WshShortcut_get_WindowStyle,
    WshShortcut_put_WindowStyle,
    WshShortcut_get_WorkingDirectory,
    WshShortcut_put_WorkingDirectory,
    WshShortcut_Load,
    WshShortcut_Save
};

static HRESULT WshShortcut_Create(const WCHAR *path, IDispatch **shortcut)
{
    WshShortcut *This;
    HRESULT hr;

    *shortcut = NULL;

    This = HeapAlloc(GetProcessHeap(), 0, sizeof(*This));
    if (!This) return E_OUTOFMEMORY;

    This->IWshShortcut_iface.lpVtbl = &WshShortcutVtbl;
    This->ref = 1;

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
            &IID_IShellLinkW, (void**)&This->link);
    if (FAILED(hr))
    {
        HeapFree(GetProcessHeap(), 0, This);
        return hr;
    }

    This->path_link = SysAllocString(path);
    *shortcut = (IDispatch*)&This->IWshShortcut_iface;

    return S_OK;
}

static HRESULT WINAPI WshShell3_QueryInterface(IWshShell3 *iface, REFIID riid, void **ppv)
{
    TRACE("(%s, %p)\n", debugstr_guid(riid), ppv);

    if(IsEqualGUID(riid, &IID_IUnknown)  ||
       IsEqualGUID(riid, &IID_IDispatch) ||
       IsEqualGUID(riid, &IID_IWshShell3))
    {
        *ppv = iface;
    }else {
        FIXME("Unknown iface %s\n", debugstr_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IWshShell3_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI WshShell3_AddRef(IWshShell3 *iface)
{
    TRACE("()\n");
    return 2;
}

static ULONG WINAPI WshShell3_Release(IWshShell3 *iface)
{
    TRACE("()\n");
    return 2;
}

static HRESULT WINAPI WshShell3_GetTypeInfoCount(IWshShell3 *iface, UINT *pctinfo)
{
    TRACE("(%p)\n", pctinfo);
    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI WshShell3_GetTypeInfo(IWshShell3 *iface, UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    TRACE("(%u %u %p)\n", iTInfo, lcid, ppTInfo);
    return get_typeinfo(IWshShell3_tid, ppTInfo);
}

static HRESULT WINAPI WshShell3_GetIDsOfNames(IWshShell3 *iface, REFIID riid, LPOLESTR *rgszNames,
        UINT cNames, LCID lcid, DISPID *rgDispId)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%s %p %u %u %p)\n", debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hr = get_typeinfo(IWshShell3_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshShell3_Invoke(IWshShell3 *iface, DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%d %s %d %d %p %p %p %p)\n", dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IWshShell3_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &WshShell3, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI WshShell3_get_SpecialFolders(IWshShell3 *iface, IWshCollection **folders)
{
    TRACE("(%p)\n", folders);
    return WshCollection_Create(folders);
}

static HRESULT WINAPI WshShell3_get_Environment(IWshShell3 *iface, VARIANT *Type, IWshEnvironment **out_Env)
{
    FIXME("(%p %p): stub\n", Type, out_Env);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_Run(IWshShell3 *iface, BSTR Command, VARIANT *WindowStyle, VARIANT *WaitOnReturn, int *out_ExitCode)
{
    FIXME("(%s %s %p): stub\n", debugstr_variant(WindowStyle), debugstr_variant(WaitOnReturn), out_ExitCode);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_Popup(IWshShell3 *iface, BSTR Text, VARIANT* SecondsToWait, VARIANT *Title, VARIANT *Type, int *button)
{
    FIXME("(%s %s %s %s %p): stub\n", debugstr_w(Text), debugstr_variant(SecondsToWait),
        debugstr_variant(Title), debugstr_variant(Type), button);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_CreateShortcut(IWshShell3 *iface, BSTR PathLink, IDispatch** Shortcut)
{
    TRACE("(%s %p)\n", debugstr_w(PathLink), Shortcut);
    return WshShortcut_Create(PathLink, Shortcut);
}

static HRESULT WINAPI WshShell3_ExpandEnvironmentStrings(IWshShell3 *iface, BSTR Src, BSTR* out_Dst)
{
    FIXME("(%s %p): stub\n", debugstr_w(Src), out_Dst);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_RegRead(IWshShell3 *iface, BSTR Name, VARIANT* out_Value)
{
    FIXME("(%s %p): stub\n", debugstr_w(Name), out_Value);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_RegWrite(IWshShell3 *iface, BSTR Name, VARIANT *Value, VARIANT *Type)
{
    FIXME("(%s %s %s): stub\n", debugstr_w(Name), debugstr_variant(Value), debugstr_variant(Type));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_RegDelete(IWshShell3 *iface, BSTR Name)
{
    FIXME("(%s): stub\n", debugstr_w(Name));
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_LogEvent(IWshShell3 *iface, VARIANT *Type, BSTR Message, BSTR Target, VARIANT_BOOL *out_Success)
{
    FIXME("(%s %s %s %p): stub\n", debugstr_variant(Type), debugstr_w(Message), debugstr_w(Target), out_Success);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_AppActivate(IWshShell3 *iface, VARIANT *App, VARIANT *Wait, VARIANT_BOOL *out_Success)
{
    FIXME("(%s %s %p): stub\n", debugstr_variant(App), debugstr_variant(Wait), out_Success);
    return E_NOTIMPL;
}

static HRESULT WINAPI WshShell3_SendKeys(IWshShell3 *iface, BSTR Keys, VARIANT *Wait)
{
    FIXME("(%s %p): stub\n", debugstr_w(Keys), Wait);
    return E_NOTIMPL;
}

static const IWshShell3Vtbl WshShell3Vtbl = {
    WshShell3_QueryInterface,
    WshShell3_AddRef,
    WshShell3_Release,
    WshShell3_GetTypeInfoCount,
    WshShell3_GetTypeInfo,
    WshShell3_GetIDsOfNames,
    WshShell3_Invoke,
    WshShell3_get_SpecialFolders,
    WshShell3_get_Environment,
    WshShell3_Run,
    WshShell3_Popup,
    WshShell3_CreateShortcut,
    WshShell3_ExpandEnvironmentStrings,
    WshShell3_RegRead,
    WshShell3_RegWrite,
    WshShell3_RegDelete,
    WshShell3_LogEvent,
    WshShell3_AppActivate,
    WshShell3_SendKeys
};

static IWshShell3 WshShell3 = { &WshShell3Vtbl };

HRESULT WINAPI WshShellFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    TRACE("(%p %s %p)\n", outer, debugstr_guid(riid), ppv);

    return IWshShell3_QueryInterface(&WshShell3, riid, ppv);
}
