/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
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
 *
 */

#include "config.h"
#include "wine/port.h"

#include "d3d10core_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d10core);

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE d3d10_buffer_QueryInterface(ID3D10Buffer *iface, REFIID riid, void **object)
{
    TRACE("iface %p, riid %s, object %p\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, &IID_ID3D10Buffer)
            || IsEqualGUID(riid, &IID_ID3D10Resource)
            || IsEqualGUID(riid, &IID_ID3D10DeviceChild)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        IUnknown_AddRef(iface);
        *object = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE\n", debugstr_guid(riid));

    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d3d10_buffer_AddRef(ID3D10Buffer *iface)
{
    struct d3d10_buffer *This = (struct d3d10_buffer *)iface;
    ULONG refcount = InterlockedIncrement(&This->refcount);

    TRACE("%p increasing refcount to %u\n", This, refcount);

    if (refcount == 1)
        wined3d_buffer_incref(This->wined3d_buffer);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d3d10_buffer_Release(ID3D10Buffer *iface)
{
    struct d3d10_buffer *This = (struct d3d10_buffer *)iface;
    ULONG refcount = InterlockedDecrement(&This->refcount);

    TRACE("%p decreasing refcount to %u\n", This, refcount);

    if (!refcount)
    {
        wined3d_buffer_decref(This->wined3d_buffer);
    }

    return refcount;
}

/* ID3D10DeviceChild methods */

static void STDMETHODCALLTYPE d3d10_buffer_GetDevice(ID3D10Buffer *iface, ID3D10Device **device)
{
    FIXME("iface %p, device %p stub!\n", iface, device);
}

static HRESULT STDMETHODCALLTYPE d3d10_buffer_GetPrivateData(ID3D10Buffer *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    FIXME("iface %p, guid %s, data_size %p, data %p stub!\n",
            iface, debugstr_guid(guid), data_size, data);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d10_buffer_SetPrivateData(ID3D10Buffer *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    FIXME("iface %p, guid %s, data_size %u, data %p stub!\n",
            iface, debugstr_guid(guid), data_size, data);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d3d10_buffer_SetPrivateDataInterface(ID3D10Buffer *iface,
        REFGUID guid, const IUnknown *data)
{
    FIXME("iface %p, guid %s, data %p stub!\n", iface, debugstr_guid(guid), data);

    return E_NOTIMPL;
}

/* ID3D10Resource methods */

static void STDMETHODCALLTYPE d3d10_buffer_GetType(ID3D10Buffer *iface, D3D10_RESOURCE_DIMENSION *resource_dimension)
{
    TRACE("iface %p, resource_dimension %p\n", iface, resource_dimension);

    *resource_dimension = D3D10_RESOURCE_DIMENSION_BUFFER;
}

static void STDMETHODCALLTYPE d3d10_buffer_SetEvictionPriority(ID3D10Buffer *iface, UINT eviction_priority)
{
    FIXME("iface %p, eviction_priority %u stub!\n", iface, eviction_priority);
}

static UINT STDMETHODCALLTYPE d3d10_buffer_GetEvictionPriority(ID3D10Buffer *iface)
{
    FIXME("iface %p stub!\n", iface);

    return 0;
}

/* ID3D10Buffer methods */

static HRESULT STDMETHODCALLTYPE d3d10_buffer_Map(ID3D10Buffer *iface, D3D10_MAP map_type, UINT map_flags, void **data)
{
    struct d3d10_buffer *buffer = (struct d3d10_buffer *)iface;

    TRACE("iface %p, map_type %u, map_flags %#x, data %p.\n", iface, map_type, map_flags, data);

    if (map_type != D3D10_MAP_READ_WRITE)
        FIXME("Ignoring map_type %#x.\n", map_type);
    if (map_flags)
        FIXME("Ignoring map_flags %#x.\n", map_flags);

    return wined3d_buffer_map(buffer->wined3d_buffer, 0, 0, (BYTE **)data, 0);
}

static void STDMETHODCALLTYPE d3d10_buffer_Unmap(ID3D10Buffer *iface)
{
    TRACE("iface %p.\n", iface);

    wined3d_buffer_unmap(((struct d3d10_buffer *)iface)->wined3d_buffer);
}

static void STDMETHODCALLTYPE d3d10_buffer_GetDesc(ID3D10Buffer *iface, D3D10_BUFFER_DESC *desc)
{
    FIXME("iface %p, desc %p stub!\n", iface, desc);
}

static const struct ID3D10BufferVtbl d3d10_buffer_vtbl =
{
    /* IUnknown methods */
    d3d10_buffer_QueryInterface,
    d3d10_buffer_AddRef,
    d3d10_buffer_Release,
    /* ID3D10DeviceChild methods */
    d3d10_buffer_GetDevice,
    d3d10_buffer_GetPrivateData,
    d3d10_buffer_SetPrivateData,
    d3d10_buffer_SetPrivateDataInterface,
    /* ID3D10Resource methods */
    d3d10_buffer_GetType,
    d3d10_buffer_SetEvictionPriority,
    d3d10_buffer_GetEvictionPriority,
    /* ID3D10Buffer methods */
    d3d10_buffer_Map,
    d3d10_buffer_Unmap,
    d3d10_buffer_GetDesc,
};

static void STDMETHODCALLTYPE d3d10_buffer_wined3d_object_released(void *parent)
{
    HeapFree(GetProcessHeap(), 0, parent);
}

static const struct wined3d_parent_ops d3d10_buffer_wined3d_parent_ops =
{
    d3d10_buffer_wined3d_object_released,
};

HRESULT d3d10_buffer_init(struct d3d10_buffer *buffer, struct d3d10_device *device,
        const D3D10_BUFFER_DESC *desc, const D3D10_SUBRESOURCE_DATA *data)
{
    struct wined3d_buffer_desc wined3d_desc;
    HRESULT hr;

    buffer->vtbl = &d3d10_buffer_vtbl;
    buffer->refcount = 1;

    FIXME("Implement DXGI<->wined3d usage conversion\n");

    wined3d_desc.byte_width = desc->ByteWidth;
    wined3d_desc.usage = desc->Usage;
    wined3d_desc.bind_flags = desc->BindFlags;
    wined3d_desc.cpu_access_flags = desc->CPUAccessFlags;
    wined3d_desc.misc_flags = desc->MiscFlags;

    hr = wined3d_buffer_create(device->wined3d_device, &wined3d_desc,
            data ? data->pSysMem : NULL, buffer, &d3d10_buffer_wined3d_parent_ops,
            &buffer->wined3d_buffer);
    if (FAILED(hr))
    {
        WARN("Failed to create wined3d buffer, hr %#x.\n", hr);
        return hr;
    }

    return S_OK;
}
