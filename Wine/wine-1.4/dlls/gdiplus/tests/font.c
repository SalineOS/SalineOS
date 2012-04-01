/*
 * Unit test suite for fonts
 *
 * Copyright (C) 2007 Google (Evan Stade)
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

#include <math.h>

#include "windows.h"
#include "gdiplus.h"
#include "wine/test.h"

#define expect(expected, got) ok(got == expected, "Expected %.8x, got %.8x\n", expected, got)
#define expectf(expected, got) ok(fabs(expected - got) < 0.0001, "Expected %.2f, got %.2f\n", expected, got)

static const WCHAR arial[] = {'A','r','i','a','l','\0'};
static const WCHAR nonexistent[] = {'T','h','i','s','F','o','n','t','s','h','o','u','l','d','N','o','t','E','x','i','s','t','\0'};
static const WCHAR MSSansSerif[] = {'M','S',' ','S','a','n','s',' ','S','e','r','i','f','\0'};
static const WCHAR MicrosoftSansSerif[] = {'M','i','c','r','o','s','o','f','t',' ','S','a','n','s',' ','S','e','r','i','f','\0'};
static const WCHAR TimesNewRoman[] = {'T','i','m','e','s',' ','N','e','w',' ','R','o','m','a','n','\0'};
static const WCHAR CourierNew[] = {'C','o','u','r','i','e','r',' ','N','e','w','\0'};
static const WCHAR Tahoma[] = {'T','a','h','o','m','a',0};
static const WCHAR LiberationSerif[] = {'L','i','b','e','r','a','t','i','o','n',' ','S','e','r','i','f',0};

static void test_createfont(void)
{
    GpFontFamily* fontfamily = NULL, *fontfamily2;
    GpFont* font = NULL;
    GpStatus stat;
    Unit unit;
    UINT i;
    REAL size;
    WCHAR familyname[LF_FACESIZE];

    stat = GdipCreateFontFamilyFromName(nonexistent, NULL, &fontfamily);
    expect (FontFamilyNotFound, stat);
    stat = GdipDeleteFont(font);
    expect (InvalidParameter, stat);
    stat = GdipCreateFontFamilyFromName(arial, NULL, &fontfamily);
    if(stat == FontFamilyNotFound)
    {
        skip("Arial not installed\n");
        return;
    }
    expect (Ok, stat);
    stat = GdipCreateFont(fontfamily, 12, FontStyleRegular, UnitPoint, &font);
    expect (Ok, stat);
    stat = GdipGetFontUnit (font, &unit);
    expect (Ok, stat);
    expect (UnitPoint, unit);

    stat = GdipGetFamily(font, &fontfamily2);
    expect(Ok, stat);
    stat = GdipGetFamilyName(fontfamily2, familyname, 0);
    expect(Ok, stat);
    ok (lstrcmpiW(arial, familyname) == 0, "Expected arial, got %s\n",
            wine_dbgstr_w(familyname));
    stat = GdipDeleteFontFamily(fontfamily2);
    expect(Ok, stat);

    /* Test to see if returned size is based on unit (its not) */
    GdipGetFontSize(font, &size);
    ok (size == 12, "Expected 12, got %f\n", size);
    GdipDeleteFont(font);

    /* Make sure everything is converted correctly for all Units */
    for (i = UnitWorld; i <=UnitMillimeter; i++)
    {
        if (i == UnitDisplay) continue; /* Crashes WindowsXP, wtf? */
        GdipCreateFont(fontfamily, 24, FontStyleRegular, i, &font);
        GdipGetFontSize (font, &size);
        ok (size == 24, "Expected 24, got %f (with unit: %d)\n", size, i);
        GdipGetFontUnit (font, &unit);
        expect (i, unit);
        GdipDeleteFont(font);
    }

    GdipDeleteFontFamily(fontfamily);
}

static void test_logfont(void)
{
    LOGFONTA lfa, lfa2;
    GpFont *font;
    GpStatus stat;
    GpGraphics *graphics;
    HDC hdc = GetDC(0);
    INT style;

    GdipCreateFromHDC(hdc, &graphics);
    memset(&lfa, 0, sizeof(LOGFONTA));
    memset(&lfa2, 0xff, sizeof(LOGFONTA));

    /* empty FaceName */
    lfa.lfFaceName[0] = 0;
    stat = GdipCreateFontFromLogfontA(hdc, &lfa, &font);
    expect(NotTrueTypeFont, stat);

    lstrcpyA(lfa.lfFaceName, "Arial");

    stat = GdipCreateFontFromLogfontA(hdc, &lfa, &font);
    if (stat == FileNotFound)
    {
        skip("Arial not installed.\n");
        return;
    }
    expect(Ok, stat);
    stat = GdipGetLogFontA(font, graphics, &lfa2);
    expect(Ok, stat);

    ok(lfa2.lfHeight < 0, "Expected negative height\n");
    expect(0, lfa2.lfWidth);
    expect(0, lfa2.lfEscapement);
    expect(0, lfa2.lfOrientation);
    ok((lfa2.lfWeight >= 100) && (lfa2.lfWeight <= 900), "Expected weight to be set\n");
    expect(0, lfa2.lfItalic);
    expect(0, lfa2.lfUnderline);
    expect(0, lfa2.lfStrikeOut);
    ok(lfa2.lfCharSet == GetTextCharset(hdc) || lfa2.lfCharSet == ANSI_CHARSET,
        "Expected %x or %x, got %x\n", GetTextCharset(hdc), ANSI_CHARSET, lfa2.lfCharSet);
    expect(0, lfa2.lfOutPrecision);
    expect(0, lfa2.lfClipPrecision);
    expect(0, lfa2.lfQuality);
    expect(0, lfa2.lfPitchAndFamily);

    GdipDeleteFont(font);

    memset(&lfa, 0, sizeof(LOGFONTA));
    lfa.lfHeight = 25;
    lfa.lfWidth = 25;
    lfa.lfEscapement = lfa.lfOrientation = 50;
    lfa.lfItalic = lfa.lfUnderline = lfa.lfStrikeOut = TRUE;

    memset(&lfa2, 0xff, sizeof(LOGFONTA));
    lstrcpyA(lfa.lfFaceName, "Arial");

    stat = GdipCreateFontFromLogfontA(hdc, &lfa, &font);
    expect(Ok, stat);
    stat = GdipGetLogFontA(font, graphics, &lfa2);
    expect(Ok, stat);

    ok(lfa2.lfHeight < 0, "Expected negative height\n");
    expect(0, lfa2.lfWidth);
    expect(0, lfa2.lfEscapement);
    expect(0, lfa2.lfOrientation);
    ok((lfa2.lfWeight >= 100) && (lfa2.lfWeight <= 900), "Expected weight to be set\n");
    expect(TRUE, lfa2.lfItalic);
    expect(TRUE, lfa2.lfUnderline);
    expect(TRUE, lfa2.lfStrikeOut);
    ok(lfa2.lfCharSet == GetTextCharset(hdc) || lfa2.lfCharSet == ANSI_CHARSET,
        "Expected %x or %x, got %x\n", GetTextCharset(hdc), ANSI_CHARSET, lfa2.lfCharSet);
    expect(0, lfa2.lfOutPrecision);
    expect(0, lfa2.lfClipPrecision);
    expect(0, lfa2.lfQuality);
    expect(0, lfa2.lfPitchAndFamily);

    stat = GdipGetFontStyle(font, &style);
    expect(Ok, stat);
    ok (style == (FontStyleItalic | FontStyleUnderline | FontStyleStrikeout),
            "Expected , got %d\n", style);

    GdipDeleteFont(font);

    GdipDeleteGraphics(graphics);
    ReleaseDC(0, hdc);
}

static void test_fontfamily (void)
{
    GpFontFamily *family, *clonedFontFamily;
    WCHAR itsName[LF_FACESIZE];
    GpStatus stat;

    /* FontFamily cannot be NULL */
    stat = GdipCreateFontFamilyFromName (arial , NULL, NULL);
    expect (InvalidParameter, stat);

    /* FontFamily must be able to actually find the family.
     * If it can't, any subsequent calls should fail.
     */
    stat = GdipCreateFontFamilyFromName (nonexistent, NULL, &family);
    expect (FontFamilyNotFound, stat);

    /* Bitmap fonts are not found */
    stat = GdipCreateFontFamilyFromName (MSSansSerif, NULL, &family);
    expect (FontFamilyNotFound, stat);
    if(stat == Ok) GdipDeleteFontFamily(family);

    stat = GdipCreateFontFamilyFromName (arial, NULL, &family);
    if(stat == FontFamilyNotFound)
    {
        skip("Arial not installed\n");
        return;
    }
    expect (Ok, stat);

    stat = GdipGetFamilyName (family, itsName, LANG_NEUTRAL);
    expect (Ok, stat);
    expect (0, lstrcmpiW(itsName, arial));

    if (0)
    {
        /* Crashes on Windows XP SP2, Vista, and so Wine as well */
        stat = GdipGetFamilyName (family, NULL, LANG_NEUTRAL);
        expect (Ok, stat);
    }

    /* Make sure we don't read old data */
    ZeroMemory (itsName, sizeof(itsName));
    stat = GdipCloneFontFamily(family, &clonedFontFamily);
    expect (Ok, stat);
    GdipDeleteFontFamily(family);
    stat = GdipGetFamilyName(clonedFontFamily, itsName, LANG_NEUTRAL);
    expect(Ok, stat);
    expect(0, lstrcmpiW(itsName, arial));

    GdipDeleteFontFamily(clonedFontFamily);
}

static void test_fontfamily_properties (void)
{
    GpFontFamily* FontFamily = NULL;
    GpStatus stat;
    UINT16 result = 0;

    stat = GdipCreateFontFamilyFromName(arial, NULL, &FontFamily);
    if(stat == FontFamilyNotFound)
        skip("Arial not installed\n");
    else
    {
        stat = GdipGetLineSpacing(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok (result == 2355, "Expected 2355, got %d\n", result);
        result = 0;
        stat = GdipGetEmHeight(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 2048, "Expected 2048, got %d\n", result);
        result = 0;
        stat = GdipGetCellAscent(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 1854, "Expected 1854, got %d\n", result);
        result = 0;
        stat = GdipGetCellDescent(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 434, "Expected 434, got %d\n", result);
        GdipDeleteFontFamily(FontFamily);
    }

    stat = GdipCreateFontFamilyFromName(TimesNewRoman, NULL, &FontFamily);
    if(stat == FontFamilyNotFound)
        skip("Times New Roman not installed\n");
    else
    {
        result = 0;
        stat = GdipGetLineSpacing(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 2355, "Expected 2355, got %d\n", result);
        result = 0;
        stat = GdipGetEmHeight(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 2048, "Expected 2048, got %d\n", result);
        result = 0;
        stat = GdipGetCellAscent(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 1825, "Expected 1825, got %d\n", result);
        result = 0;
        stat = GdipGetCellDescent(FontFamily, FontStyleRegular, &result);
        expect(Ok, stat);
        ok(result == 443, "Expected 443 got %d\n", result);
        GdipDeleteFontFamily(FontFamily);
    }
}

static void check_family(const char* context, GpFontFamily *family, WCHAR *name)
{
    GpStatus stat;
    GpFont* font;

    *name = 0;
    stat = GdipGetFamilyName(family, name, LANG_NEUTRAL);
    ok(stat == Ok, "could not get the %s family name: %.8x\n", context, stat);

    stat = GdipCreateFont(family, 12, FontStyleRegular, UnitPixel, &font);
    ok(stat == Ok, "could not create a font for the %s family: %.8x\n", context, stat);
    if (stat == Ok)
    {
        stat = GdipDeleteFont(font);
        ok(stat == Ok, "could not delete the %s family font: %.8x\n", context, stat);
    }

    stat = GdipDeleteFontFamily(family);
    ok(stat == Ok, "could not delete the %s family: %.8x\n", context, stat);
}

static void test_getgenerics (void)
{
    GpStatus stat;
    GpFontFamily *family;
    WCHAR sansname[LF_FACESIZE], serifname[LF_FACESIZE], mononame[LF_FACESIZE];
    int missingfonts = 0;

    stat = GdipGetGenericFontFamilySansSerif(&family);
    expect (Ok, stat);
    if (stat == FontFamilyNotFound)
        missingfonts = 1;
    else
        check_family("Sans Serif", family, sansname);

    stat = GdipGetGenericFontFamilySerif(&family);
    expect (Ok, stat);
    if (stat == FontFamilyNotFound)
        missingfonts = 1;
    else
        check_family("Serif", family, serifname);

    stat = GdipGetGenericFontFamilyMonospace(&family);
    expect (Ok, stat);
    if (stat == FontFamilyNotFound)
        missingfonts = 1;
    else
        check_family("Monospace", family, mononame);

    if (missingfonts && strcmp(winetest_platform, "wine") == 0)
        trace("You may need to install either the Microsoft Web Fonts or the Liberation Fonts\n");

    /* Check that the family names are all different */
    ok(lstrcmpiW(sansname, serifname) != 0, "Sans Serif and Serif families should be different: %s\n", wine_dbgstr_w(sansname));
    ok(lstrcmpiW(sansname, mononame) != 0, "Sans Serif and Monospace families should be different: %s\n", wine_dbgstr_w(sansname));
    ok(lstrcmpiW(serifname, mononame) != 0, "Serif and Monospace families should be different: %s\n", wine_dbgstr_w(serifname));
}

static void test_installedfonts (void)
{
    GpStatus stat;
    GpFontCollection* collection=NULL;

    stat = GdipNewInstalledFontCollection(NULL);
    expect (InvalidParameter, stat);

    stat = GdipNewInstalledFontCollection(&collection);
    expect (Ok, stat);
    ok (collection != NULL, "got NULL font collection\n");
}

static void test_heightgivendpi(void)
{
    GpStatus stat;
    GpFont* font = NULL;
    GpFontFamily* fontfamily = NULL;
    REAL height;

    stat = GdipCreateFontFamilyFromName(arial, NULL, &fontfamily);
    if(stat == FontFamilyNotFound)
    {
        skip("Arial not installed\n");
        return;
    }
    expect(Ok, stat);

    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitPixel, &font);
    expect(Ok, stat);

    stat = GdipGetFontHeightGivenDPI(NULL, 96, &height);
    expect(InvalidParameter, stat);

    stat = GdipGetFontHeightGivenDPI(font, 96, NULL);
    expect(InvalidParameter, stat);

    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)34.497070, height);
    GdipDeleteFont(font);

    height = 12345;
    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitWorld, &font);
    expect(Ok, stat);
    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)34.497070, height);
    GdipDeleteFont(font);

    height = 12345;
    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitPoint, &font);
    expect(Ok, stat);
    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)45.996094, height);
    GdipDeleteFont(font);

    height = 12345;
    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitInch, &font);
    expect(Ok, stat);
    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)3311.718750, height);
    GdipDeleteFont(font);

    height = 12345;
    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitDocument, &font);
    expect(Ok, stat);
    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)11.039062, height);
    GdipDeleteFont(font);

    height = 12345;
    stat = GdipCreateFont(fontfamily, 30, FontStyleRegular, UnitMillimeter, &font);
    expect(Ok, stat);
    stat = GdipGetFontHeightGivenDPI(font, 96, &height);
    expect(Ok, stat);
    expectf((REAL)130.382614, height);
    GdipDeleteFont(font);

    GdipDeleteFontFamily(fontfamily);
}

START_TEST(font)
{
    struct GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;

    gdiplusStartupInput.GdiplusVersion              = 1;
    gdiplusStartupInput.DebugEventCallback          = NULL;
    gdiplusStartupInput.SuppressBackgroundThread    = 0;
    gdiplusStartupInput.SuppressExternalCodecs      = 0;

    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    test_createfont();
    test_logfont();
    test_fontfamily();
    test_fontfamily_properties();
    test_getgenerics();
    test_installedfonts();
    test_heightgivendpi();

    GdiplusShutdown(gdiplusToken);
}
