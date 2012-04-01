/*
 * Copyright (C) 2009 Tony Wasserka
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

#include "wine/debug.h"
#include "d3dx9_36_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

/************************************************************
 * pixel format table providing info about number of bytes per pixel,
 * number of bits per channel and format type.
 *
 * Call get_format_info to request information about a specific format.
 */
static const PixelFormatDesc formats[] =
{
   /* format                    bits per channel        shifts per channel   bpp   type        */
    { D3DFMT_R8G8B8,          {  0,   8,   8,   8 },  {  0,  16,   8,   0 },   3,  FORMAT_ARGB },
    { D3DFMT_A8R8G8B8,        {  8,   8,   8,   8 },  { 24,  16,   8,   0 },   4,  FORMAT_ARGB },
    { D3DFMT_X8R8G8B8,        {  0,   8,   8,   8 },  {  0,  16,   8,   0 },   4,  FORMAT_ARGB },
    { D3DFMT_A8B8G8R8,        {  8,   8,   8,   8 },  { 24,   0,   8,  16 },   4,  FORMAT_ARGB },
    { D3DFMT_X8B8G8R8,        {  0,   8,   8,   8 },  {  0,   0,   8,  16 },   4,  FORMAT_ARGB },
    { D3DFMT_R5G6B5,          {  0,   5,   6,   5 },  {  0,  11,   5,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_X1R5G5B5,        {  0,   5,   5,   5 },  {  0,  10,   5,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_A1R5G5B5,        {  1,   5,   5,   5 },  { 15,  10,   5,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_R3G3B2,          {  0,   3,   3,   2 },  {  0,   5,   2,   0 },   1,  FORMAT_ARGB },
    { D3DFMT_A8R3G3B2,        {  8,   3,   3,   2 },  {  8,   5,   2,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_A4R4G4B4,        {  4,   4,   4,   4 },  { 12,   8,   4,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_X4R4G4B4,        {  0,   4,   4,   4 },  {  0,   8,   4,   0 },   2,  FORMAT_ARGB },
    { D3DFMT_A2R10G10B10,     {  2,  10,  10,  10 },  { 30,  20,  10,   0 },   4,  FORMAT_ARGB },
    { D3DFMT_A2B10G10R10,     {  2,  10,  10,  10 },  { 30,   0,  10,  20 },   4,  FORMAT_ARGB },
    { D3DFMT_G16R16,          {  0,  16,  16,   0 },  {  0,   0,  16,   0 },   4,  FORMAT_ARGB },
    { D3DFMT_A8,              {  8,   0,   0,   0 },  {  0,   0,   0,   0 },   1,  FORMAT_ARGB },

    { D3DFMT_UNKNOWN,         {  0,   0,   0,   0 },  {  0,   0,   0,   0 },   0,  FORMAT_UNKNOWN }, /* marks last element */
};


/************************************************************
 * map_view_of_file
 *
 * Loads a file into buffer and stores the number of read bytes in length.
 *
 * PARAMS
 *   filename [I] name of the file to be loaded
 *   buffer   [O] pointer to destination buffer
 *   length   [O] size of the obtained data
 *
 * RETURNS
 *   Success: D3D_OK
 *   Failure:
 *     see error codes for CreateFileW, GetFileSize, CreateFileMapping and MapViewOfFile
 *
 * NOTES
 *   The caller must UnmapViewOfFile when it doesn't need the data anymore
 *
 */
HRESULT map_view_of_file(LPCWSTR filename, LPVOID *buffer, DWORD *length)
{
    HANDLE hfile, hmapping = NULL;

    hfile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(hfile == INVALID_HANDLE_VALUE) goto error;

    *length = GetFileSize(hfile, NULL);
    if(*length == INVALID_FILE_SIZE) goto error;

    hmapping = CreateFileMappingW(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
    if(!hmapping) goto error;

    *buffer = MapViewOfFile(hmapping, FILE_MAP_READ, 0, 0, 0);
    if(*buffer == NULL) goto error;

    CloseHandle(hmapping);
    CloseHandle(hfile);

    return S_OK;

error:
    CloseHandle(hmapping);
    CloseHandle(hfile);
    return HRESULT_FROM_WIN32(GetLastError());
}

/************************************************************
 * load_resource_into_memory
 *
 * Loads a resource into buffer and stores the number of
 * read bytes in length.
 *
 * PARAMS
 *   module  [I] handle to the module
 *   resinfo [I] handle to the resource's information block
 *   buffer  [O] pointer to destination buffer
 *   length  [O] size of the obtained data
 *
 * RETURNS
 *   Success: D3D_OK
 *   Failure:
 *     See error codes for SizeofResource, LoadResource and LockResource
 *
 * NOTES
 *   The memory doesn't need to be freed by the caller manually
 *
 */
HRESULT load_resource_into_memory(HMODULE module, HRSRC resinfo, LPVOID *buffer, DWORD *length)
{
    HGLOBAL resource;

    *length = SizeofResource(module, resinfo);
    if(*length == 0) return HRESULT_FROM_WIN32(GetLastError());

    resource = LoadResource(module, resinfo);
    if( !resource ) return HRESULT_FROM_WIN32(GetLastError());

    *buffer = LockResource(resource);
    if(*buffer == NULL) return HRESULT_FROM_WIN32(GetLastError());

    return S_OK;
}


/************************************************************
 * get_format_info
 *
 * Returns information about the specified format.
 * If the format is unsupported, it's filled with the D3DFMT_UNKNOWN desc.
 *
 * PARAMS
 *   format [I] format whose description is queried
 *   desc   [O] pointer to a StaticPixelFormatDesc structure
 *
 */
const PixelFormatDesc *get_format_info(D3DFORMAT format)
{
    unsigned int i = 0;
    while(formats[i].format != format && formats[i].format != D3DFMT_UNKNOWN) i++;
    if(formats[i].format == D3DFMT_UNKNOWN)
        FIXME("Unknown format %#x\n", format);
    return &formats[i];
}

const PixelFormatDesc *get_format_info_idx(int idx)
{
    if(idx >= sizeof(formats) / sizeof(formats[0]))
        return NULL;
    if(formats[idx].format == D3DFMT_UNKNOWN)
        return NULL;
    return &formats[idx];
}

#define WINE_D3DX_TO_STR(x) case x: return #x

const char *debug_d3dxparameter_class(D3DXPARAMETER_CLASS c)
{
    switch (c)
    {
        WINE_D3DX_TO_STR(D3DXPC_SCALAR);
        WINE_D3DX_TO_STR(D3DXPC_VECTOR);
        WINE_D3DX_TO_STR(D3DXPC_MATRIX_ROWS);
        WINE_D3DX_TO_STR(D3DXPC_MATRIX_COLUMNS);
        WINE_D3DX_TO_STR(D3DXPC_OBJECT);
        WINE_D3DX_TO_STR(D3DXPC_STRUCT);
        default:
            FIXME("Unrecognized D3DXPARAMETER_CLASS %#x.\n", c);
            return "unrecognized";
    }
}

const char *debug_d3dxparameter_type(D3DXPARAMETER_TYPE t)
{
    switch (t)
    {
        WINE_D3DX_TO_STR(D3DXPT_VOID);
        WINE_D3DX_TO_STR(D3DXPT_BOOL);
        WINE_D3DX_TO_STR(D3DXPT_INT);
        WINE_D3DX_TO_STR(D3DXPT_FLOAT);
        WINE_D3DX_TO_STR(D3DXPT_STRING);
        WINE_D3DX_TO_STR(D3DXPT_TEXTURE);
        WINE_D3DX_TO_STR(D3DXPT_TEXTURE1D);
        WINE_D3DX_TO_STR(D3DXPT_TEXTURE2D);
        WINE_D3DX_TO_STR(D3DXPT_TEXTURE3D);
        WINE_D3DX_TO_STR(D3DXPT_TEXTURECUBE);
        WINE_D3DX_TO_STR(D3DXPT_SAMPLER);
        WINE_D3DX_TO_STR(D3DXPT_SAMPLER1D);
        WINE_D3DX_TO_STR(D3DXPT_SAMPLER2D);
        WINE_D3DX_TO_STR(D3DXPT_SAMPLER3D);
        WINE_D3DX_TO_STR(D3DXPT_SAMPLERCUBE);
        WINE_D3DX_TO_STR(D3DXPT_PIXELSHADER);
        WINE_D3DX_TO_STR(D3DXPT_VERTEXSHADER);
        WINE_D3DX_TO_STR(D3DXPT_PIXELFRAGMENT);
        WINE_D3DX_TO_STR(D3DXPT_VERTEXFRAGMENT);
        WINE_D3DX_TO_STR(D3DXPT_UNSUPPORTED);
        default:
           FIXME("Unrecognized D3DXPARAMETER_TYP %#x.\n", t);
            return "unrecognized";
    }
}

#undef WINE_D3DX_TO_STR
