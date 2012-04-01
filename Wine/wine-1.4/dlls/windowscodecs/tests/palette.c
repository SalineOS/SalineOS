/*
 * Copyright 2009 Vincent Povirk for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "objbase.h"
#include "wincodec.h"
#include "wine/test.h"

static void test_custom_palette(void)
{
    IWICImagingFactory *factory;
    IWICPalette *palette;
    HRESULT hr;
    WICBitmapPaletteType type=0xffffffff;
    UINT count=1;
    const WICColor initcolors[4]={0xff000000,0xff0000ff,0xffffff00,0xffffffff};
    WICColor colors[4];
    BOOL boolresult;

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory, (void**)&factory);
    ok(SUCCEEDED(hr), "CoCreateInstance failed, hr=%x\n", hr);
    if (FAILED(hr)) return;

    hr = IWICImagingFactory_CreatePalette(factory, &palette);
    ok(SUCCEEDED(hr), "CreatePalette failed, hr=%x\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IWICPalette_GetType(palette, &type);
        ok(SUCCEEDED(hr), "GetType failed, hr=%x\n", hr);
        ok(type == WICBitmapPaletteTypeCustom, "expected WICBitmapPaletteTypeCustom, got %x\n", type);

        hr = IWICPalette_GetColorCount(palette, &count);
        ok(SUCCEEDED(hr), "GetColorCount failed, hr=%x\n", hr);
        ok(count == 0, "expected 0, got %u\n", count);

        hr = IWICPalette_GetColors(palette, 0, colors, &count);
        ok(SUCCEEDED(hr), "GetColors failed, hr=%x\n", hr);
        ok(count == 0, "expected 0, got %u\n", count);

        hr = IWICPalette_GetColors(palette, 4, colors, &count);
        ok(SUCCEEDED(hr), "GetColors failed, hr=%x\n", hr);
        ok(count == 0, "expected 0, got %u\n", count);

        memcpy(colors, initcolors, sizeof(initcolors));
        hr = IWICPalette_InitializeCustom(palette, colors, 4);
        ok(SUCCEEDED(hr), "InitializeCustom failed, hr=%x\n", hr);

        hr = IWICPalette_GetType(palette, &type);
        ok(SUCCEEDED(hr), "GetType failed, hr=%x\n", hr);
        ok(type == WICBitmapPaletteTypeCustom, "expected WICBitmapPaletteTypeCustom, got %x\n", type);

        hr = IWICPalette_GetColorCount(palette, &count);
        ok(SUCCEEDED(hr), "GetColorCount failed, hr=%x\n", hr);
        ok(count == 4, "expected 4, got %u\n", count);

        memset(colors, 0, sizeof(colors));
        count = 0;
        hr = IWICPalette_GetColors(palette, 4, colors, &count);
        ok(SUCCEEDED(hr), "GetColors failed, hr=%x\n", hr);
        ok(count == 4, "expected 4, got %u\n", count);
        ok(!memcmp(colors, initcolors, sizeof(colors)), "got unexpected palette data\n");

        memset(colors, 0, sizeof(colors));
        count = 0;
        hr = IWICPalette_GetColors(palette, 2, colors, &count);
        ok(SUCCEEDED(hr), "GetColors failed, hr=%x\n", hr);
        ok(count == 2, "expected 2, got %u\n", count);
        ok(!memcmp(colors, initcolors, sizeof(WICColor)*2), "got unexpected palette data\n");

        count = 0;
        hr = IWICPalette_GetColors(palette, 6, colors, &count);
        ok(SUCCEEDED(hr), "GetColors failed, hr=%x\n", hr);
        ok(count == 4, "expected 4, got %u\n", count);

        hr = IWICPalette_HasAlpha(palette, &boolresult);
        ok(SUCCEEDED(hr), "HasAlpha failed, hr=%x\n", hr);
        ok(!boolresult, "expected FALSE, got TRUE\n");

        hr = IWICPalette_IsBlackWhite(palette, &boolresult);
        ok(SUCCEEDED(hr), "IsBlackWhite failed, hr=%x\n", hr);
        ok(!boolresult, "expected FALSE, got TRUE\n");

        hr = IWICPalette_IsGrayscale(palette, &boolresult);
        ok(SUCCEEDED(hr), "IsGrayscale failed, hr=%x\n", hr);
        ok(!boolresult, "expected FALSE, got TRUE\n");

        /* try a palette with some alpha in it */
        colors[2] = 0x80ffffff;
        hr = IWICPalette_InitializeCustom(palette, colors, 4);
        ok(SUCCEEDED(hr), "InitializeCustom failed, hr=%x\n", hr);

        hr = IWICPalette_HasAlpha(palette, &boolresult);
        ok(SUCCEEDED(hr), "HasAlpha failed, hr=%x\n", hr);
        ok(boolresult, "expected TRUE, got FALSE\n");

        /* setting to a 0-color palette is acceptable */
        hr = IWICPalette_InitializeCustom(palette, NULL, 0);
        ok(SUCCEEDED(hr), "InitializeCustom failed, hr=%x\n", hr);

        /* IWICPalette is paranoid about NULL pointers */
        hr = IWICPalette_GetType(palette, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_GetColorCount(palette, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_InitializeCustom(palette, NULL, 4);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_GetColors(palette, 4, NULL, &count);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_GetColors(palette, 4, colors, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_HasAlpha(palette, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_IsBlackWhite(palette, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        hr = IWICPalette_IsGrayscale(palette, NULL);
        ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %x\n", hr);

        IWICPalette_Release(palette);
    }

    IWICImagingFactory_Release(factory);
}

START_TEST(palette)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    test_custom_palette();

    CoUninitialize();
}
