/* IDirectMusicContainerImpl
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
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

#include "dmloader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmloader);
WINE_DECLARE_DEBUG_CHANNEL(dmfile);
WINE_DECLARE_DEBUG_CHANNEL(dmdump);

#define DMUS_MAX_CATEGORY_SIZE DMUS_MAX_CATEGORY*sizeof(WCHAR)
#define DMUS_MAX_NAME_SIZE     DMUS_MAX_NAME*sizeof(WCHAR)
#define DMUS_MAX_FILENAME_SIZE DMUS_MAX_FILENAME*sizeof(WCHAR)

static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicContainer_AddRef (LPDIRECTMUSICCONTAINER iface);
static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicObject_AddRef (LPDIRECTMUSICOBJECT iface);
static ULONG WINAPI IDirectMusicContainerImpl_IPersistStream_AddRef (LPPERSISTSTREAM iface);

/*****************************************************************************
 * IDirectMusicContainerImpl implementation
 */
/* IUnknown/IDirectMusicContainer part: */

static HRESULT DMUSIC_DestroyDirectMusicContainerImpl (LPDIRECTMUSICCONTAINER iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ContainerVtbl, iface);
	LPDIRECTMUSICLOADER pLoader;
	LPDIRECTMUSICGETLOADER pGetLoader;
	struct list *pEntry;
	LPWINE_CONTAINER_ENTRY pContainedObject;

	/* get loader (from stream we loaded from) */
	TRACE(": getting loader\n");
	IStream_QueryInterface (This->pStream, &IID_IDirectMusicGetLoader, (LPVOID*)&pGetLoader);
	IDirectMusicGetLoader_GetLoader (pGetLoader, &pLoader);
	IDirectMusicGetLoader_Release (pGetLoader);

	/* release objects from loader's cache (if appropriate) */
	TRACE(": releasing objects from loader's cache\n");
	LIST_FOR_EACH (pEntry, This->pContainedObjects) {
		pContainedObject = LIST_ENTRY (pEntry, WINE_CONTAINER_ENTRY, entry);
		/* my tests indicate that container releases objects *only*
		   if they were loaded at its load-time (makes sense, it doesn't
		   have pointers to objects otherwise); BTW: native container seems
		   to ignore the flags (I won't) */
		if (pContainedObject->pObject && !(pContainedObject->dwFlags & DMUS_CONTAINED_OBJF_KEEP)) {
			/* flags say it shouldn't be kept in loader's cache */
			IDirectMusicLoader_ReleaseObject (pLoader, pContainedObject->pObject);
		}
	}
	IDirectMusicLoader_Release (pLoader);

	/* release stream we loaded from */
	IStream_Release (This->pStream);

	/* FIXME: release allocated entries */
	unlock_module();

	return S_OK;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicContainer_QueryInterface (LPDIRECTMUSICCONTAINER iface, REFIID riid, LPVOID *ppobj) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ContainerVtbl, iface);
	
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);
	if (IsEqualIID (riid, &IID_IUnknown) ||
		IsEqualIID (riid, &IID_IDirectMusicContainer)) {
		*ppobj = &This->ContainerVtbl;
		IDirectMusicContainerImpl_IDirectMusicContainer_AddRef ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl);
		return S_OK;
	} else if (IsEqualIID (riid, &IID_IDirectMusicObject)) {
		*ppobj = &This->ObjectVtbl;
		IDirectMusicContainerImpl_IDirectMusicObject_AddRef ((LPDIRECTMUSICOBJECT)&This->ObjectVtbl);		
		return S_OK;
	} else if (IsEqualIID (riid, &IID_IPersistStream)) {
		*ppobj = &This->PersistStreamVtbl;
		IDirectMusicContainerImpl_IPersistStream_AddRef ((LPPERSISTSTREAM)&This->PersistStreamVtbl);		
		return S_OK;
	}
	
	WARN(": not found\n");
	return E_NOINTERFACE;
}

static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicContainer_AddRef (LPDIRECTMUSICCONTAINER iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ContainerVtbl, iface);
	TRACE("(%p): AddRef from %d\n", This, This->dwRef);
	return InterlockedIncrement (&This->dwRef);
}

static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicContainer_Release (LPDIRECTMUSICCONTAINER iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ContainerVtbl, iface);
	
	DWORD dwRef = InterlockedDecrement (&This->dwRef);
	TRACE("(%p): ReleaseRef to %d\n", This, dwRef);
	if (dwRef == 0) {
		DMUSIC_DestroyDirectMusicContainerImpl (iface);
		HeapFree(GetProcessHeap(), 0, This);
	}
	
	return dwRef;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicContainer_EnumObject (LPDIRECTMUSICCONTAINER iface, REFGUID rguidClass, DWORD dwIndex, LPDMUS_OBJECTDESC pDesc, WCHAR* pwszAlias) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ContainerVtbl, iface);	
	struct list *pEntry;
	LPWINE_CONTAINER_ENTRY pContainedObject;
	DWORD dwCount = 0;

	TRACE("(%p, %s, %d, %p, %p)\n", This, debugstr_dmguid(rguidClass), dwIndex, pDesc, pwszAlias);

	if (!pDesc)
		return E_POINTER;
	if (pDesc->dwSize != sizeof(DMUS_OBJECTDESC)) {
		ERR(": invalid pDesc->dwSize %d\n", pDesc->dwSize);
		return E_INVALIDARG;
	}

	DM_STRUCT_INIT(pDesc);

	LIST_FOR_EACH (pEntry, This->pContainedObjects) {
		pContainedObject = LIST_ENTRY (pEntry, WINE_CONTAINER_ENTRY, entry);
		
		if (IsEqualGUID (rguidClass, &GUID_DirectMusicAllTypes) || IsEqualGUID (rguidClass, &pContainedObject->Desc.guidClass)) {
			if (dwCount == dwIndex) {
				HRESULT result = S_OK;
				if (pwszAlias) {
					lstrcpynW (pwszAlias, pContainedObject->wszAlias, DMUS_MAX_FILENAME);
					if (strlenW (pContainedObject->wszAlias) > DMUS_MAX_FILENAME)
						result = DMUS_S_STRING_TRUNCATED;
				}
				*pDesc = pContainedObject->Desc;
				return result;
			}
			dwCount++;
		}
	}		
	
	TRACE(": not found\n");
	return S_FALSE;
}

static const IDirectMusicContainerVtbl DirectMusicContainer_Container_Vtbl = {
	IDirectMusicContainerImpl_IDirectMusicContainer_QueryInterface,
	IDirectMusicContainerImpl_IDirectMusicContainer_AddRef,
	IDirectMusicContainerImpl_IDirectMusicContainer_Release,
	IDirectMusicContainerImpl_IDirectMusicContainer_EnumObject
};

/* IDirectMusicObject part: */
static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicObject_QueryInterface (LPDIRECTMUSICOBJECT iface, REFIID riid, LPVOID *ppobj) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_QueryInterface ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl, riid, ppobj);
}

static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicObject_AddRef (LPDIRECTMUSICOBJECT iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_AddRef ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl);
}

static ULONG WINAPI IDirectMusicContainerImpl_IDirectMusicObject_Release (LPDIRECTMUSICOBJECT iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_Release ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl);
}

static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicObject_GetDescriptor (LPDIRECTMUSICOBJECT iface, LPDMUS_OBJECTDESC pDesc) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	TRACE("(%p, %p):\n", This, pDesc);
	
	/* check if we can write to whole pDesc */
	if (IsBadReadPtr (pDesc, sizeof(DWORD))) {
		ERR(": pDesc->dwSize bad read pointer\n");
		return E_POINTER;
	}
	if (pDesc->dwSize != sizeof(DMUS_OBJECTDESC)) {
		ERR(": invalid pDesc->dwSize\n");
		return E_INVALIDARG;
	}
	if (IsBadWritePtr (pDesc, sizeof(DMUS_OBJECTDESC))) {
		ERR(": pDesc bad write pointer\n");
		return E_POINTER;
	}

	DM_STRUCT_INIT(pDesc);
	*pDesc = This->Desc;

	return S_OK;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicObject_SetDescriptor (LPDIRECTMUSICOBJECT iface, LPDMUS_OBJECTDESC pDesc) {
	DWORD dwNewFlags = 0;
	DWORD dwFlagDifference;
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	TRACE("(%p, %p):\n", This, pDesc);

	/* check if we can read whole pDesc */
	if (IsBadReadPtr (pDesc, sizeof(DWORD))) {
		ERR(": pDesc->dwSize bad read pointer\n");
		return E_POINTER;
	}
	if (pDesc->dwSize != sizeof(DMUS_OBJECTDESC)) {
		ERR(": invalid pDesc->dwSize\n");
		return E_INVALIDARG;
	}
	if (IsBadReadPtr (pDesc, sizeof(DMUS_OBJECTDESC))) {
		ERR(": pDesc bad read pointer\n");
		return E_POINTER;
	}

	if (pDesc->dwValidData & DMUS_OBJ_OBJECT) {
		This->Desc.guidObject = pDesc->guidObject;
		dwNewFlags |= DMUS_OBJ_OBJECT;
	}
	if (pDesc->dwValidData & DMUS_OBJ_NAME) {
		lstrcpynW (This->Desc.wszName, pDesc->wszName, DMUS_MAX_NAME);
		dwNewFlags |= DMUS_OBJ_NAME;
	}
	if (pDesc->dwValidData & DMUS_OBJ_CATEGORY) {
		lstrcpynW (This->Desc.wszCategory, pDesc->wszCategory, DMUS_MAX_CATEGORY);
		dwNewFlags |= DMUS_OBJ_CATEGORY;
	}
	if (pDesc->dwValidData & (DMUS_OBJ_FILENAME | DMUS_OBJ_FULLPATH)) {
		lstrcpynW (This->Desc.wszFileName, pDesc->wszFileName, DMUS_MAX_FILENAME);
		dwNewFlags |= (pDesc->dwValidData & (DMUS_OBJ_FILENAME | DMUS_OBJ_FULLPATH));
	}
	if (pDesc->dwValidData & DMUS_OBJ_VERSION) {
		This->Desc.vVersion.dwVersionLS = pDesc->vVersion.dwVersionLS;
		This->Desc.vVersion.dwVersionMS = pDesc->vVersion.dwVersionMS;
		dwNewFlags |= DMUS_OBJ_VERSION;
	}
	if (pDesc->dwValidData & DMUS_OBJ_DATE) {
		This->Desc.ftDate.dwHighDateTime = pDesc->ftDate.dwHighDateTime;
		This->Desc.ftDate.dwLowDateTime = pDesc->ftDate.dwLowDateTime;
		dwNewFlags |= DMUS_OBJ_DATE;
	}
	/* set new flags */
	This->Desc.dwValidData |= dwNewFlags;
	
	dwFlagDifference = pDesc->dwValidData - dwNewFlags;
	if (dwFlagDifference) {
		pDesc->dwValidData &= ~dwFlagDifference; /* and with bitwise complement */
		return S_FALSE;
	} else return S_OK;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IDirectMusicObject_ParseDescriptor (LPDIRECTMUSICOBJECT iface, LPSTREAM pStream, LPDMUS_OBJECTDESC pDesc) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, ObjectVtbl, iface);
	WINE_CHUNK Chunk;
	DWORD StreamSize, StreamCount, ListSize[1], ListCount[1];
	LARGE_INTEGER liMove; /* used when skipping chunks */

	TRACE("(%p, %p, %p)\n", This, pStream, pDesc);
	
	/* check whether arguments are OK */
	if (IsBadReadPtr (pStream, sizeof(LPVOID))) {
		ERR(": pStream bad read pointer\n");
		return E_POINTER;
	}
	/* check whether pDesc is OK */
	if (IsBadReadPtr (pDesc, sizeof(DWORD))) {
		ERR(": pDesc->dwSize bad read pointer\n");
		return E_POINTER;
	}
	if (pDesc->dwSize != sizeof(DMUS_OBJECTDESC)) {
		ERR(": invalid pDesc->dwSize\n");
		return E_INVALIDARG;
	}
	if (IsBadWritePtr (pDesc, sizeof(DMUS_OBJECTDESC))) {
		ERR(": pDesc bad write pointer\n");
		return E_POINTER;
	}

	DM_STRUCT_INIT(pDesc);
	
	/* here we go... */
	IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
	TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
	switch (Chunk.fccID) {	
		case FOURCC_RIFF: {
			IStream_Read (pStream, &Chunk.fccID, sizeof(FOURCC), NULL);				
			TRACE_(dmfile)(": RIFF chunk of type %s", debugstr_fourcc(Chunk.fccID));
			StreamSize = Chunk.dwSize - sizeof(FOURCC);
			StreamCount = 0;
			if (Chunk.fccID == DMUS_FOURCC_CONTAINER_FORM) {
				TRACE_(dmfile)(": container form\n");
				/* set guidClass */
				pDesc->dwValidData |= DMUS_OBJ_CLASS;
				pDesc->guidClass = CLSID_DirectMusicContainer;
				do {
					IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
					StreamCount += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
					TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
					switch (Chunk.fccID) {
						case DMUS_FOURCC_GUID_CHUNK: {
							TRACE_(dmfile)(": GUID chunk\n");
							pDesc->dwValidData |= DMUS_OBJ_OBJECT;
							IStream_Read (pStream, &pDesc->guidObject, Chunk.dwSize, NULL);
							TRACE_(dmdump)(": GUID: %s\n", debugstr_guid(&pDesc->guidObject));
							break;
						}
						case DMUS_FOURCC_VERSION_CHUNK: {
							TRACE_(dmfile)(": version chunk\n");
							pDesc->dwValidData |= DMUS_OBJ_VERSION;
							IStream_Read (pStream, &pDesc->vVersion, Chunk.dwSize, NULL);
							TRACE_(dmdump)(": version: %s\n", debugstr_dmversion(&pDesc->vVersion));
							break;
						}
						case DMUS_FOURCC_DATE_CHUNK: {
							TRACE_(dmfile)(": date chunk\n");
							IStream_Read (pStream, &pDesc->ftDate, Chunk.dwSize, NULL);
							pDesc->dwValidData |= DMUS_OBJ_DATE;
							TRACE_(dmdump)(": date: %s\n", debugstr_filetime(&pDesc->ftDate));
							break;
						}								
						case DMUS_FOURCC_CATEGORY_CHUNK: {
							TRACE_(dmfile)(": category chunk\n");
							/* if it happens that string is too long,
							   read what we can and skip the rest*/
							if (Chunk.dwSize > DMUS_MAX_CATEGORY_SIZE) {
								IStream_Read (pStream, pDesc->wszCategory, DMUS_MAX_CATEGORY_SIZE, NULL);
								liMove.QuadPart = Chunk.dwSize - DMUS_MAX_CATEGORY_SIZE;
								IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
							} else {
								IStream_Read (pStream, pDesc->wszCategory, Chunk.dwSize, NULL);
							}
							pDesc->dwValidData |= DMUS_OBJ_CATEGORY;
							TRACE_(dmdump)(": category: %s\n", debugstr_w(pDesc->wszCategory));							
							break;
						}
						case FOURCC_LIST: {
							IStream_Read (pStream, &Chunk.fccID, sizeof(FOURCC), NULL);				
							TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(Chunk.fccID));
							ListSize[0] = Chunk.dwSize - sizeof(FOURCC);
							ListCount[0] = 0;
							switch (Chunk.fccID) {
								/* evil M$ UNFO list, which can (!?) contain INFO elements */
								case DMUS_FOURCC_UNFO_LIST: {
									TRACE_(dmfile)(": UNFO list\n");
									do {
										IStream_Read (pStream, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
										ListCount[0] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
										TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
										switch (Chunk.fccID) {
											/* don't ask me why, but M$ puts INFO elements in UNFO list sometimes
                                             (though strings seem to be valid unicode) */
											case mmioFOURCC('I','N','A','M'):
											case DMUS_FOURCC_UNAM_CHUNK: {
												TRACE_(dmfile)(": name chunk\n");
												/* if it happens that string is too long,
													   read what we can and skip the rest*/
												if (Chunk.dwSize > DMUS_MAX_NAME_SIZE) {
													IStream_Read (pStream, pDesc->wszName, DMUS_MAX_NAME_SIZE, NULL);
													liMove.QuadPart = Chunk.dwSize - DMUS_MAX_NAME_SIZE;
													IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												} else {
													IStream_Read (pStream, pDesc->wszName, Chunk.dwSize, NULL);
												}
												pDesc->dwValidData |= DMUS_OBJ_NAME;
												TRACE_(dmdump)(": name: %s\n", debugstr_w(pDesc->wszName));
												break;
											}
											default: {
												TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
												liMove.QuadPart = Chunk.dwSize;
												IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
												break;						
											}
										}
										TRACE_(dmfile)(": ListCount[0] = 0x%08X < ListSize[0] = 0x%08X\n", ListCount[0], ListSize[0]);
									} while (ListCount[0] < ListSize[0]);
									break;
								}
								default: {
									TRACE_(dmfile)(": unknown (skipping)\n");
									liMove.QuadPart = Chunk.dwSize - sizeof(FOURCC);
									IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
									break;						
								}
							}
							break;
						}	
						default: {
							TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
							liMove.QuadPart = Chunk.dwSize;
							IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL);
							break;						
						}
					}
					TRACE_(dmfile)(": StreamCount[0] = 0x%08X < StreamSize[0] = 0x%08X\n", StreamCount, StreamSize);
				} while (StreamCount < StreamSize);
			} else {
				TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
				liMove.QuadPart = StreamSize;
				IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
				return E_FAIL;
			}
		
			TRACE_(dmfile)(": reading finished\n");
			break;
		}
		default: {
			TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
			liMove.QuadPart = Chunk.dwSize;
			IStream_Seek (pStream, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
			return DMUS_E_INVALIDFILE;
		}
	}	
	
	TRACE(": returning descriptor:\n%s\n", debugstr_DMUS_OBJECTDESC(pDesc));
	return S_OK;	
}

static const IDirectMusicObjectVtbl DirectMusicContainer_Object_Vtbl = {
	IDirectMusicContainerImpl_IDirectMusicObject_QueryInterface,
	IDirectMusicContainerImpl_IDirectMusicObject_AddRef,
	IDirectMusicContainerImpl_IDirectMusicObject_Release,
	IDirectMusicContainerImpl_IDirectMusicObject_GetDescriptor,
	IDirectMusicContainerImpl_IDirectMusicObject_SetDescriptor,
	IDirectMusicContainerImpl_IDirectMusicObject_ParseDescriptor
};

/* IPersistStream part: */
static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_QueryInterface (LPPERSISTSTREAM iface, REFIID riid, LPVOID *ppobj) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, PersistStreamVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_QueryInterface ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl, riid, ppobj);
}

static ULONG WINAPI IDirectMusicContainerImpl_IPersistStream_AddRef (LPPERSISTSTREAM iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, PersistStreamVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_AddRef ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl);
}

static ULONG WINAPI IDirectMusicContainerImpl_IPersistStream_Release (LPPERSISTSTREAM iface) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, PersistStreamVtbl, iface);
	return IDirectMusicContainerImpl_IDirectMusicContainer_Release ((LPDIRECTMUSICCONTAINER)&This->ContainerVtbl);
}

static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_GetClassID (LPPERSISTSTREAM iface, CLSID* pClassID) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, PersistStreamVtbl, iface);
	
	TRACE("(%p, %p)\n", This, pClassID);
	if (IsBadWritePtr (pClassID, sizeof(CLSID))) {
		ERR(": pClassID bad write pointer\n");
		return E_POINTER;
	}
	
	*pClassID = CLSID_DirectMusicContainer;
	return S_OK;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_IsDirty (LPPERSISTSTREAM iface) {
	/* FIXME: is implemented (somehow) */
	return E_NOTIMPL;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_Load (LPPERSISTSTREAM iface, IStream* pStm) {
	ICOM_THIS_MULTI(IDirectMusicContainerImpl, PersistStreamVtbl, iface);
	WINE_CHUNK Chunk;
	DWORD StreamSize, StreamCount, ListSize[3], ListCount[3];
	LARGE_INTEGER liMove; /* used when skipping chunks */
	ULARGE_INTEGER uliPos; /* needed when dealing with RIFF chunks */
	LPDIRECTMUSICGETLOADER pGetLoader;
	LPDIRECTMUSICLOADER pLoader;
	HRESULT result = S_OK;

	TRACE("(%p, %p):\n", This, pStm);
	
	/* check whether pStm is valid read pointer */
	if (IsBadReadPtr (pStm, sizeof(LPVOID))) {
		ERR(": pStm bad read pointer\n");
		return E_POINTER;
	}
	/* if stream is already set, this means the container is already loaded */
	if (This->pStream) {
		TRACE(": stream is already set, which means container is already loaded\n");
		return DMUS_E_ALREADY_LOADED;
	}

	/* get loader since it will be needed later */
	if (FAILED(IStream_QueryInterface (pStm, &IID_IDirectMusicGetLoader, (LPVOID*)&pGetLoader))) {
		ERR(": stream not supported\n");
		return DMUS_E_UNSUPPORTED_STREAM;
	}
	IDirectMusicGetLoader_GetLoader (pGetLoader, &pLoader);
	IDirectMusicGetLoader_Release (pGetLoader);
	
	This->pStream = pStm;
	IStream_AddRef (pStm); /* add count for later references */
	
	/* start with load */
	IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
	TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
	switch (Chunk.fccID) {	
		case FOURCC_RIFF: {
			IStream_Read (pStm, &Chunk.fccID, sizeof(FOURCC), NULL);				
			TRACE_(dmfile)(": RIFF chunk of type %s", debugstr_fourcc(Chunk.fccID));
			StreamSize = Chunk.dwSize - sizeof(FOURCC);
			StreamCount = 0;
			switch (Chunk.fccID) {
				case DMUS_FOURCC_CONTAINER_FORM: {
					TRACE_(dmfile)(": container form\n");
					This->Desc.guidClass = CLSID_DirectMusicContainer;
					This->Desc.dwValidData |= DMUS_OBJ_CLASS;
					do {
						IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
						StreamCount += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
						TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
						switch (Chunk.fccID) {
							case DMUS_FOURCC_CONTAINER_CHUNK: {
								TRACE_(dmfile)(": container header chunk\n");
								IStream_Read (pStm, &This->Header, Chunk.dwSize, NULL);
								TRACE_(dmdump)(": container header chunk:\n%s\n", debugstr_DMUS_IO_CONTAINER_HEADER(&This->Header));
								break;	
							}
							case DMUS_FOURCC_GUID_CHUNK: {
								TRACE_(dmfile)(": GUID chunk\n");
								IStream_Read (pStm, &This->Desc.guidObject, Chunk.dwSize, NULL);
								This->Desc.dwValidData |= DMUS_OBJ_OBJECT;
								TRACE_(dmdump)(": GUID: %s\n", debugstr_guid(&This->Desc.guidObject));
								break;
							}
							case DMUS_FOURCC_VERSION_CHUNK: {
								TRACE_(dmfile)(": version chunk\n");
								IStream_Read (pStm, &This->Desc.vVersion, Chunk.dwSize, NULL);
								This->Desc.dwValidData |= DMUS_OBJ_VERSION;
								TRACE_(dmdump)(": version: %s\n", debugstr_dmversion(&This->Desc.vVersion));
								break;
							}
							case DMUS_FOURCC_DATE_CHUNK: {
								TRACE_(dmfile)(": date chunk\n");
								IStream_Read (pStm, &This->Desc.ftDate, Chunk.dwSize, NULL);
								This->Desc.dwValidData |= DMUS_OBJ_DATE;
								TRACE_(dmdump)(": date: %s\n", debugstr_filetime(&This->Desc.ftDate));
								break;
							}							
							case DMUS_FOURCC_CATEGORY_CHUNK: {
								TRACE_(dmfile)(": category chunk\n");
								/* if it happens that string is too long,
								   read what we can and skip the rest*/
								if (Chunk.dwSize > DMUS_MAX_CATEGORY_SIZE) {
									IStream_Read (pStm, This->Desc.wszCategory, DMUS_MAX_CATEGORY_SIZE, NULL);
									liMove.QuadPart = Chunk.dwSize - DMUS_MAX_CATEGORY_SIZE;
									IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
								} else {
									IStream_Read (pStm, This->Desc.wszCategory, Chunk.dwSize, NULL);
								}
								This->Desc.dwValidData |= DMUS_OBJ_CATEGORY;
								TRACE_(dmdump)(": category: %s\n", debugstr_w(This->Desc.wszCategory));								
								break;
							}
							case FOURCC_LIST: {
								IStream_Read (pStm, &Chunk.fccID, sizeof(FOURCC), NULL);				
								TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(Chunk.fccID));
								ListSize[0] = Chunk.dwSize - sizeof(FOURCC);
								ListCount[0] = 0;
								switch (Chunk.fccID) {
									case DMUS_FOURCC_UNFO_LIST: {
										TRACE_(dmfile)(": UNFO list\n");
										do {
											IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
											ListCount[0] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
											TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
											switch (Chunk.fccID) {
												/* don't ask me why, but M$ puts INFO elements in UNFO list sometimes
                                              (though strings seem to be valid unicode) */
												case mmioFOURCC('I','N','A','M'):
												case DMUS_FOURCC_UNAM_CHUNK: {
													TRACE_(dmfile)(": name chunk\n");
													/* if it happens that string is too long,
													   read what we can and skip the rest*/
													if (Chunk.dwSize > DMUS_MAX_NAME_SIZE) {
														IStream_Read (pStm, This->Desc.wszName, DMUS_MAX_NAME_SIZE, NULL);
														liMove.QuadPart = Chunk.dwSize - DMUS_MAX_NAME_SIZE;
														IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													} else {
														IStream_Read (pStm, This->Desc.wszName, Chunk.dwSize, NULL);
													}
													This->Desc.dwValidData |= DMUS_OBJ_NAME;
													TRACE_(dmdump)(": name: %s\n", debugstr_w(This->Desc.wszName));
													break;
												}
												default: {
													TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
													liMove.QuadPart = Chunk.dwSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;						
												}
											}
											TRACE_(dmfile)(": ListCount[0] = 0x%08X < ListSize[0] = 0x%08X\n", ListCount[0], ListSize[0]);
										} while (ListCount[0] < ListSize[0]);
										break;
									}
									case DMUS_FOURCC_CONTAINED_OBJECTS_LIST: {
										TRACE_(dmfile)(": contained objects list\n");
										do {
											IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
											ListCount[0] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
											TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
											switch (Chunk.fccID) {
												case FOURCC_LIST: {
													IStream_Read (pStm, &Chunk.fccID, sizeof(FOURCC), NULL);				
													TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(Chunk.fccID));
													ListSize[1] = Chunk.dwSize - sizeof(FOURCC);
													ListCount[1] = 0;
													switch (Chunk.fccID) {
														case DMUS_FOURCC_CONTAINED_OBJECT_LIST: {
															LPWINE_CONTAINER_ENTRY pNewEntry;
															TRACE_(dmfile)(": contained object list\n");
															pNewEntry = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, sizeof(WINE_CONTAINER_ENTRY));
															DM_STRUCT_INIT(&pNewEntry->Desc);
															do {
																IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
																ListCount[1] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
																TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
																switch (Chunk.fccID) {
																	case DMUS_FOURCC_CONTAINED_ALIAS_CHUNK: {
																		TRACE_(dmfile)(": alias chunk\n");
																		pNewEntry->wszAlias = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, Chunk.dwSize);
																		IStream_Read (pStm, pNewEntry->wszAlias, Chunk.dwSize, NULL);
																		TRACE_(dmdump)(": alias: %s\n", debugstr_w(pNewEntry->wszAlias));
																		break;
																	}
																	case DMUS_FOURCC_CONTAINED_OBJECT_CHUNK: {
																		DMUS_IO_CONTAINED_OBJECT_HEADER tmpObjectHeader;
																		TRACE_(dmfile)(": contained object header chunk\n");
																		IStream_Read (pStm, &tmpObjectHeader, Chunk.dwSize, NULL);
																		TRACE_(dmdump)(": contained object header:\n%s\n", debugstr_DMUS_IO_CONTAINED_OBJECT_HEADER(&tmpObjectHeader));
																		/* copy guidClass */
																		pNewEntry->Desc.dwValidData |= DMUS_OBJ_CLASS;
																		pNewEntry->Desc.guidClass = tmpObjectHeader.guidClassID;
																		/* store flags */
																		pNewEntry->dwFlags = tmpObjectHeader.dwFlags;
																		break;
																	}
																	/* now read data... it may be safe to read everything after object header chunk, 
																		but I'm not comfortable with MSDN's "the header is *normally* followed by ..." */
																	case FOURCC_LIST: {
																		IStream_Read (pStm, &Chunk.fccID, sizeof(FOURCC), NULL);				
																		TRACE_(dmfile)(": LIST chunk of type %s", debugstr_fourcc(Chunk.fccID));
																		ListSize[2] = Chunk.dwSize - sizeof(FOURCC);
																		ListCount[2] = 0;
																		switch (Chunk.fccID) {
																			case DMUS_FOURCC_REF_LIST: {
																				TRACE_(dmfile)(": reference list\n");
																				pNewEntry->bIsRIFF = 0;
																				do {
																					IStream_Read (pStm, &Chunk, sizeof(FOURCC)+sizeof(DWORD), NULL);
																					ListCount[2] += sizeof(FOURCC) + sizeof(DWORD) + Chunk.dwSize;
																					TRACE_(dmfile)(": %s chunk (size = 0x%08X)", debugstr_fourcc (Chunk.fccID), Chunk.dwSize);
																					switch (Chunk.fccID) {
																						case DMUS_FOURCC_REF_CHUNK: {
																							DMUS_IO_REFERENCE tmpReferenceHeader; /* temporary structure */
																							TRACE_(dmfile)(": reference header chunk\n");
																							memset (&tmpReferenceHeader, 0, sizeof(DMUS_IO_REFERENCE));
																							IStream_Read (pStm, &tmpReferenceHeader, Chunk.dwSize, NULL);
																							/* copy retrieved data to DMUS_OBJECTDESC */
																							if (!IsEqualCLSID (&pNewEntry->Desc.guidClass, &tmpReferenceHeader.guidClassID)) ERR(": object header declares different CLSID than reference header?\n");
																							/* it shouldn't be necessary to copy guidClass, since it was set in contained object header already...
																							   yet if they happen to be different, I'd rather stick to this one */
																							pNewEntry->Desc.guidClass = tmpReferenceHeader.guidClassID;
																							pNewEntry->Desc.dwValidData |= tmpReferenceHeader.dwValidData;
																							break;																	
																						}
																						case DMUS_FOURCC_GUID_CHUNK: {
																							TRACE_(dmfile)(": guid chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, &pNewEntry->Desc.guidObject, Chunk.dwSize, NULL);
																							break;
																						}
																						case DMUS_FOURCC_DATE_CHUNK: {
																							TRACE_(dmfile)(": file date chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, &pNewEntry->Desc.ftDate, Chunk.dwSize, NULL);
																							break;
																						}
																						case DMUS_FOURCC_NAME_CHUNK: {
																							TRACE_(dmfile)(": name chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, pNewEntry->Desc.wszName, Chunk.dwSize, NULL);
																							break;
																						}
																						case DMUS_FOURCC_FILE_CHUNK: {
																							TRACE_(dmfile)(": file name chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, pNewEntry->Desc.wszFileName, Chunk.dwSize, NULL);
																							break;
																						}
																						case DMUS_FOURCC_CATEGORY_CHUNK: {
																							TRACE_(dmfile)(": category chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, pNewEntry->Desc.wszCategory, Chunk.dwSize, NULL);
																							break;
																						}
																						case DMUS_FOURCC_VERSION_CHUNK: {
																							TRACE_(dmfile)(": version chunk\n");
																							/* no need to set flags since they were copied from reference header */
																							IStream_Read (pStm, &pNewEntry->Desc.vVersion, Chunk.dwSize, NULL);
																							break;
																						}
																						default: {
																							TRACE_(dmfile)(": unknown chunk (skipping)\n");
																							liMove.QuadPart = Chunk.dwSize;
																							IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip this chunk */
																							break;
																						}
																					}
																					TRACE_(dmfile)(": ListCount[2] = 0x%08X < ListSize[2] = 0x%08X\n", ListCount[2], ListSize[2]);
																				} while (ListCount[2] < ListSize[2]);
																				break;
																			}
																			default: {
																				TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
																				return E_FAIL;
																			}
																		}
																		break;
																	}
																	
																	case FOURCC_RIFF: {
																		IStream_Read (pStm, &Chunk.fccID, sizeof(FOURCC), NULL);
																		TRACE_(dmfile)(": RIFF chunk of type %s", debugstr_fourcc(Chunk.fccID));
																		if (IS_VALID_DMFORM (Chunk.fccID)) {
																			TRACE_(dmfile)(": valid DMUSIC form\n");
																			pNewEntry->bIsRIFF = 1;
																			/* we'll have to skip whole RIFF chunk after SetObject is called */
																			liMove.QuadPart = 0;
																			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, &uliPos);
																			uliPos.QuadPart += (Chunk.dwSize - sizeof(FOURCC)); /* set uliPos at the end of RIFF chunk */																			
																			/* move at the beginning of RIFF chunk */
																			liMove.QuadPart = 0;
																			liMove.QuadPart -= (sizeof(FOURCC)+sizeof(DWORD)+sizeof(FOURCC));
																			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
																			/* put pointer to stream in descriptor */
																			pNewEntry->Desc.dwValidData |= DMUS_OBJ_STREAM;
																			pNewEntry->Desc.pStream = pStm; /* we don't have to worry about cloning, since SetObject will perform it */
																			/* wait till we get on the end of object list */
																		} else {
																			TRACE_(dmfile)(": invalid DMUSIC form (skipping)\n");
																			liMove.QuadPart = Chunk.dwSize - sizeof(FOURCC);
																			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
																			/* FIXME: should we return E_FAIL? */
																		}
																		break;
																	}
																	default: {
																		TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
																		liMove.QuadPart = Chunk.dwSize;
																		IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
																		break;						
																	}
																}
																TRACE_(dmfile)(": ListCount[1] = 0x%08X < ListSize[1] = 0x%08X\n", ListCount[1], ListSize[1]);
															} while (ListCount[1] < ListSize[1]);
															/* SetObject: this will fill descriptor with additional info	and add alias in loader's cache */
															IDirectMusicLoader_SetObject (pLoader, &pNewEntry->Desc);
															/* now that SetObject collected appropriate info into descriptor we can live happily ever after;
															   or not, since we have to clean evidence of loading through stream... *sigh*
															   and we have to skip rest of the chunk, if we loaded through RIFF */
															if (pNewEntry->bIsRIFF) {
																liMove.QuadPart = uliPos.QuadPart;
																IStream_Seek (pStm, liMove, STREAM_SEEK_SET, NULL);
																pNewEntry->Desc.dwValidData &= ~DMUS_OBJ_STREAM; /* clear flag (and with bitwise complement) */
																pNewEntry->Desc.pStream = NULL;											
															}
															/* add entry to list of objects */
															list_add_tail (This->pContainedObjects, &pNewEntry->entry);
															break;
														}
														default: {
															TRACE_(dmfile)(": unknown (skipping)\n");
															liMove.QuadPart = Chunk.dwSize - sizeof(FOURCC);
															IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
															break;						
														}
													}
													break;
												}
												default: {
													TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
													liMove.QuadPart = Chunk.dwSize;
													IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
													break;						
												}
											}
											TRACE_(dmfile)(": ListCount[0] = 0x%08X < ListSize[0] = 0x%08X\n", ListCount[0], ListSize[0]);
										} while (ListCount[0] < ListSize[0]);
										break;
									}									
									default: {
										TRACE_(dmfile)(": unknown (skipping)\n");
										liMove.QuadPart = Chunk.dwSize - sizeof(FOURCC);
										IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
										break;						
									}
								}
								break;
							}	
							default: {
								TRACE_(dmfile)(": unknown chunk (irrelevant & skipping)\n");
								liMove.QuadPart = Chunk.dwSize;
								IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL);
								break;						
							}
						}
						TRACE_(dmfile)(": StreamCount[0] = 0x%08X < StreamSize[0] = 0x%08X\n", StreamCount, StreamSize);
					} while (StreamCount < StreamSize);
					break;
				}
				default: {
					TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
					liMove.QuadPart = StreamSize;
					IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
					return E_FAIL;
				}
			}
			TRACE_(dmfile)(": reading finished\n");
			This->Desc.dwValidData |= DMUS_OBJ_LOADED;
			break;
		}
		default: {
			TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
			liMove.QuadPart = Chunk.dwSize;
			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
			return E_FAIL;
		}
	}
	
	/* now, if DMUS_CONTAINER_NOLOADS is not set, we are supposed to load contained objects;
	   so when we call GetObject later, they'll already be in cache */
	if (!(This->Header.dwFlags & DMUS_CONTAINER_NOLOADS)) {
		struct list *pEntry;
		LPWINE_CONTAINER_ENTRY pContainedObject;

		TRACE(": DMUS_CONTAINER_NOLOADS not set... load all objects\n");
		
		LIST_FOR_EACH (pEntry, This->pContainedObjects) {
			IDirectMusicObject* pObject;
			pContainedObject = LIST_ENTRY (pEntry, WINE_CONTAINER_ENTRY, entry);		
			/* get object from loader and then release it */
			if (SUCCEEDED(IDirectMusicLoader_GetObject (pLoader, &pContainedObject->Desc, &IID_IDirectMusicObject, (LPVOID*)&pObject))) {
				pContainedObject->pObject = pObject; /* for final release */
				IDirectMusicObject_Release (pObject); /* we don't really need this one */
			} else {
				WARN(": failed to load contained object\n");
				result = DMUS_S_PARTIALLOAD;
			}
		}
	}
	
	IDirectMusicLoader_Release (pLoader); /* release loader */

#if 0
	/* DEBUG: dumps whole container object tree: */
	if (TRACE_ON(dmloader)) {
		int r = 0;
		LPWINE_CONTAINER_ENTRY tmpEntry;
		struct list *listEntry;

		TRACE("*** IDirectMusicContainer (%p) ***\n", This->ContainerVtbl);
		TRACE(" - Objects:\n");
		LIST_FOR_EACH (listEntry, This->pContainedObjects) {
			tmpEntry = LIST_ENTRY( listEntry, WINE_CONTAINER_ENTRY, entry );
			TRACE("    - Object[%i]:\n", r);
			TRACE("       - wszAlias: %s\n", debugstr_w(tmpEntry->wszAlias));
			TRACE("       - Object descriptor:\n%s\n", debugstr_DMUS_OBJECTDESC(&tmpEntry->Desc));
			r++;
		}
	}
#endif

	return result;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_Save (LPPERSISTSTREAM iface, IStream* pStm, BOOL fClearDirty) {
	ERR(": should not be needed\n");
	return E_NOTIMPL;
}

static HRESULT WINAPI IDirectMusicContainerImpl_IPersistStream_GetSizeMax (LPPERSISTSTREAM iface, ULARGE_INTEGER* pcbSize) {
	ERR(": should not be needed\n");
	return E_NOTIMPL;
}

static const IPersistStreamVtbl DirectMusicContainer_PersistStream_Vtbl = {
	IDirectMusicContainerImpl_IPersistStream_QueryInterface,
	IDirectMusicContainerImpl_IPersistStream_AddRef,
	IDirectMusicContainerImpl_IPersistStream_Release,
	IDirectMusicContainerImpl_IPersistStream_GetClassID,
	IDirectMusicContainerImpl_IPersistStream_IsDirty,
	IDirectMusicContainerImpl_IPersistStream_Load,
	IDirectMusicContainerImpl_IPersistStream_Save,
	IDirectMusicContainerImpl_IPersistStream_GetSizeMax
};

/* for ClassFactory */
HRESULT WINAPI DMUSIC_CreateDirectMusicContainerImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicContainerImpl* obj;

	obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicContainerImpl));
	if (NULL == obj) {
		*ppobj = NULL;
		return E_OUTOFMEMORY;
	}
	obj->ContainerVtbl = &DirectMusicContainer_Container_Vtbl;
	obj->ObjectVtbl = &DirectMusicContainer_Object_Vtbl;
	obj->PersistStreamVtbl = &DirectMusicContainer_PersistStream_Vtbl;
	obj->dwRef = 0; /* will be inited by QueryInterface */
	obj->pContainedObjects = HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, sizeof(struct list));
	list_init (obj->pContainedObjects);

	lock_module();

	return IDirectMusicContainerImpl_IDirectMusicContainer_QueryInterface ((LPDIRECTMUSICCONTAINER)&obj->ContainerVtbl, lpcGUID, ppobj);
}
