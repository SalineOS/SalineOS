/*
 * Copyright 2008 Luis Busquets
 * Copyright 2011 Travis Athougies
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

#include "wine/test.h"
#include "d3dx9.h"

static const DWORD simple_vs[] = {
    0xfffe0101,                                                             /* vs_1_1                       */
    0x0000001f, 0x80000000, 0x900f0000,                                     /* dcl_position0 v0             */
    0x00000009, 0xc0010000, 0x90e40000, 0xa0e40000,                         /* dp4 oPos.x, v0, c0           */
    0x00000009, 0xc0020000, 0x90e40000, 0xa0e40001,                         /* dp4 oPos.y, v0, c1           */
    0x00000009, 0xc0040000, 0x90e40000, 0xa0e40002,                         /* dp4 oPos.z, v0, c2           */
    0x00000009, 0xc0080000, 0x90e40000, 0xa0e40003,                         /* dp4 oPos.w, v0, c3           */
    0x0000ffff};                                                            /* END                          */

static const DWORD simple_ps[] = {
    0xffff0101,                                                             /* ps_1_1                       */
    0x00000051, 0xa00f0001, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, /* def c1 = 1.0, 0.0, 0.0, 0.0  */
    0x00000042, 0xb00f0000,                                                 /* tex t0                       */
    0x00000008, 0x800f0000, 0xa0e40001, 0xa0e40000,                         /* dp3 r0, c1, c0               */
    0x00000005, 0x800f0000, 0x90e40000, 0x80e40000,                         /* mul r0, v0, r0               */
    0x00000005, 0x800f0000, 0xb0e40000, 0x80e40000,                         /* mul r0, t0, r0               */
    0x0000ffff};                                                            /* END                          */

#define FCC_TEXT MAKEFOURCC('T','E','X','T')
#define FCC_CTAB MAKEFOURCC('C','T','A','B')

static const DWORD shader_with_ctab[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x0002fffe, FCC_TEXT,   0x00000000,                                     /* TEXT comment                 */
    0x0008fffe, FCC_CTAB,   0x0000001c, 0x00000010, 0xfffe0300, 0x00000000, /* CTAB comment                 */
                0x00000000, 0x00000000, 0x00000000,
    0x0004fffe, FCC_TEXT,   0x00000000, 0x00000000, 0x00000000,             /* TEXT comment                 */
    0x0000ffff};                                                            /* END                          */

static const DWORD shader_with_invalid_ctab[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x0005fffe, FCC_CTAB,                                                   /* CTAB comment                 */
                0x0000001c, 0x000000a9, 0xfffe0300,
                0x00000000, 0x00000000,
    0x0000ffff};                                                            /* END                          */

static const DWORD shader_with_ctab_constants[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x002efffe, FCC_CTAB,                                                   /* CTAB comment                 */
    0x0000001c, 0x000000a4, 0xfffe0300, 0x00000003, 0x0000001c, 0x20008100, /* Header                       */
    0x0000009c,
    0x00000058, 0x00070002, 0x00000001, 0x00000064, 0x00000000,             /* Constant 1 desc              */
    0x00000074, 0x00000002, 0x00000004, 0x00000080, 0x00000000,             /* Constant 2 desc              */
    0x00000090, 0x00040002, 0x00000003, 0x00000080, 0x00000000,             /* Constant 3 desc              */
    0x736e6f43, 0x746e6174, 0xabab0031,                                     /* Constant 1 name string       */
    0x00030001, 0x00040001, 0x00000001, 0x00000000,                         /* Constant 1 type desc         */
    0x736e6f43, 0x746e6174, 0xabab0032,                                     /* Constant 2 name string       */
    0x00030003, 0x00040004, 0x00000001, 0x00000000,                         /* Constant 2 & 3 type desc     */
    0x736e6f43, 0x746e6174, 0xabab0033,                                     /* Constant 3 name string       */
    0x335f7376, 0xab00305f,                                                 /* Target name string           */
    0x656e6957, 0x6f727020, 0x7463656a, 0xababab00,                         /* Creator name string          */
    0x0000ffff};                                                            /* END                          */

static const DWORD ctab_basic[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x0040fffe, FCC_CTAB,                                                   /* CTAB comment                 */
    0x0000001c, 0x000000ec, 0xfffe0300, 0x00000005, 0x0000001c, 0x20008100, /* Header                       */
    0x000000e4,
    0x00000080, 0x00060002, 0x00000001, 0x00000084, 0x00000000,             /* Constant 1 desc (f)          */
    0x00000094, 0x00070002, 0x00000001, 0x00000098, 0x00000000,             /* Constant 2 desc (f4)         */
    0x000000A8, 0x00040002, 0x00000001, 0x000000AC, 0x00000000,             /* Constant 3 desc (i)          */
    0x000000BC, 0x00050002, 0x00000001, 0x000000C0, 0x00000000,             /* Constant 4 desc (i4)         */
    0x000000D0, 0x00000002, 0x00000004, 0x000000D4, 0x00000000,             /* Constant 5 desc (mvp)        */
    0xabab0066, 0x00030000, 0x00010001, 0x00000001, 0x00000000,             /* Constant 1 name/type desc    */
    0xab003466, 0x00030001, 0x00040001, 0x00000001, 0x00000000,             /* Constant 2 name/type desc    */
    0xabab0069, 0x00020000, 0x00010001, 0x00000001, 0x00000000,             /* Constant 3 name/type desc    */
    0xab003469, 0x00020001, 0x00040001, 0x00000001, 0x00000000,             /* Constant 4 name/type desc    */
    0x0070766d, 0x00030003, 0x00040004, 0x00000001, 0x00000000,             /* Constant 5 name/type desc    */
    0x335f7376, 0xab00305f,                                                 /* Target name string           */
    0x656e6957, 0x6f727020, 0x7463656a, 0xababab00,                         /* Creator name string          */
    0x0000ffff};                                                            /* END                          */

static const D3DXCONSTANT_DESC ctab_basic_expected[] = {
    {"mvp", D3DXRS_FLOAT4, 0, 4, D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, 4, 4, 1, 0, 64, 0},
    {"i",   D3DXRS_FLOAT4, 4, 1, D3DXPC_SCALAR,         D3DXPT_INT,   1, 1, 1, 0,  4, 0},
    {"i4",  D3DXRS_FLOAT4, 5, 1, D3DXPC_VECTOR,         D3DXPT_INT,   1, 4, 1, 0, 16, 0},
    {"f",   D3DXRS_FLOAT4, 6, 1, D3DXPC_SCALAR,         D3DXPT_FLOAT, 1, 1, 1, 0,  4, 0},
    {"f4",  D3DXRS_FLOAT4, 7, 1, D3DXPC_VECTOR,         D3DXPT_FLOAT, 1, 4, 1, 0, 16, 0}};

static const DWORD ctab_matrices[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x0032fffe, FCC_CTAB,                                                   /* CTAB comment                 */
    0x0000001c, 0x000000b4, 0xfffe0300, 0x00000003, 0x0000001c, 0x20008100, /* Header                       */
    0x000000ac,
    0x00000058, 0x00070002, 0x00000001, 0x00000064, 0x00000000,             /* Constant 1 desc (fmatrix3x1) */
    0x00000074, 0x00000002, 0x00000004, 0x00000080, 0x00000000,             /* Constant 2 desc (fmatrix4x4) */
    0x00000090, 0x00040002, 0x00000003, 0x0000009c, 0x00000000,             /* Constant 3 desc (imatrix2x3) */
    0x74616D66, 0x33786972, 0xab003178,                                     /* Constant 1 name              */
    0x00030003, 0x00010003, 0x00000001, 0x00000000,                         /* Constant 1 type desc         */
    0x74616D66, 0x34786972, 0xab003478,                                     /* Constant 2 name              */
    0x00030003, 0x00040004, 0x00000001, 0x00000000,                         /* Constant 2 type desc         */
    0x74616D69, 0x32786972, 0xab003378,                                     /* Constant 3 name              */
    0x00020002, 0x00030002, 0x00000001, 0x00000000,                         /* Constant 3 type desc         */
    0x335f7376, 0xab00305f,                                                 /* Target name string           */
    0x656e6957, 0x6f727020, 0x7463656a, 0xababab00,                         /* Creator name string          */
    0x0000ffff};                                                            /* END                          */

static const D3DXCONSTANT_DESC ctab_matrices_expected[] = {
    {"fmatrix4x4", D3DXRS_FLOAT4, 0, 4, D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, 4, 4, 1, 0, 64, 0},
    {"imatrix2x3", D3DXRS_FLOAT4, 4, 3, D3DXPC_MATRIX_ROWS,    D3DXPT_INT,   2, 3, 1, 0, 24, 0},
    {"fmatrix3x1", D3DXRS_FLOAT4, 7, 1, D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, 3, 1, 1, 0, 12, 0}};

static const DWORD ctab_arrays[] = {
    0xfffe0300,                                                             /* vs_3_0                       */
    0x0052fffe, FCC_CTAB,                                                   /* CTAB comment                 */
    0x0000001c, 0x0000013c, 0xfffe0300, 0x00000006, 0x0000001c, 0x20008100, /* Header                       */
    0x00000134,
    0x00000094, 0x000E0002, 0x00000002, 0x0000009c, 0x00000000,             /* Constant 1 desc (barray)     */
    0x000000ac, 0x00100002, 0x00000002, 0x000000b8, 0x00000000,             /* Constant 2 desc (bvecarray)  */
    0x000000c8, 0x00080002, 0x00000004, 0x000000d0, 0x00000000,             /* Constant 3 desc (farray)     */
    0x000000e0, 0x00000002, 0x00000008, 0x000000ec, 0x00000000,             /* Constant 4 desc (fmtxarray)  */
    0x000000fc, 0x000C0002, 0x00000002, 0x00000108, 0x00000000,             /* Constant 5 desc (fvecarray)  */
    0x00000118, 0x00120002, 0x00000001, 0x00000124, 0x00000000,             /* Constant 6 desc (ivecarray)  */
    0x72726162, 0xab007961,                                                 /* Constant 1 name              */
    0x00010000, 0x00010001, 0x00000002, 0x00000000,                         /* Constant 1 type desc         */
    0x63657662, 0x61727261, 0xabab0079,                                     /* Constant 2 name              */
    0x00010001, 0x00030001, 0x00000003, 0x00000000,                         /* Constant 2 type desc         */
    0x72726166, 0xab007961,                                                 /* Constant 3 name              */
    0x00030000, 0x00010001, 0x00000004, 0x00000000,                         /* constant 3 type desc         */
    0x78746d66, 0x61727261, 0xabab0079,                                     /* Constant 4 name              */
    0x00030002, 0x00040004, 0x00000002, 0x00000000,                         /* Constant 4 type desc         */
    0x63657666, 0x61727261, 0xabab0079,                                     /* Constant 5 name              */
    0x00030001, 0x00040001, 0x00000002, 0x00000000,                         /* Constant 5 type desc         */
    0x63657669, 0x61727261, 0xabab0079,                                     /* Constant 6 name              */
    0x00020001, 0x00040001, 0x00000001, 0x00000000,                         /* Constant 6 type desc         */
    0x335f7376, 0xab00305f,                                                 /* Target name string           */
    0x656e6957, 0x6f727020, 0x7463656a, 0xababab00,                         /* Creator name string          */
    0x0000ffff};                                                            /* END                          */

static const D3DXCONSTANT_DESC ctab_arrays_expected[] = {
    {"fmtxarray", D3DXRS_FLOAT4,  0, 8, D3DXPC_MATRIX_ROWS, D3DXPT_FLOAT, 4, 4, 2, 0, 128, 0},
    {"farray",    D3DXRS_FLOAT4,  8, 4, D3DXPC_SCALAR,      D3DXPT_FLOAT, 1, 1, 4, 0,  16, 0},
    {"fvecarray", D3DXRS_FLOAT4, 12, 2, D3DXPC_VECTOR,      D3DXPT_FLOAT, 1, 4, 2, 0,  32, 0},
    {"barray",    D3DXRS_FLOAT4, 14, 2, D3DXPC_SCALAR,      D3DXPT_BOOL,  1, 1, 2, 0,   8, 0},
    {"bvecarray", D3DXRS_FLOAT4, 16, 2, D3DXPC_VECTOR,      D3DXPT_BOOL,  1, 3, 3, 0,  36, 0},
    {"ivecarray", D3DXRS_FLOAT4, 18, 1, D3DXPC_VECTOR,      D3DXPT_INT,   1, 4, 1, 0,  16, 0}};

static const DWORD ctab_samplers[] = {
    0xfffe0300,                                                             /* vs_3_0                        */
    0x0032fffe, FCC_CTAB,                                                   /* CTAB comment                  */
    0x0000001c, 0x000000b4, 0xfffe0300, 0x00000003, 0x0000001c, 0x20008100, /* Header                        */
    0x000000ac,
    0x00000058, 0x00020002, 0x00000001, 0x00000064, 0x00000000,             /* Constant 1 desc (notsampler)  */
    0x00000074, 0x00000003, 0x00000001, 0x00000080, 0x00000000,             /* Constant 2 desc (sampler1)    */
    0x00000090, 0x00030003, 0x00000001, 0x0000009c, 0x00000000,             /* Constant 3 desc (sampler2)    */
    0x73746f6e, 0x6c706d61, 0xab007265,                                     /* Constant 1 name               */
    0x00030001, 0x00040001, 0x00000001, 0x00000000,                         /* Constant 1 type desc          */
    0x706d6173, 0x3172656c, 0xababab00,                                     /* Constant 2 name               */
    0x000c0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 2 type desc          */
    0x706d6173, 0x3272656c, 0xababab00,                                     /* Constant 3 name               */
    0x000d0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 3 type desc          */
    0x335f7376, 0xab00305f,                                                 /* Target name string            */
    0x656e6957, 0x6f727020, 0x7463656a, 0xababab00,                         /* Creator name string           */
    0x0000ffff};                                                            /* END                           */

static const D3DXCONSTANT_DESC ctab_samplers_expected[] = {
    {"sampler1",   D3DXRS_SAMPLER, 0, 1, D3DXPC_OBJECT, D3DXPT_SAMPLER2D, 1, 1, 1, 0, 4,  0},
    {"sampler2",   D3DXRS_SAMPLER, 3, 1, D3DXPC_OBJECT, D3DXPT_SAMPLER3D, 1, 1, 1, 0, 4,  0},
    {"notsampler", D3DXRS_FLOAT4,  2, 1, D3DXPC_VECTOR, D3DXPT_FLOAT,     1, 4, 1, 0, 16, 0}};

static void test_get_shader_size(void)
{
    UINT shader_size, expected;

    shader_size = D3DXGetShaderSize(simple_vs);
    expected = sizeof(simple_vs);
    ok(shader_size == expected, "Got shader size %u, expected %u\n", shader_size, expected);

    shader_size = D3DXGetShaderSize(simple_ps);
    expected = sizeof(simple_ps);
    ok(shader_size == expected, "Got shader size %u, expected %u\n", shader_size, expected);

    shader_size = D3DXGetShaderSize(NULL);
    ok(shader_size == 0, "Got shader size %u, expected 0\n", shader_size);
}

static void test_get_shader_version(void)
{
    DWORD shader_version;

    shader_version = D3DXGetShaderVersion(simple_vs);
    ok(shader_version == D3DVS_VERSION(1, 1), "Got shader version 0x%08x, expected 0x%08x\n",
            shader_version, D3DVS_VERSION(1, 1));

    shader_version = D3DXGetShaderVersion(simple_ps);
    ok(shader_version == D3DPS_VERSION(1, 1), "Got shader version 0x%08x, expected 0x%08x\n",
            shader_version, D3DPS_VERSION(1, 1));

    shader_version = D3DXGetShaderVersion(NULL);
    ok(shader_version == 0, "Got shader version 0x%08x, expected 0\n", shader_version);
}

static void test_find_shader_comment(void)
{
    HRESULT hr;
    LPCVOID data = (LPVOID)0xdeadbeef;
    UINT size = 100;

    hr = D3DXFindShaderComment(NULL, MAKEFOURCC('C','T','A','B'), &data, &size);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);
    ok(!data, "Got %p, expected NULL\n", data);
    ok(!size, "Got %u, expected 0\n", size);

    hr = D3DXFindShaderComment(shader_with_ctab, MAKEFOURCC('C','T','A','B'), NULL, &size);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    ok(size == 28, "Got %u, expected 28\n", size);

    hr = D3DXFindShaderComment(shader_with_ctab, MAKEFOURCC('C','T','A','B'), &data, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    ok(data == (LPCVOID)(shader_with_ctab + 6), "Got result %p, expected %p\n", data, shader_with_ctab + 6);

    hr = D3DXFindShaderComment(shader_with_ctab, 0, &data, &size);
    ok(hr == S_FALSE, "Got result %x, expected 1 (S_FALSE)\n", hr);
    ok(!data, "Got %p, expected NULL\n", data);
    ok(!size, "Got %u, expected 0\n", size);

    hr = D3DXFindShaderComment(shader_with_ctab, MAKEFOURCC('X','X','X','X'), &data, &size);
    ok(hr == S_FALSE, "Got result %x, expected 1 (S_FALSE)\n", hr);
    ok(!data, "Got %p, expected NULL\n", data);
    ok(!size, "Got %u, expected 0\n", size);

    hr = D3DXFindShaderComment(shader_with_ctab, MAKEFOURCC('C','T','A','B'), &data, &size);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    ok(data == (LPCVOID)(shader_with_ctab + 6), "Got result %p, expected %p\n", data, shader_with_ctab + 6);
    ok(size == 28, "Got result %d, expected 28\n", size);
}

static void test_get_shader_constant_table_ex(void)
{
    LPD3DXCONSTANTTABLE constant_table = NULL;
    HRESULT hr;
    LPVOID data;
    DWORD size;
    D3DXCONSTANTTABLE_DESC desc;

    hr = D3DXGetShaderConstantTableEx(NULL, 0, &constant_table);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    /* No CTAB data */
    hr = D3DXGetShaderConstantTableEx(simple_ps, 0, &constant_table);
    ok(hr == D3DXERR_INVALIDDATA, "Got result %x, expected %x (D3DXERR_INVALIDDATA)\n", hr, D3DXERR_INVALIDDATA);

    /* With invalid CTAB data */
    hr = D3DXGetShaderConstantTableEx(shader_with_invalid_ctab, 0, &constant_table);
    ok(hr == D3DXERR_INVALIDDATA || broken(hr == D3D_OK), /* winxp 64-bit, w2k3 64-bit */
       "Got result %x, expected %x (D3DXERR_INVALIDDATA)\n", hr, D3DXERR_INVALIDDATA);
    if (constant_table) ID3DXConstantTable_Release(constant_table);

    hr = D3DXGetShaderConstantTableEx(shader_with_ctab, 0, &constant_table);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

    if (constant_table)
    {
        size = ID3DXConstantTable_GetBufferSize(constant_table);
        ok(size == 28, "Got result %x, expected 28\n", size);

        data = ID3DXConstantTable_GetBufferPointer(constant_table);
        ok(!memcmp(data, shader_with_ctab + 6, size), "Retrieved wrong CTAB data\n");

        hr = ID3DXConstantTable_GetDesc(constant_table, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = ID3DXConstantTable_GetDesc(constant_table, &desc);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(desc.Creator == (LPCSTR)data + 0x10, "Got result %p, expected %p\n", desc.Creator, (LPCSTR)data + 0x10);
        ok(desc.Version == D3DVS_VERSION(3, 0), "Got result %x, expected %x\n", desc.Version, D3DVS_VERSION(3, 0));
        ok(desc.Constants == 0, "Got result %x, expected 0\n", desc.Constants);

        ID3DXConstantTable_Release(constant_table);
    }

    hr = D3DXGetShaderConstantTableEx(shader_with_ctab_constants, 0, &constant_table);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

    if (SUCCEEDED(hr))
    {
        D3DXHANDLE constant;
        D3DXCONSTANT_DESC constant_desc;
        D3DXCONSTANT_DESC constant_desc_save;
        UINT nb;

        /* Test GetDesc */
        hr = ID3DXConstantTable_GetDesc(constant_table, &desc);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!strcmp(desc.Creator, "Wine project"), "Got result '%s', expected 'Wine project'\n", desc.Creator);
        ok(desc.Version == D3DVS_VERSION(3, 0), "Got result %x, expected %x\n", desc.Version, D3DVS_VERSION(3, 0));
        ok(desc.Constants == 3, "Got result %x, expected 3\n", desc.Constants);

        /* Test GetConstant */
        constant = ID3DXConstantTable_GetConstant(constant_table, NULL, 0);
        ok(constant != NULL, "No constant found\n");
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, &constant_desc, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!strcmp(constant_desc.Name, "Constant1"), "Got result '%s', expected 'Constant1'\n",
            constant_desc.Name);
        ok(constant_desc.Class == D3DXPC_VECTOR, "Got result %x, expected %u (D3DXPC_VECTOR)\n",
            constant_desc.Class, D3DXPC_VECTOR);
        ok(constant_desc.Type == D3DXPT_FLOAT, "Got result %x, expected %u (D3DXPT_FLOAT)\n",
            constant_desc.Type, D3DXPT_FLOAT);
        ok(constant_desc.Rows == 1, "Got result %x, expected 1\n", constant_desc.Rows);
        ok(constant_desc.Columns == 4, "Got result %x, expected 4\n", constant_desc.Columns);

        constant = ID3DXConstantTable_GetConstant(constant_table, NULL, 1);
        ok(constant != NULL, "No constant found\n");
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, &constant_desc, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!strcmp(constant_desc.Name, "Constant2"), "Got result '%s', expected 'Constant2'\n",
            constant_desc.Name);
        ok(constant_desc.Class == D3DXPC_MATRIX_COLUMNS, "Got result %x, expected %u (D3DXPC_MATRIX_COLUMNS)\n",
            constant_desc.Class, D3DXPC_MATRIX_COLUMNS);
        ok(constant_desc.Type == D3DXPT_FLOAT, "Got result %x, expected %u (D3DXPT_FLOAT)\n",
            constant_desc.Type, D3DXPT_FLOAT);
        ok(constant_desc.Rows == 4, "Got result %x, expected 1\n", constant_desc.Rows);
        ok(constant_desc.Columns == 4, "Got result %x, expected 4\n", constant_desc.Columns);

        constant = ID3DXConstantTable_GetConstant(constant_table, NULL, 2);
        ok(constant != NULL, "No constant found\n");
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, &constant_desc, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!strcmp(constant_desc.Name, "Constant3"), "Got result '%s', expected 'Constant3'\n",
            constant_desc.Name);
        ok(constant_desc.Class == D3DXPC_MATRIX_COLUMNS, "Got result %x, expected %u (D3DXPC_MATRIX_COLUMNS)\n",
            constant_desc.Class, D3DXPC_MATRIX_COLUMNS);
        ok(constant_desc.Type == D3DXPT_FLOAT, "Got result %x, expected %u (D3DXPT_FLOAT)\n",
            constant_desc.Type, D3DXPT_FLOAT);
        ok(constant_desc.Rows == 4, "Got result %x, expected 1\n", constant_desc.Rows);
        ok(constant_desc.Columns == 4, "Got result %x, expected 4\n", constant_desc.Columns);
        constant_desc_save = constant_desc; /* For GetConstantDesc test */

        constant = ID3DXConstantTable_GetConstant(constant_table, NULL, 3);
        ok(constant == NULL, "Got result %p, expected NULL\n", constant);

        /* Test GetConstantByName */
        constant = ID3DXConstantTable_GetConstantByName(constant_table, NULL, "Constant unknown");
        ok(constant == NULL, "Got result %p, expected NULL\n", constant);
        constant = ID3DXConstantTable_GetConstantByName(constant_table, NULL, "Constant3");
        ok(constant != NULL, "No constant found\n");
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, &constant_desc, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!memcmp(&constant_desc, &constant_desc_save, sizeof(D3DXCONSTANT_DESC)), "Got different constant data\n");

        /* Test GetConstantDesc */
        constant = ID3DXConstantTable_GetConstant(constant_table, NULL, 0);
        ok(constant != NULL, "No constant found\n");
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, NULL, &constant_desc, &nb);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, NULL, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, constant, &constant_desc, NULL);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, "Constant unknow", &constant_desc, &nb);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);
        hr = ID3DXConstantTable_GetConstantDesc(constant_table, "Constant3", &constant_desc, &nb);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
        ok(!memcmp(&constant_desc, &constant_desc_save, sizeof(D3DXCONSTANT_DESC)), "Got different constant data\n");

        ID3DXConstantTable_Release(constant_table);
    }
}

static void test_constant_table(const char *test_name, const DWORD *ctable_fn,
        const D3DXCONSTANT_DESC *expecteds, UINT count)
{
    UINT i;
    ID3DXConstantTable *ctable;

    HRESULT res;

    /* Get the constant table from the shader itself */
    res = D3DXGetShaderConstantTable(ctable_fn, &ctable);
    ok(res == D3D_OK, "D3DXGetShaderConstantTable failed on %s: got %08x\n", test_name, res);

    for (i = 0; i < count; i++)
    {
        const D3DXCONSTANT_DESC *expected = &expecteds[i];
        D3DXHANDLE const_handle;
        D3DXCONSTANT_DESC actual;
        UINT pCount = 1;

        const_handle = ID3DXConstantTable_GetConstantByName(ctable, NULL, expected->Name);

        res = ID3DXConstantTable_GetConstantDesc(ctable, const_handle, &actual, &pCount);
        ok(SUCCEEDED(res), "%s in %s: ID3DXConstantTable_GetConstantDesc returned %08x\n", expected->Name,
                test_name, res);
        ok(pCount == 1, "%s in %s: Got more or less descriptions: %d\n", expected->Name, test_name, pCount);

        ok(strcmp(actual.Name, expected->Name) == 0,
           "%s in %s: Got different names: Got %s, expected %s\n", expected->Name,
           test_name, actual.Name, expected->Name);
        ok(actual.RegisterSet == expected->RegisterSet,
           "%s in %s: Got different register sets: Got %d, expected %d\n",
           expected->Name, test_name, actual.RegisterSet, expected->RegisterSet);
        ok(actual.RegisterIndex == expected->RegisterIndex,
           "%s in %s: Got different register indices: Got %d, expected %d\n",
           expected->Name, test_name, actual.RegisterIndex, expected->RegisterIndex);
        ok(actual.RegisterCount == expected->RegisterCount,
           "%s in %s: Got different register counts: Got %d, expected %d\n",
           expected->Name, test_name, actual.RegisterCount, expected->RegisterCount);
        ok(actual.Class == expected->Class,
           "%s in %s: Got different classes: Got %d, expected %d\n", expected->Name,
           test_name, actual.Class, expected->Class);
        ok(actual.Type == expected->Type,
           "%s in %s: Got different types: Got %d, expected %d\n", expected->Name,
           test_name, actual.Type, expected->Type);
        ok(actual.Rows == expected->Rows && actual.Columns == expected->Columns,
           "%s in %s: Got different dimensions: Got (%d, %d), expected (%d, %d)\n",
           expected->Name, test_name, actual.Rows, actual.Columns, expected->Rows,
           expected->Columns);
        ok(actual.Elements == expected->Elements,
           "%s in %s: Got different element count: Got %d, expected %d\n",
           expected->Name, test_name, actual.Elements, expected->Elements);
        ok(actual.StructMembers == expected->StructMembers,
           "%s in %s: Got different struct member count: Got %d, expected %d\n",
           expected->Name, test_name, actual.StructMembers, expected->StructMembers);
        ok(actual.Bytes == expected->Bytes,
           "%s in %s: Got different byte count: Got %d, expected %d\n",
           expected->Name, test_name, actual.Bytes, expected->Bytes);
    }

    /* Finally, release the constant table */
    ID3DXConstantTable_Release(ctable);
}

static void test_constant_tables(void)
{
    test_constant_table("test_basic", ctab_basic, ctab_basic_expected,
            sizeof(ctab_basic_expected)/sizeof(*ctab_basic_expected));
    test_constant_table("test_matrices", ctab_matrices, ctab_matrices_expected,
            sizeof(ctab_matrices_expected)/sizeof(*ctab_matrices_expected));
    test_constant_table("test_arrays", ctab_arrays, ctab_arrays_expected,
            sizeof(ctab_arrays_expected)/sizeof(*ctab_arrays_expected));
    test_constant_table("test_samplers", ctab_samplers, ctab_samplers_expected,
            sizeof(ctab_samplers_expected)/sizeof(*ctab_samplers_expected));
}

static void test_setting_basic_table(IDirect3DDevice9 *device)
{
    static const D3DXMATRIX mvp = {{{
        0.514f, 0.626f, 0.804f, 0.786f,
        0.238f, 0.956f, 0.374f, 0.483f,
        0.109f, 0.586f, 0.900f, 0.255f,
        0.898f, 0.411f, 0.932f, 0.275f}}};
    static const D3DXVECTOR4 f4 = {0.350f, 0.526f, 0.925f, 0.021f};
    static const float f = 0.12543f;
    static const int i = 321;

    ID3DXConstantTable *ctable;

    HRESULT res;
    float out[16];
    ULONG refcnt;

    /* Get the constant table from the shader itself */
    res = D3DXGetShaderConstantTable(ctab_basic, &ctable);
    ok(res == D3D_OK, "D3DXGetShaderConstantTable failed: got 0x%08x\n", res);

    /* Set constants */
    res = ID3DXConstantTable_SetMatrix(ctable, device, "mvp", &mvp);
    ok(res == D3D_OK, "ID3DXConstantTable_SetMatrix failed on variable mvp: got 0x%08x\n", res);

    ID3DXConstantTable_SetInt(ctable, device, "i", i + 1);
    ok(res == D3D_OK, "ID3DXConstantTable_SetInt failed on variable i: got 0x%08x\n", res);

    /* Check that setting i again will overwrite the previous value */
    res = ID3DXConstantTable_SetInt(ctable, device, "i", i);
    ok(res == D3D_OK, "ID3DXConstantTable_SetInt failed on variable i: got 0x%08x\n", res);

    res = ID3DXConstantTable_SetFloat(ctable, device, "f", f);
    ok(res == D3D_OK, "ID3DXConstantTable_SetFloat failed on variable f: got 0x%08x\n", res);

    res = ID3DXConstantTable_SetVector(ctable, device, "f4", &f4);
    ok(res == D3D_OK, "ID3DXConstantTable_SetVector failed on variable f4: got 0x%08x\n", res);

    /* Get constants back and validate */
    IDirect3DDevice9_GetVertexShaderConstantF(device, 0, out, 4);
    ok(out[0] == S(U(mvp))._11 && out[4] == S(U(mvp))._12 && out[8] == S(U(mvp))._13 && out[12] == S(U(mvp))._14,
            "The first row of mvp was not set correctly, got {%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[0], out[4], out[8], out[12], S(U(mvp))._11, S(U(mvp))._12, S(U(mvp))._13, S(U(mvp))._14);
    ok(out[1] == S(U(mvp))._21 && out[5] == S(U(mvp))._22 && out[9] == S(U(mvp))._23 && out[13] == S(U(mvp))._24,
            "The second row of mvp was not set correctly, got {%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[1], out[5], out[9], out[13], S(U(mvp))._21, S(U(mvp))._22, S(U(mvp))._23, S(U(mvp))._24);
    ok(out[2] == S(U(mvp))._31 && out[6] == S(U(mvp))._32 && out[10] == S(U(mvp))._33 && out[14] == S(U(mvp))._34,
            "The third row of mvp was not set correctly, got {%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[2], out[6], out[10], out[14], S(U(mvp))._31, S(U(mvp))._32, S(U(mvp))._33, S(U(mvp))._34);
    ok(out[3] == S(U(mvp))._41 && out[7] == S(U(mvp))._42 && out[11] == S(U(mvp))._43 && out[15] == S(U(mvp))._44,
            "The fourth row of mvp was not set correctly, got {%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[3], out[7], out[11], out[15], S(U(mvp))._41, S(U(mvp))._42, S(U(mvp))._43, S(U(mvp))._44);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 4, out, 1);
    ok(out[0] == (float)i && out[1] == 0.0f && out[2] == 0.0f && out[3] == 0.0f,
            "The variable i was not set correctly, out={%f, %f, %f, %f}, should be {%d, 0.0, 0.0, 0.0}\n",
            out[0], out[1], out[2], out[3], i);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 6, out, 1);
    ok(out[0] == f && out[1] == 0.0f && out[2] == 0.0f && out[3] == 0.0f,
            "The variable f was not set correctly, out={%f, %f, %f, %f}, should be {%f, 0.0, 0.0, 0.0}\n",
            out[0], out[1], out[2], out[3], f);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 7, out, 1);
    ok(memcmp(out, (void*)&f4, sizeof(f4)) == 0,
            "The variable f4 was not set correctly, out={%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[0], out[1], out[2], out[3], f4.x, f4.y, f4.z, f4.w);

    /* Finally test using a set* function for one type to set a variable of another type (should succeed) */
    res = ID3DXConstantTable_SetVector(ctable, device, "f", &f4);
    ok(res == D3D_OK, "ID3DXConstantTable_SetVector failed on variable f: 0x%08x\n", res);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 6, out, 1);
    ok(out[0] == f4.x, "The variable f was not set correctly by ID3DXConstantTable_SetVector, got %f, should be %f\n",
            out[0], f4.x);

    refcnt = ID3DXConstantTable_Release(ctable);
    ok(refcnt == 0, "The constant table reference count was %u, should be 0\n", refcnt);
}

static void test_setting_arrays_table(IDirect3DDevice9 *device)
{
    static const float farray[8] = {
        0.005f, 0.745f, 0.973f, 0.264f,
        0.010f, 0.020f, 0.030f, 0.040f};
    static const D3DXMATRIX fmtxarray[2] = {
        {{{0.001f, 0.002f, 0.003f, 0.004f,
           0.005f, 0.006f, 0.007f, 0.008f,
           0.009f, 0.010f, 0.011f, 0.012f,
           0.013f, 0.014f, 0.015f, 0.016f}}},
        {{{0.010f, 0.020f, 0.030f, 0.040f,
           0.050f, 0.060f, 0.070f, 0.080f,
           0.090f, 0.100f, 0.110f, 0.120f,
           0.130f, 0.140f, 0.150f, 0.160f}}}};
    static const int iarray[4] = {1, 2, 3, 4};
    static const D3DXVECTOR4 fvecarray[2] = {
        {0.745f, 0.997f, 0.353f, 0.237f},
        {0.060f, 0.455f, 0.333f, 0.983f}};

    ID3DXConstantTable *ctable;

    HRESULT res;
    float out[32];
    ULONG refcnt;

    /* Get the constant table from the shader */
    res = D3DXGetShaderConstantTable(ctab_arrays, &ctable);
    ok(res == D3D_OK, "D3DXGetShaderConstantTable failed: got 0x%08x\n", res);

    /* Set constants */

    /* Make sure that we cannot set registers that do not belong to this constant */
    res = ID3DXConstantTable_SetFloatArray(ctable, device, "farray", farray, 8);
    ok(res == D3D_OK, "ID3DXConstantTable_SetFloatArray failed: got 0x%08x\n", res);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 8, out, 8);
    ok(out[0] == farray[0] && out[4] == farray[1] && out[8] == farray[2] && out[12] == farray[3],
            "The in-bounds elements of the array were not set, out={%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[0], out[4], out[8], out[12], farray[0], farray[1], farray[2], farray[3]);
    ok(out[16] == 0.0f && out[20] == 0.0f && out[24] == 0.0f && out[28] == 0.0f,
            "The excess elements of the array were set, out={%f, %f, %f, %f}, should be all 0.0f\n",
            out[16], out[20], out[24], out[28]);

    /* ivecarray takes up only 1 register, but a matrix takes up 4, so no elements should be set */
    res = ID3DXConstantTable_SetMatrix(ctable, device, "ivecarray", &fmtxarray[0]);
    ok(res == D3D_OK, "ID3DXConstantTable_SetMatrix failed: got 0x%08x\n", res);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 18, out, 4);
    ok(out[0] == 0.0f && out[1] == 0.0f && out[2] == 0.0f && out[3] == 0.0f,
       "The array was set, out={%f, %f, %f, %f}, should be all 0.0f\n", out[0], out[1], out[2], out[3]);

    /* Try setting an integer array to an array declared as a float array */
    res = ID3DXConstantTable_SetIntArray(ctable, device, "farray", iarray, 4);
    ok(res == D3D_OK, "ID3DXConstantTable_SetIntArray failed: got 0x%08x\n", res);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 8, out, 4);
    ok(out[0] == iarray[0] && out[4] == iarray[1] && out[8] == iarray[2] && out[12] == iarray[3],
           "SetIntArray did not properly set a float array: out={%f, %f, %f, %f}, should be {%d, %d, %d, %d}\n",
            out[0], out[4], out[8], out[12], iarray[0], iarray[1], iarray[2], iarray[3]);

    res = ID3DXConstantTable_SetFloatArray(ctable, device, "farray", farray, 4);
    ok(res == D3D_OK, "ID3DXConstantTable_SetFloatArray failed: got x0%08x\n", res);

    res = ID3DXConstantTable_SetVectorArray(ctable, device, "fvecarray", fvecarray, 2);
    ok(res == D3D_OK, "ID3DXConstantTable_SetVectorArray failed: got 0x%08x\n", res);

    res = ID3DXConstantTable_SetMatrixArray(ctable, device, "fmtxarray", fmtxarray, 2);
    ok(res == D3D_OK, "ID3DXConstantTable_SetMatrixArray failed: got 0x%08x\n", res);

    /* Read back constants */
    IDirect3DDevice9_GetVertexShaderConstantF(device, 8, out, 4);
    ok(out[0] == farray[0] && out[4] == farray[1] && out[8] == farray[2] && out[12] == farray[3],
            "The variable farray was not set correctly, out={%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
            out[0], out[4], out[8], out[12], farray[0], farray[1], farray[2], farray[3]);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 12, out, 2);
    ok(out[0] == fvecarray[0].x && out[1] == fvecarray[0].y && out[2] == fvecarray[0].z && out[3] == fvecarray[0].w &&
            out[4] == fvecarray[1].x && out[5] == fvecarray[1].y && out[6] == fvecarray[1].z && out[7] == fvecarray[1].w,
            "The variable fvecarray was not set correctly, out={{%f, %f, %f, %f}, {%f, %f, %f, %f}}, should be "
            "{{%f, %f, %f, %f}, {%f, %f, %f, %f}}\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7],
            fvecarray[0].x, fvecarray[0].y, fvecarray[0].z, fvecarray[0].w, fvecarray[1].x, fvecarray[1].y,
            fvecarray[1].z, fvecarray[1].w);

    IDirect3DDevice9_GetVertexShaderConstantF(device, 0, out, 8);
    /* Just check a few elements in each matrix to make sure fmtxarray was set row-major */
    ok(out[0] == S(U(fmtxarray[0]))._11 && out[1] == S(U(fmtxarray[0]))._12 && out[2] == S(U(fmtxarray[0]))._13 && out[3] == S(U(fmtxarray[0]))._14,
           "The variable fmtxarray was not set row-major, out={%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
           out[0], out[1], out[2], out[3], S(U(fmtxarray[0]))._11, S(U(fmtxarray[0]))._12, S(U(fmtxarray[0]))._13, S(U(fmtxarray[0]))._14);
    ok(out[16] == S(U(fmtxarray[1]))._11 && out[17] == S(U(fmtxarray[1]))._12 && out[18] == S(U(fmtxarray[1]))._13 && out[19] == S(U(fmtxarray[1]))._14,
           "The variable fmtxarray was not set row-major, out={%f, %f, %f, %f}, should be {%f, %f, %f, %f}\n",
           out[16], out[17], out[18], out[19], S(U(fmtxarray[1]))._11, S(U(fmtxarray[1]))._12, S(U(fmtxarray[1]))._13, S(U(fmtxarray[1]))._14);

    refcnt = ID3DXConstantTable_Release(ctable);
    ok(refcnt == 0, "The constant table reference count was %u, should be 0\n", refcnt);
}

static void test_setting_constants(void)
{
    HWND wnd;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device;
    D3DPRESENT_PARAMETERS d3dpp;
    HRESULT hr;
    ULONG refcnt;

    /* Create the device to use for our tests */
    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed   = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    test_setting_basic_table(device);
    test_setting_arrays_table(device);

    /* Release resources */
    refcnt = IDirect3DDevice9_Release(device);
    ok(refcnt == 0, "The Direct3D device reference count was %u, should be 0\n", refcnt);

    refcnt = IDirect3D9_Release(d3d);
    ok(refcnt == 0, "The Direct3D object referenct count was %u, should be 0\n", refcnt);

    if (wnd) DestroyWindow(wnd);
}

static void test_get_sampler_index(void)
{
    ID3DXConstantTable *ctable;

    HRESULT res;
    UINT index;

    ULONG refcnt;

    res = D3DXGetShaderConstantTable(ctab_samplers, &ctable);
    ok(res == D3D_OK, "D3DXGetShaderConstantTable failed on ctab_samplers: got %08x\n", res);

    index = ID3DXConstantTable_GetSamplerIndex(ctable, "sampler1");
    ok(index == 0, "ID3DXConstantTable_GetSamplerIndex returned wrong index: Got %d, expected 0\n", index);

    index = ID3DXConstantTable_GetSamplerIndex(ctable, "sampler2");
    ok(index == 3, "ID3DXConstantTable_GetSamplerIndex returned wrong index: Got %d, expected 3\n", index);

    index = ID3DXConstantTable_GetSamplerIndex(ctable, "nonexistent");
    ok(index == -1, "ID3DXConstantTable_GetSamplerIndex found nonexistent sampler: Got %d\n",
            index);

    index = ID3DXConstantTable_GetSamplerIndex(ctable, "notsampler");
    ok(index == -1, "ID3DXConstantTable_GetSamplerIndex succeeded on non-sampler constant: Got %d\n",
            index);

    refcnt = ID3DXConstantTable_Release(ctable);
    ok(refcnt == 0, "The ID3DXConstantTable reference count was %u, should be 0\n", refcnt);
}

/*
 * fxc.exe /Tps_3_0
 */
#if 0
sampler s;
sampler1D s1D;
sampler2D s2D;
sampler3D s3D;
samplerCUBE scube;
float4 init;
float4 main(float3 tex : TEXCOORD0) : COLOR
{
    float4 tmp = init;
    tmp = tmp + tex1D(s1D, tex.x);
    tmp = tmp + tex1D(s1D, tex.y);
    tmp = tmp + tex3D(s3D, tex.xyz);
    tmp = tmp + tex1D(s, tex.x);
    tmp = tmp + tex2D(s2D, tex.xy);
    tmp = tmp + texCUBE(scube, tex.xyz);
    return tmp;
}
#endif
static const DWORD get_shader_samplers_blob[] =
{
    0xffff0300,                                                             /* ps_3_0                        */
    0x0054fffe, FCC_CTAB,                                                   /* CTAB comment                  */
    0x0000001c, 0x0000011b, 0xffff0300, 0x00000006, 0x0000001c, 0x00000100, /* Header                        */
    0x00000114,
    0x00000094, 0x00000002, 0x00000001, 0x0000009c, 0x00000000,             /* Constant 1 desc (init)        */
    0x000000ac, 0x00040003, 0x00000001, 0x000000b0, 0x00000000,             /* Constant 2 desc (s)           */
    0x000000c0, 0x00000003, 0x00000001, 0x000000c4, 0x00000000,             /* Constant 3 desc (s1D)         */
    0x000000d4, 0x00010003, 0x00000001, 0x000000d8, 0x00000000,             /* Constant 4 desc (s2D)         */
    0x000000e8, 0x00030003, 0x00000001, 0x000000ec, 0x00000000,             /* Constant 5 desc (s3D)         */
    0x000000fc, 0x00020003, 0x00000001, 0x00000104, 0x00000000,             /* Constant 6 desc (scube)       */
    0x74696e69, 0xababab00,                                                 /* Constant 1 name               */
    0x00030001, 0x00040001, 0x00000001, 0x00000000,                         /* Constant 1 type desc          */
    0xabab0073,                                                             /* Constant 2 name               */
    0x000c0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 2 type desc          */
    0x00443173,                                                             /* Constant 3 name               */
    0x000b0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 3 type desc          */
    0x00443273,                                                             /* Constant 4 name               */
    0x000c0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 4 type desc          */
    0x00443373,                                                             /* Constant 5 name               */
    0x000d0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 5 type desc          */
    0x62756373, 0xabab0065,                                                 /* Constant 6 name               */
    0x000e0004, 0x00010001, 0x00000001, 0x00000000,                         /* Constant 6 type desc          */
    0x335f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, /* Target/Creator name string    */
    0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3932,
    0x332e3235, 0x00313131,
    0x0200001f, 0x80000005, 0x90070000, 0x0200001f, 0x90000000, 0xa00f0800, /* shader                        */
    0x0200001f, 0x90000000, 0xa00f0801, 0x0200001f, 0x98000000, 0xa00f0802,
    0x0200001f, 0xa0000000, 0xa00f0803, 0x0200001f, 0x90000000, 0xa00f0804,
    0x03000042, 0x800f0000, 0x90e40000, 0xa0e40800, 0x03000002, 0x800f0000,
    0x80e40000, 0xa0e40000, 0x03000042, 0x800f0001, 0x90550000, 0xa0e40800,
    0x03000002, 0x800f0000, 0x80e40000, 0x80e40001, 0x03000042, 0x800f0001,
    0x90e40000, 0xa0e40803, 0x03000002, 0x800f0000, 0x80e40000, 0x80e40001,
    0x03000042, 0x800f0001, 0x90e40000, 0xa0e40804, 0x03000002, 0x800f0000,
    0x80e40000, 0x80e40001, 0x03000042, 0x800f0001, 0x90e40000, 0xa0e40801,
    0x03000002, 0x800f0000, 0x80e40000, 0x80e40001, 0x03000042, 0x800f0001,
    0x90e40000, 0xa0e40802, 0x03000002, 0x800f0800, 0x80e40000, 0x80e40001,
    0x0000ffff,                                                             /* END                           */
};

static void test_get_shader_samplers(void)
{
    LPCSTR samplers[16] = {NULL}; /* maximum number of sampler registers v/ps 3.0 = 16 */
    LPCSTR sampler_orig;
    UINT count = 2;
    HRESULT hr;

#if 0
    /* crashes if bytecode is NULL */
    hr = D3DXGetShaderSamplers(NULL, NULL, &count);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);
#endif

    hr = D3DXGetShaderSamplers(get_shader_samplers_blob, NULL, NULL);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);

    samplers[5] = "dummy";

    hr = D3DXGetShaderSamplers(get_shader_samplers_blob, samplers, NULL);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);

    /* check that sampler points to shader blob */
    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x2E];
    ok(sampler_orig == samplers[0], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[0], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x33];
    ok(sampler_orig == samplers[1], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[1], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x38];
    ok(sampler_orig == samplers[2], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[2], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x3D];
    ok(sampler_orig == samplers[3], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[3], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x42];
    ok(sampler_orig == samplers[4], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[4], sampler_orig);

    ok(!strcmp(samplers[5], "dummy"), "D3DXGetShaderSamplers failed, got \"%s\", expected \"%s\"\n", samplers[5], "dummy");

    /* reset samplers */
    memset(samplers, 0, sizeof(samplers));
    samplers[5] = "dummy";

    hr = D3DXGetShaderSamplers(get_shader_samplers_blob, NULL, &count);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);
    ok(count == 5, "D3DXGetShaderSamplers failed, got %u, expected %u\n", count, 5);

    hr = D3DXGetShaderSamplers(get_shader_samplers_blob, samplers, &count);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);
    ok(count == 5, "D3DXGetShaderSamplers failed, got %u, expected %u\n", count, 5);

    /* check that sampler points to shader blob */
    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x2E];
    ok(sampler_orig == samplers[0], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[0], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x33];
    ok(sampler_orig == samplers[1], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[1], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x38];
    ok(sampler_orig == samplers[2], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[2], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x3D];
    ok(sampler_orig == samplers[3], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[3], sampler_orig);

    sampler_orig = (LPCSTR)&get_shader_samplers_blob[0x42];
    ok(sampler_orig == samplers[4], "D3DXGetShaderSamplers failed, got %p, expected %p\n", samplers[4], sampler_orig);

    ok(!strcmp(samplers[5], "dummy"), "D3DXGetShaderSamplers failed, got \"%s\", expected \"%s\"\n", samplers[5], "dummy");

    /* check without ctab */
    hr = D3DXGetShaderSamplers(simple_vs, samplers, &count);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);
    ok(count == 0, "D3DXGetShaderSamplers failed, got %u, expected %u\n", count, 0);

    /* check invalid ctab */
    hr = D3DXGetShaderSamplers(shader_with_invalid_ctab, samplers, &count);
    ok(hr == D3D_OK, "D3DXGetShaderSamplers failed, got %x, expected %x\n", hr, D3D_OK);
    ok(count == 0, "D3DXGetShaderSamplers failed, got %u, expected %u\n", count, 0);
}

START_TEST(shader)
{
    test_get_shader_size();
    test_get_shader_version();
    test_find_shader_comment();
    test_get_shader_constant_table_ex();
    test_constant_tables();
    test_setting_constants();
    test_get_sampler_index();
    test_get_shader_samplers();
}
