/*
 * Copyright (C) 2008 Stefan Dösinger
 * Copyright (C) 2009 Matteo Bruni
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
#define CONST_VTABLE
#include "wine/test.h"

#include <d3dx9.h>

#include "resources.h"

static HRESULT create_file(const char *filename, const char *data, const unsigned int size)
{
    DWORD received;
    HANDLE hfile;

    hfile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if(hfile == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());

    if(WriteFile(hfile, data, size, &received, NULL))
    {
        CloseHandle(hfile);
        return D3D_OK;
    }

    CloseHandle(hfile);
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI testD3DXInclude_open(ID3DXInclude *iface,
                                           D3DXINCLUDE_TYPE include_type,
                                           LPCSTR filename, LPCVOID parent_data,
                                           LPCVOID *data, UINT *bytes) {
    char *buffer;
    const char include[] = "#define REGISTER r0\nvs.1.1\n";
    const char include2[] = "#include \"incl3.vsh\"\n";
    const char include3[] = "vs.1.1\n";

    trace("filename = %s\n", filename);
    trace("parent_data (%p) -> %s\n", parent_data, parent_data ? (char *)parent_data : "(null)");

    if(!strcmp(filename,"incl.vsh")) {
        buffer = HeapAlloc(GetProcessHeap(), 0, sizeof(include));
        CopyMemory(buffer, include, sizeof(include));
        *bytes = sizeof(include);
        /* Also check for the correct parent_data content */
        ok(parent_data == NULL, "wrong parent_data value\n");
    }
    else if(!strcmp(filename,"incl3.vsh")) {
        buffer = HeapAlloc(GetProcessHeap(), 0, sizeof(include3));
        CopyMemory(buffer, include3, sizeof(include3));
        *bytes = sizeof(include3);
        /* Also check for the correct parent_data content */
        ok(parent_data != NULL && !strncmp(include2, parent_data, strlen(include2)), "wrong parent_data value\n");
    }
    else {
        buffer = HeapAlloc(GetProcessHeap(), 0, sizeof(include2));
        CopyMemory(buffer, include2, sizeof(include2));
        *bytes = sizeof(include2);
    }
    *data = buffer;
    return S_OK;
}

static HRESULT WINAPI testD3DXInclude_close(ID3DXInclude *iface, LPCVOID data) {
    HeapFree(GetProcessHeap(), 0, (LPVOID)data);
    return S_OK;
}

static const struct ID3DXIncludeVtbl D3DXInclude_Vtbl = {
    testD3DXInclude_open,
    testD3DXInclude_close
};

struct D3DXIncludeImpl {
    ID3DXInclude ID3DXInclude_iface;
};

static void assembleshader_test(void) {
    const char test1[] = {
        "vs.1.1\n"
        "mov DEF2, v0\n"
    };
    const char testincl[] = {
        "#define REGISTER r0\n"
        "vs.1.1\n"
    };
    const char testshader[] = {
        "#include \"incl.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    const char testshader2[] = {
        "#include \"incl2.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    const char testshader3[] = {
        "#include \"include/incl3.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    const char testincl3[] = {
        "#include \"incl4.vsh\"\n"
    };
    const char testincl4_ok[] = {
        "#define REGISTER r0\n"
        "vs.1.1\n"
    };
    const char testincl4_wrong[] = {
        "#error \"wrong include\"\n"
    };
    HRESULT hr;
    LPD3DXBUFFER shader, messages;
    D3DXMACRO defines[] = {
        {
            "DEF1", "10 + 15"
        },
        {
            "DEF2", "r0"
        },
        {
            NULL, NULL
        }
    };
    struct D3DXIncludeImpl include;
    HRESULT shader_vsh_res;

    /* pDefines test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(test1, strlen(test1),
                            defines, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3D_OK, "pDefines test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* NULL messages test */
    shader = NULL;
    hr = D3DXAssembleShader(test1, strlen(test1),
                            defines, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, NULL);
    ok(hr == D3D_OK, "NULL messages test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(shader) ID3DXBuffer_Release(shader);

    /* NULL shader test */
    messages = NULL;
    hr = D3DXAssembleShader(test1, strlen(test1),
                            defines, NULL, D3DXSHADER_SKIPVALIDATION,
                            NULL, &messages);
    ok(hr == D3D_OK, "NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }

    /* pInclude test */
    shader = NULL;
    messages = NULL;
    include.ID3DXInclude_iface.lpVtbl = &D3DXInclude_Vtbl;
    hr = D3DXAssembleShader(testshader, strlen(testshader), NULL, &include.ID3DXInclude_iface,
                            D3DXSHADER_SKIPVALIDATION, &shader, &messages);
    ok(hr == D3D_OK, "pInclude test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* "unexpected #include file from memory" test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(testshader, strlen(testshader),
                            NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "D3DXAssembleShader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* recursive #include test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(testshader2, strlen(testshader2), NULL, &include.ID3DXInclude_iface,
                            D3DXSHADER_SKIPVALIDATION, &shader, &messages);
    ok(hr == D3D_OK, "D3DXAssembleShader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("recursive D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    shader_vsh_res = create_file("shader.vsh", testshader, sizeof(testshader) - 1);
    if(SUCCEEDED(shader_vsh_res)) {
        create_file("incl.vsh", testincl, sizeof(testincl) - 1);

        /* D3DXAssembleShaderFromFile + #include test */
        shader = NULL;
        messages = NULL;
        hr = D3DXAssembleShaderFromFileA("shader.vsh",
                                         NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
        ok(hr == D3D_OK, "D3DXAssembleShaderFromFile test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) ID3DXBuffer_Release(shader);

        /* D3DXAssembleShaderFromFile + pInclude test */
        shader = NULL;
        messages = NULL;
        hr = D3DXAssembleShaderFromFileA("shader.vsh", NULL, &include.ID3DXInclude_iface,
                                         D3DXSHADER_SKIPVALIDATION, &shader, &messages);
        ok(hr == D3D_OK, "D3DXAssembleShaderFromFile + pInclude test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) ID3DXBuffer_Release(shader);

        create_file("shader3.vsh", testshader3, sizeof(testshader3) - 1);
        create_file("incl4.vsh", testincl4_wrong, sizeof(testincl4_wrong) - 1);
        if(CreateDirectoryA("include", NULL)) {
            create_file("include/incl3.vsh", testincl3, sizeof(testincl3) - 1);
            create_file("include/incl4.vsh", testincl4_ok, sizeof(testincl4_ok) - 1);

            /* path search #include test */
            shader = NULL;
            messages = NULL;
            hr = D3DXAssembleShaderFromFileA("shader3.vsh", NULL, NULL,
                                             D3DXSHADER_SKIPVALIDATION,
                                             &shader, &messages);
            ok(hr == D3D_OK, "D3DXAssembleShaderFromFile path search test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
            if(messages) {
                trace("D3DXAssembleShaderFromFile path search messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
                ID3DXBuffer_Release(messages);
            }
            if(shader) ID3DXBuffer_Release(shader);
        } else skip("Couldn't create \"include\" directory\n");
    } else skip("Couldn't create \"shader.vsh\"\n");

    /* NULL shader tests */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(NULL, 0,
                            NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromFileA("nonexistent.vsh",
                                     NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                     &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA || hr == E_FAIL, /* I get this on WinXP */
        "D3DXAssembleShaderFromFile nonexistent file test failed with error 0x%x - %d\n",
        hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromFile messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* D3DXAssembleShaderFromResource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromResourceA(NULL, MAKEINTRESOURCEA(IDB_ASMSHADER),
                                         NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
    ok(hr == D3D_OK, "D3DXAssembleShaderFromResource test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* D3DXAssembleShaderFromResource with missing shader resource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromResourceA(NULL, "notexisting",
                                         NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "D3DXAssembleShaderFromResource NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* cleanup */
    if(SUCCEEDED(shader_vsh_res)) {
        DeleteFileA("shader.vsh");
        DeleteFileA("incl.vsh");
        DeleteFileA("shader3.vsh");
        DeleteFileA("incl4.vsh");
        DeleteFileA("include/incl3.vsh");
        DeleteFileA("include/incl4.vsh");
        RemoveDirectoryA("include");
    }
}

static void d3dxpreprocess_test(void) {
    const char testincl[] = {
        "#define REGISTER r0\n"
        "vs.1.1\n"
    };
    const char testshader[] = {
        "#include \"incl.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    const char testshader3[] = {
        "#include \"include/incl3.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    const char testincl3[] = {
        "#include \"incl4.vsh\"\n"
    };
    const char testincl4_ok[] = {
        "#define REGISTER r0\n"
        "vs.1.1\n"
    };
    const char testincl4_wrong[] = {
        "#error \"wrong include\"\n"
    };
    HRESULT hr;
    LPD3DXBUFFER shader, messages;
    HRESULT shader_vsh_res;
    struct D3DXIncludeImpl include = {{&D3DXInclude_Vtbl}};

    shader_vsh_res = create_file("shader.vsh", testshader, sizeof(testshader) - 1);
    if(SUCCEEDED(shader_vsh_res)) {
        create_file("incl.vsh", testincl, sizeof(testincl) - 1);
        create_file("shader3.vsh", testshader3, sizeof(testshader3) - 1);
        create_file("incl4.vsh", testincl4_wrong, sizeof(testincl4_wrong) - 1);
        if(CreateDirectoryA("include", NULL)) {
            create_file("include/incl3.vsh", testincl3, sizeof(testincl3) - 1);
            create_file("include/incl4.vsh", testincl4_ok, sizeof(testincl4_ok) - 1);

            /* path search #include test */
            shader = NULL;
            messages = NULL;
            hr = D3DXPreprocessShaderFromFileA("shader3.vsh", NULL, NULL,
                                               &shader, &messages);
            ok(hr == D3D_OK, "D3DXPreprocessShaderFromFile path search test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
            if(messages) {
                trace("D3DXPreprocessShaderFromFile path search messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
                ID3DXBuffer_Release(messages);
            }
            if(shader) ID3DXBuffer_Release(shader);
        } else skip("Couldn't create \"include\" directory\n");

        /* D3DXPreprocessShaderFromFile + #include test */
        shader = NULL;
        messages = NULL;
        hr = D3DXPreprocessShaderFromFileA("shader.vsh",
                                           NULL, NULL,
                                           &shader, &messages);
        ok(hr == D3D_OK, "D3DXPreprocessShaderFromFile test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXPreprocessShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) ID3DXBuffer_Release(shader);

        /* D3DXPreprocessShaderFromFile + pInclude test */
        shader = NULL;
        messages = NULL;
        hr = D3DXPreprocessShaderFromFileA("shader.vsh", NULL, &include.ID3DXInclude_iface,
                                           &shader, &messages);
        ok(hr == D3D_OK, "D3DXPreprocessShaderFromFile + pInclude test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXPreprocessShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) ID3DXBuffer_Release(shader);
    } else skip("Couldn't create \"shader.vsh\"\n");

    /* NULL shader tests */
    shader = NULL;
    messages = NULL;
    hr = D3DXPreprocessShaderFromFileA("nonexistent.vsh",
                                       NULL, NULL,
                                       &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA || hr == E_FAIL, /* I get this on WinXP */
        "D3DXPreprocessShaderFromFile nonexistent file test failed with error 0x%x - %d\n",
        hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXPreprocessShaderFromFile messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* D3DXPreprocessShaderFromResource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXPreprocessShaderFromResourceA(NULL, MAKEINTRESOURCEA(IDB_ASMSHADER),
                                           NULL, NULL,
                                           &shader, &messages);
    ok(hr == D3D_OK, "D3DXPreprocessShaderFromResource test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXPreprocessShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* D3DXPreprocessShaderFromResource with missing shader resource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXPreprocessShaderFromResourceA(NULL, "notexisting",
                                           NULL, NULL,
                                           &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "D3DXPreprocessShaderFromResource NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXPreprocessShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* cleanup */
    if(SUCCEEDED(shader_vsh_res)) {
        DeleteFileA("shader.vsh");
        DeleteFileA("incl.vsh");
        DeleteFileA("shader3.vsh");
        DeleteFileA("incl4.vsh");
        DeleteFileA("include/incl3.vsh");
        DeleteFileA("include/incl4.vsh");
        RemoveDirectoryA("include");
    }
}

START_TEST(asm)
{
    assembleshader_test();

    d3dxpreprocess_test();
}
