/*
 * ExplorerFrame main include
 *
 * Copyright 2010 David Hedberg
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

#ifndef __WINE_EXPLORERFRAME_H
#define __WINE_EXPLORERFRAME_H

#include "shlobj.h"

/* Not declared in commctrl.h ("for internal use (msdn)") */
#define TVS_EX_NOSINGLECOLLAPSE 0x0001

extern HINSTANCE explorerframe_hinstance DECLSPEC_HIDDEN;

extern LONG EFRAME_refCount DECLSPEC_HIDDEN;
static inline void EFRAME_LockModule(void) { InterlockedIncrement( &EFRAME_refCount ); }
static inline void EFRAME_UnlockModule(void) { InterlockedDecrement( &EFRAME_refCount ); }

HRESULT NamespaceTreeControl_Constructor(IUnknown *pUnkOuter, REFIID riid, void **ppv) DECLSPEC_HIDDEN;

#endif /* __WINE_EXPLORERFRAME_H */