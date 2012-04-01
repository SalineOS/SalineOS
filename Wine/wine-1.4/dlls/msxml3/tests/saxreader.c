/*
 * XML test
 *
 * Copyright 2008 Piotr Caban
 * Copyright 2011 Thomas Mullaly
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

#include <stdio.h>
#include <assert.h>

#include "windows.h"
#include "ole2.h"
#include "msxml2.h"
#include "msxml2did.h"
#include "ocidl.h"
#include "dispex.h"

#include "wine/test.h"

#define EXPECT_HR(hr,hr_exp) \
    ok(hr == hr_exp, "got 0x%08x, expected 0x%08x\n", hr, hr_exp)

#define EXPECT_REF(obj,ref) _expect_ref((IUnknown*)obj, ref, __LINE__)
static void _expect_ref(IUnknown* obj, ULONG ref, int line)
{
    ULONG rc = IUnknown_AddRef(obj);
    IUnknown_Release(obj);
    ok_(__FILE__,line)(rc-1 == ref, "expected refcount %d, got %d\n", ref, rc-1);
}

static BSTR alloc_str_from_narrow(const char *str)
{
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    BSTR ret = SysAllocStringLen(NULL, len - 1);  /* NUL character added automatically */
    MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    return ret;
}

static BSTR alloced_bstrs[512];
static int alloced_bstrs_count;

static BSTR _bstr_(const char *str)
{
    assert(alloced_bstrs_count < sizeof(alloced_bstrs)/sizeof(alloced_bstrs[0]));
    alloced_bstrs[alloced_bstrs_count] = alloc_str_from_narrow(str);
    return alloced_bstrs[alloced_bstrs_count++];
}

static void free_bstrs(void)
{
    int i;
    for (i = 0; i < alloced_bstrs_count; i++)
        SysFreeString(alloced_bstrs[i]);
    alloced_bstrs_count = 0;
}

typedef enum _CH {
    CH_ENDTEST,
    CH_PUTDOCUMENTLOCATOR,
    CH_STARTDOCUMENT,
    CH_ENDDOCUMENT,
    CH_STARTPREFIXMAPPING,
    CH_ENDPREFIXMAPPING,
    CH_STARTELEMENT,
    CH_ENDELEMENT,
    CH_CHARACTERS,
    CH_IGNORABLEWHITESPACE,
    CH_PROCESSINGINSTRUCTION,
    CH_SKIPPEDENTITY,
    EH_ERROR,
    EH_FATALERROR,
    EG_IGNORABLEWARNING
} CH;

static const WCHAR szSimpleXML[] = {
'<','?','x','m','l',' ','v','e','r','s','i','o','n','=','\"','1','.','0','\"',' ','?','>','\n',
'<','B','a','n','k','A','c','c','o','u','n','t','>','\n',
' ',' ',' ','<','N','u','m','b','e','r','>','1','2','3','4','<','/','N','u','m','b','e','r','>','\n',
' ',' ',' ','<','N','a','m','e','>','C','a','p','t','a','i','n',' ','A','h','a','b','<','/','N','a','m','e','>','\n',
'<','/','B','a','n','k','A','c','c','o','u','n','t','>','\n','\0'
};

static const WCHAR szCarriageRetTest[] = {
'<','?','x','m','l',' ','v','e','r','s','i','o','n','=','"','1','.','0','"','?','>','\r','\n',
'<','B','a','n','k','A','c','c','o','u','n','t','>','\r','\n',
'\t','<','N','u','m','b','e','r','>','1','2','3','4','<','/','N','u','m','b','e','r','>','\r','\n',
'\t','<','N','a','m','e','>','C','a','p','t','a','i','n',' ','A','h','a','b','<','/','N','a','m','e','>','\r','\n',
'<','/','B','a','n','k','A','c','c','o','u','n','t','>','\r','\n','\0'
};

static const WCHAR szUtf16XML[] = {
'<','?','x','m','l',' ','v','e','r','s','i','o','n','=','"','1','.','0','"',' ',
'e','n','c','o','d','i','n','g','=','"','U','T','F','-','1','6','"',' ',
's','t','a','n','d','a','l','o','n','e','=','"','n','o','"','?','>','\r','\n'
};

static const CHAR szUtf16BOM[] = {0xff, 0xfe};

static const CHAR szUtf8XML[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\r\n";

static const char utf8xml2[] =
"<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\r\n";

static const CHAR szTestXML[] =
"<?xml version=\"1.0\" ?>\n"
"<BankAccount>\n"
"   <Number>1234</Number>\n"
"   <Name>Captain Ahab</Name>\n"
"</BankAccount>\n";

static const CHAR szTestAttributes[] =
"<?xml version=\"1.0\" ?>\n"
"<document xmlns:test=\"prefix_test\" xmlns=\"prefix\" test:arg1=\"arg1\" arg2=\"arg2\" test:ar3=\"arg3\">\n"
"<node1 xmlns:p=\"test\" />"
"</document>\n";

typedef struct _contenthandlercheck {
    CH id;
    int line;
    int column;
    int line_v6;
    int column_v6;
    const char *arg1;
    const char *arg2;
    const char *arg3;
    HRESULT ret;
} content_handler_test;

static content_handler_test contentHandlerTest1[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0 },
    { CH_STARTDOCUMENT, 0, 0, 1, 22 },
    { CH_STARTELEMENT, 2, 14, 2, 13, "", "BankAccount", "BankAccount" },
    { CH_CHARACTERS, 2, 14, 3, 4, "\n   " },
    { CH_STARTELEMENT, 3, 12, 3, 11, "", "Number", "Number" },
    { CH_CHARACTERS, 3, 12, 3, 16, "1234" },
    { CH_ENDELEMENT, 3, 18, 3, 24, "", "Number", "Number" },
    { CH_CHARACTERS, 3, 25, 4, 4, "\n   " },
    { CH_STARTELEMENT, 4, 10, 4, 9, "", "Name", "Name" },
    { CH_CHARACTERS, 4, 10, 4, 22, "Captain Ahab" },
    { CH_ENDELEMENT, 4, 24, 4, 28, "", "Name", "Name" },
    { CH_CHARACTERS, 4, 29, 5, 1, "\n" },
    { CH_ENDELEMENT, 5, 3, 5, 14, "", "BankAccount", "BankAccount" },
    { CH_ENDDOCUMENT, 0, 0, 6, 0 },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTest2[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0 },
    { CH_STARTDOCUMENT, 0, 0, 1, 21 },
    { CH_STARTELEMENT, 2, 14, 2, 13, "", "BankAccount", "BankAccount" },
    { CH_CHARACTERS, 2, 14, 3, 0, "\n" },
    { CH_CHARACTERS, 2, 16, 3, 2, "\t" },
    { CH_STARTELEMENT, 3, 10, 3, 9, "", "Number", "Number" },
    { CH_CHARACTERS, 3, 10, 3, 14, "1234" },
    { CH_ENDELEMENT, 3, 16, 3, 22, "", "Number", "Number" },
    { CH_CHARACTERS, 3, 23, 4, 0, "\n" },
    { CH_CHARACTERS, 3, 25, 4, 2, "\t" },
    { CH_STARTELEMENT, 4, 8, 4, 7, "", "Name", "Name" },
    { CH_CHARACTERS, 4, 8, 4, 20, "Captain Ahab" },
    { CH_ENDELEMENT, 4, 22, 4, 26, "", "Name", "Name" },
    { CH_CHARACTERS, 4, 27, 5, 0, "\n" },
    { CH_ENDELEMENT, 5, 3, 5, 14, "", "BankAccount", "BankAccount" },
    { CH_ENDDOCUMENT, 0, 0, 6, 0 },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTestError[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0, NULL, NULL, NULL, E_FAIL },
    { EH_FATALERROR, 0, 0, 0, 0, NULL, NULL, NULL, E_FAIL },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTestCallbackResults[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0, NULL, NULL, NULL, S_FALSE },
    { CH_STARTDOCUMENT, 0, 0, 1, 22, NULL, NULL, NULL, S_FALSE },
    { EH_FATALERROR, 0, 0, 0, 0, NULL, NULL, NULL, S_FALSE },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTestCallbackResult6[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0, NULL, NULL, NULL, S_FALSE },
    { CH_STARTDOCUMENT, 0, 0, 1, 22, NULL, NULL, NULL, S_FALSE },
    { CH_STARTELEMENT, 2, 14, 2, 13, "", "BankAccount", "BankAccount", S_FALSE },
    { CH_CHARACTERS, 2, 14, 3, 4, "\n   ", NULL, NULL, S_FALSE },
    { CH_STARTELEMENT, 3, 12, 3, 11, "", "Number", "Number", S_FALSE },
    { CH_CHARACTERS, 3, 12, 3, 16, "1234", NULL, NULL, S_FALSE },
    { CH_ENDELEMENT, 3, 18, 3, 24, "", "Number", "Number", S_FALSE },
    { CH_CHARACTERS, 3, 25, 4, 4, "\n   ", NULL, NULL, S_FALSE },
    { CH_STARTELEMENT, 4, 10, 4, 9, "", "Name", "Name", S_FALSE },
    { CH_CHARACTERS, 4, 10, 4, 22, "Captain Ahab", NULL, NULL, S_FALSE },
    { CH_ENDELEMENT, 4, 24, 4, 28, "", "Name", "Name", S_FALSE },
    { CH_CHARACTERS, 4, 29, 5, 1, "\n", NULL, NULL, S_FALSE },
    { CH_ENDELEMENT, 5, 3, 5, 14, "", "BankAccount", "BankAccount", S_FALSE },
    { CH_ENDDOCUMENT, 0, 0, 6, 0, NULL, NULL, NULL, S_FALSE },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTestAttributes[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0 },
    { CH_STARTDOCUMENT, 0, 0, 1, 22 },
    { CH_STARTPREFIXMAPPING, 2, 96, 2, 95, "test", "prefix_test" },
    { CH_STARTPREFIXMAPPING, 2, 96, 2, 95, "", "prefix" },
    { CH_STARTELEMENT, 2, 96, 2, 95, "prefix", "document", "document" },
    { CH_CHARACTERS, 2, 96, 3, 1, "\n" },
    { CH_STARTPREFIXMAPPING, 3, 25, 3, 24, "p", "test" },
    { CH_STARTELEMENT, 3, 25, 3, 24, "prefix", "node1", "node1" },
    { CH_ENDELEMENT, 3, 25, 3, 24, "prefix", "node1", "node1" },
    { CH_ENDPREFIXMAPPING, 3, 25, 3, 24, "p" },
    { CH_ENDELEMENT, 3, 27, 3, 35, "prefix", "document", "document" },
    { CH_ENDPREFIXMAPPING, 3, 27, 3, 35, "" },
    { CH_ENDPREFIXMAPPING, 3, 27, 3, 35, "test" },
    { CH_ENDDOCUMENT, 0, 0, 4, 0 },
    { CH_ENDTEST }
};

static content_handler_test contentHandlerTestAttributes6[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0 },
    { CH_STARTDOCUMENT, 0, 0, 1, 22 },
    { CH_STARTPREFIXMAPPING, 2, 96, 2, 95, "test", "prefix_test" },
    { CH_STARTPREFIXMAPPING, 2, 96, 2, 95, "", "prefix" },
    { CH_STARTELEMENT, 2, 96, 2, 95, "prefix", "document", "document" },
    { CH_CHARACTERS, 2, 96, 3, 1, "\n" },
    { CH_STARTPREFIXMAPPING, 3, 25, 3, 24, "p", "test" },
    { CH_STARTELEMENT, 3, 25, 3, 24, "prefix", "node1", "node1" },
    { CH_ENDELEMENT, 3, 25, 3, 24, "prefix", "node1", "node1" },
    { CH_ENDPREFIXMAPPING, 3, 25, 3, 24, "p" },
    { CH_ENDELEMENT, 3, 27, 3, 35, "prefix", "document", "document" },
    { CH_ENDPREFIXMAPPING, 3, 27, 3, 35, "test" },
    { CH_ENDPREFIXMAPPING, 3, 27, 3, 35, "" },
    { CH_ENDDOCUMENT, 0, 0, 4, 0 },
    { CH_ENDTEST }
};

static content_handler_test xmlspaceattr_test[] = {
    { CH_PUTDOCUMENTLOCATOR, 0, 0, 1, 0 },
    { CH_STARTDOCUMENT, 0, 0, 1, 39 },
    { CH_STARTELEMENT, 1, 64, 1, 63, "", "a", "a" },
    { CH_CHARACTERS, 1, 64, 1, 80, " Some text data " },
    { CH_ENDELEMENT, 1, 82, 1, 83, "", "a", "a" },
    { CH_ENDDOCUMENT, 0, 0, 1, 83 },
    { CH_ENDTEST }
};

static const char xmlspace_attr[] =
    "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
    "<a xml:space=\"preserve\"> Some text data </a>";

static content_handler_test *expectCall;
static ISAXLocator *locator;
int msxml_version;

static void test_saxstr(unsigned line, const WCHAR *szStr, int nStr, const char *szTest)
{
    WCHAR buf[1024];
    int len;

    if(!szTest) {
        ok_(__FILE__,line) (szStr == NULL, "szStr != NULL\n");
        ok_(__FILE__,line) (nStr == 0, "nStr = %d, expected 0\n", nStr);
        return;
    }

    len = strlen(szTest);
    ok_(__FILE__,line) (len == nStr, "nStr = %d, expected %d (%s)\n", nStr, len, szTest);
    if(len != nStr) {
        ok_(__FILE__,line)(0, "got string %s, expected %s\n", wine_dbgstr_wn(szStr, nStr), szTest);
        return;
    }

    MultiByteToWideChar(CP_ACP, 0, szTest, -1, buf, sizeof(buf)/sizeof(WCHAR));
    ok_(__FILE__,line) (!memcmp(szStr, buf, len*sizeof(WCHAR)), "unexpected szStr %s, expected %s\n",
                        wine_dbgstr_wn(szStr, nStr), szTest);
}

static BOOL test_expect_call(CH id)
{
    ok(expectCall->id == id, "unexpected call %d, expected %d\n", id, expectCall->id);
    return expectCall->id == id;
}

static void test_locator(unsigned line, int loc_line, int loc_column)
{
    int rcolumn, rline;
    ISAXLocator_getLineNumber(locator, &rline);
    ISAXLocator_getColumnNumber(locator, &rcolumn);

    ok_(__FILE__,line) (rline == loc_line,
            "unexpected line %d, expected %d\n", rline, loc_line);
    ok_(__FILE__,line) (rcolumn == loc_column,
            "unexpected column %d, expected %d\n", rcolumn, loc_column);
}

static HRESULT WINAPI contentHandler_QueryInterface(
        ISAXContentHandler* iface,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ISAXContentHandler))
    {
        *ppvObject = iface;
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG WINAPI contentHandler_AddRef(
        ISAXContentHandler* iface)
{
    return 2;
}

static ULONG WINAPI contentHandler_Release(
        ISAXContentHandler* iface)
{
    return 1;
}

static HRESULT WINAPI contentHandler_putDocumentLocator(
        ISAXContentHandler* iface,
        ISAXLocator *pLocator)
{
    HRESULT hr;

    if(!test_expect_call(CH_PUTDOCUMENTLOCATOR))
        return E_FAIL;

    locator = pLocator;
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    if(msxml_version >= 6) {
        ISAXAttributes *attr, *attr1;
        IMXAttributes *mxattr;

        EXPECT_REF(pLocator, 1);
        hr = ISAXLocator_QueryInterface(pLocator, &IID_ISAXAttributes, (void**)&attr);
        EXPECT_HR(hr, S_OK);
        EXPECT_REF(pLocator, 2);
        hr = ISAXLocator_QueryInterface(pLocator, &IID_ISAXAttributes, (void**)&attr1);
        EXPECT_HR(hr, S_OK);
        EXPECT_REF(pLocator, 3);
        ok(attr == attr1, "got %p, %p\n", attr, attr1);

        hr = ISAXAttributes_QueryInterface(attr, &IID_IMXAttributes, (void**)&mxattr);
        EXPECT_HR(hr, E_NOINTERFACE);

        ISAXAttributes_Release(attr);
        ISAXAttributes_Release(attr1);
    }

    return (expectCall++)->ret;
}

static ISAXAttributes *test_attr_ptr;
static HRESULT WINAPI contentHandler_startDocument(
        ISAXContentHandler* iface)
{
    if(!test_expect_call(CH_STARTDOCUMENT))
        return E_FAIL;

    test_attr_ptr = NULL;
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_endDocument(
        ISAXContentHandler* iface)
{
    if(!test_expect_call(CH_ENDDOCUMENT))
        return E_FAIL;

    if(expectCall == xmlspaceattr_test+5 && msxml_version>=6) {
    todo_wine
        test_locator(__LINE__, expectCall->line_v6, expectCall->column_v6);
    }
    else
        test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
                msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_startPrefixMapping(
        ISAXContentHandler* iface,
        const WCHAR *pPrefix,
        int nPrefix,
        const WCHAR *pUri,
        int nUri)
{
    if(!test_expect_call(CH_STARTPREFIXMAPPING))
        return E_FAIL;

    test_saxstr(__LINE__, pPrefix, nPrefix, expectCall->arg1);
    test_saxstr(__LINE__, pUri, nUri, expectCall->arg2);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_endPrefixMapping(
        ISAXContentHandler* iface,
        const WCHAR *pPrefix,
        int nPrefix)
{
    if(!test_expect_call(CH_ENDPREFIXMAPPING))
        return E_FAIL;

    test_saxstr(__LINE__, pPrefix, nPrefix, expectCall->arg1);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_startElement(
        ISAXContentHandler* iface,
        const WCHAR *pNamespaceUri,
        int nNamespaceUri,
        const WCHAR *pLocalName,
        int nLocalName,
        const WCHAR *pQName,
        int nQName,
        ISAXAttributes *pAttr)
{
    int len;
    HRESULT hres;

    if(!test_expect_call(CH_STARTELEMENT))
        return E_FAIL;

    test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, expectCall->arg1);
    test_saxstr(__LINE__, pLocalName, nLocalName, expectCall->arg2);
    test_saxstr(__LINE__, pQName, nQName, expectCall->arg3);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    if(!test_attr_ptr)
        test_attr_ptr = pAttr;
    ok(test_attr_ptr == pAttr, "Multiple ISAXAttributes instances are used (%p %p)\n", test_attr_ptr, pAttr);

    if(expectCall == contentHandlerTestAttributes+4) {
        const WCHAR *uri_ptr = NULL;
        int i;
        /* msxml3 returns attributes and namespaces in the input order */
        hres = ISAXAttributes_getLength(pAttr, &len);
        ok(hres == S_OK, "getLength returned %x\n", hres);
        ok(len == 5, "Incorrect number of attributes: %d\n", len);
        ok(msxml_version < 6, "wrong msxml_version: %d\n", msxml_version);

        for(i=0; i<len; i++) {
            hres = ISAXAttributes_getName(pAttr, i, &pNamespaceUri, &nNamespaceUri,
                    &pLocalName, &nLocalName, &pQName, &nQName);
            ok(hres == S_OK, "getName returned %x\n", hres);

            if(nQName == 4) {
                todo_wine ok(i==3, "Incorrect attributes order\n");
                test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "");
                test_saxstr(__LINE__, pLocalName, nLocalName, "arg2");
                test_saxstr(__LINE__, pQName, nQName, "arg2");
            } else if(nQName == 5) {
                todo_wine ok(i==1, "Incorrect attributes order\n");
                test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "");
                test_saxstr(__LINE__, pLocalName, nLocalName, "");
                test_saxstr(__LINE__, pQName, nQName, "xmlns");
            } else if(nQName == 8) {
                todo_wine ok(i==4, "Incorrect attributes order\n");
                test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "prefix_test");
                test_saxstr(__LINE__, pLocalName, nLocalName, "ar3");
                test_saxstr(__LINE__, pQName, nQName, "test:ar3");
                ok(uri_ptr == pNamespaceUri, "Incorrect NamespaceUri pointer\n");
            } else if(nQName == 9) {
                todo_wine ok(i==2, "Incorrect attributes order\n");
                test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "prefix_test");
                test_saxstr(__LINE__, pLocalName, nLocalName, "arg1");
                test_saxstr(__LINE__, pQName, nQName, "test:arg1");
                uri_ptr = pNamespaceUri;
            } else if(nQName == 10) {
                todo_wine ok(i==0, "Incorrect attributes order\n");
                test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "");
                test_saxstr(__LINE__, pLocalName, nLocalName, "");
                test_saxstr(__LINE__, pQName, nQName, "xmlns:test");
            } else {
                ok(0, "Unexpected attribute\n");
            }
        }
    } else if(expectCall == contentHandlerTestAttributes6+4) {
        const WCHAR *uri_ptr;

        /* msxml6 returns attributes first and then namespaces */
        hres = ISAXAttributes_getLength(pAttr, &len);
        ok(hres == S_OK, "getLength returned %x\n", hres);
        ok(len == 5, "Incorrect number of attributes: %d\n", len);
        ok(msxml_version >= 6, "wrong msxml_version: %d\n", msxml_version);

        hres = ISAXAttributes_getName(pAttr, 0, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        ok(hres == S_OK, "getName returned %x\n", hres);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "prefix_test");
        test_saxstr(__LINE__, pLocalName, nLocalName, "arg1");
        test_saxstr(__LINE__, pQName, nQName, "test:arg1");
        uri_ptr = pNamespaceUri;

        hres = ISAXAttributes_getName(pAttr, 1, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        ok(hres == S_OK, "getName returned %x\n", hres);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "");
        test_saxstr(__LINE__, pLocalName, nLocalName, "arg2");
        test_saxstr(__LINE__, pQName, nQName, "arg2");

        hres = ISAXAttributes_getName(pAttr, 2, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        ok(hres == S_OK, "getName returned %x\n", hres);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "prefix_test");
        test_saxstr(__LINE__, pLocalName, nLocalName, "ar3");
        test_saxstr(__LINE__, pQName, nQName, "test:ar3");
        ok(uri_ptr == pNamespaceUri, "Incorrect NamespaceUri pointer\n");

        hres = ISAXAttributes_getName(pAttr, 3, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        ok(hres == S_OK, "getName returned %x\n", hres);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "http://www.w3.org/2000/xmlns/");
        test_saxstr(__LINE__, pLocalName, nLocalName, "");
        test_saxstr(__LINE__, pQName, nQName, "xmlns:test");

        hres = ISAXAttributes_getName(pAttr, 4, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        ok(hres == S_OK, "getName returned %x\n", hres);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "http://www.w3.org/2000/xmlns/");
        test_saxstr(__LINE__, pLocalName, nLocalName, "");
        test_saxstr(__LINE__, pQName, nQName, "xmlns");
    } else if(expectCall == xmlspaceattr_test+2) {
        const WCHAR *value;
        int valuelen;

        hres = ISAXAttributes_getLength(pAttr, &len);
        EXPECT_HR(hres, S_OK);
        ok(len == 1, "Incorrect number of attributes: %d\n", len);

        hres = ISAXAttributes_getName(pAttr, 0, &pNamespaceUri, &nNamespaceUri,
                &pLocalName, &nLocalName, &pQName, &nQName);
        EXPECT_HR(hres, S_OK);
        test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, "http://www.w3.org/XML/1998/namespace");
        test_saxstr(__LINE__, pLocalName, nLocalName, "space");
        test_saxstr(__LINE__, pQName, nQName, "xml:space");

        hres = ISAXAttributes_getValue(pAttr, 0, &value, &valuelen);
        EXPECT_HR(hres, S_OK);
        test_saxstr(__LINE__, value, valuelen, "preserve");
    }

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_endElement(
        ISAXContentHandler* iface,
        const WCHAR *pNamespaceUri,
        int nNamespaceUri,
        const WCHAR *pLocalName,
        int nLocalName,
        const WCHAR *pQName,
        int nQName)
{
    if(!test_expect_call(CH_ENDELEMENT))
        return E_FAIL;

    test_saxstr(__LINE__, pNamespaceUri, nNamespaceUri, expectCall->arg1);
    test_saxstr(__LINE__, pLocalName, nLocalName, expectCall->arg2);
    test_saxstr(__LINE__, pQName, nQName, expectCall->arg3);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_characters(
        ISAXContentHandler* iface,
        const WCHAR *pChars,
        int nChars)
{
    if(!test_expect_call(CH_CHARACTERS))
        return E_FAIL;

    test_saxstr(__LINE__, pChars, nChars, expectCall->arg1);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_ignorableWhitespace(
        ISAXContentHandler* iface,
        const WCHAR *pChars,
        int nChars)
{
    if(!test_expect_call(CH_IGNORABLEWHITESPACE))
        return E_FAIL;

    test_saxstr(__LINE__, pChars, nChars, expectCall->arg1);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_processingInstruction(
        ISAXContentHandler* iface,
        const WCHAR *pTarget,
        int nTarget,
        const WCHAR *pData,
        int nData)
{
    if(!test_expect_call(CH_PROCESSINGINSTRUCTION))
        return E_FAIL;

    test_saxstr(__LINE__, pTarget, nTarget, expectCall->arg1);
    test_saxstr(__LINE__, pData, nData, expectCall->arg2);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}

static HRESULT WINAPI contentHandler_skippedEntity(
        ISAXContentHandler* iface,
        const WCHAR *pName,
        int nName)
{
    if(!test_expect_call(CH_SKIPPEDENTITY))
        return E_FAIL;

    test_saxstr(__LINE__, pName, nName, expectCall->arg1);
    test_locator(__LINE__, msxml_version>=6 ? expectCall->line_v6 : expectCall->line,
            msxml_version>=6 ? expectCall->column_v6 : expectCall->column);

    return (expectCall++)->ret;
}


static const ISAXContentHandlerVtbl contentHandlerVtbl =
{
    contentHandler_QueryInterface,
    contentHandler_AddRef,
    contentHandler_Release,
    contentHandler_putDocumentLocator,
    contentHandler_startDocument,
    contentHandler_endDocument,
    contentHandler_startPrefixMapping,
    contentHandler_endPrefixMapping,
    contentHandler_startElement,
    contentHandler_endElement,
    contentHandler_characters,
    contentHandler_ignorableWhitespace,
    contentHandler_processingInstruction,
    contentHandler_skippedEntity
};

static ISAXContentHandler contentHandler = { &contentHandlerVtbl };

static HRESULT WINAPI isaxerrorHandler_QueryInterface(
        ISAXErrorHandler* iface,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ISAXErrorHandler))
    {
        *ppvObject = iface;
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG WINAPI isaxerrorHandler_AddRef(
        ISAXErrorHandler* iface)
{
    return 2;
}

static ULONG WINAPI isaxerrorHandler_Release(
        ISAXErrorHandler* iface)
{
    return 1;
}

static HRESULT WINAPI isaxerrorHandler_error(
        ISAXErrorHandler* iface,
        ISAXLocator *pLocator,
        const WCHAR *pErrorMessage,
        HRESULT hrErrorCode)
{
    ok(0, "unexpected call\n");
    return S_OK;
}

static HRESULT WINAPI isaxerrorHandler_fatalError(
        ISAXErrorHandler* iface,
        ISAXLocator *pLocator,
        const WCHAR *pErrorMessage,
        HRESULT hrErrorCode)
{
    if(!test_expect_call(EH_FATALERROR))
        return E_FAIL;

    ok(hrErrorCode == expectCall->ret, "hrErrorCode = %x, expected %x\n", hrErrorCode, expectCall->ret);

    expectCall++;
    return S_OK;
}

static HRESULT WINAPI isaxerrorHanddler_ignorableWarning(
        ISAXErrorHandler* iface,
        ISAXLocator *pLocator,
        const WCHAR *pErrorMessage,
        HRESULT hrErrorCode)
{
    ok(0, "unexpected call\n");
    return S_OK;
}

static const ISAXErrorHandlerVtbl errorHandlerVtbl =
{
    isaxerrorHandler_QueryInterface,
    isaxerrorHandler_AddRef,
    isaxerrorHandler_Release,
    isaxerrorHandler_error,
    isaxerrorHandler_fatalError,
    isaxerrorHanddler_ignorableWarning
};

static ISAXErrorHandler errorHandler = { &errorHandlerVtbl };

static HRESULT WINAPI isaxattributes_QueryInterface(
        ISAXAttributes* iface,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ISAXAttributes))
    {
        *ppvObject = iface;
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG WINAPI isaxattributes_AddRef(ISAXAttributes* iface)
{
    return 2;
}

static ULONG WINAPI isaxattributes_Release(ISAXAttributes* iface)
{
    return 1;
}

static HRESULT WINAPI isaxattributes_getLength(ISAXAttributes* iface, int *length)
{
    *length = 3;
    return S_OK;
}

static HRESULT WINAPI isaxattributes_getURI(
    ISAXAttributes* iface,
    int nIndex,
    const WCHAR **pUrl,
    int *pUriSize)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getLocalName(
    ISAXAttributes* iface,
    int nIndex,
    const WCHAR **pLocalName,
    int *pLocalNameLength)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getQName(
    ISAXAttributes* iface,
    int index,
    const WCHAR **QName,
    int *QNameLength)
{
    static const WCHAR attrqnamesW[][15] = {{'a',':','a','t','t','r','1','j','u','n','k',0},
                                            {'a','t','t','r','2','j','u','n','k',0},
                                            {'a','t','t','r','3',0}};
    static const int attrqnamelen[] = {7, 5, 5};

    ok(index >= 0 && index <= 2, "invalid index received %d\n", index);

    *QName = attrqnamesW[index];
    *QNameLength = attrqnamelen[index];

    return S_OK;
}

static HRESULT WINAPI isaxattributes_getName(
    ISAXAttributes* iface,
    int nIndex,
    const WCHAR **pUri,
    int * pUriLength,
    const WCHAR ** pLocalName,
    int * pLocalNameSize,
    const WCHAR ** pQName,
    int * pQNameLength)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getIndexFromName(
    ISAXAttributes* iface,
    const WCHAR * pUri,
    int cUriLength,
    const WCHAR * pLocalName,
    int cocalNameLength,
    int * index)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getIndexFromQName(
    ISAXAttributes* iface,
    const WCHAR * pQName,
    int nQNameLength,
    int * index)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getType(
    ISAXAttributes* iface,
    int nIndex,
    const WCHAR ** pType,
    int * pTypeLength)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getTypeFromName(
    ISAXAttributes* iface,
    const WCHAR * pUri,
    int nUri,
    const WCHAR * pLocalName,
    int nLocalName,
    const WCHAR ** pType,
    int * nType)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getTypeFromQName(
    ISAXAttributes* iface,
    const WCHAR * pQName,
    int nQName,
    const WCHAR ** pType,
    int * nType)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getValue(ISAXAttributes* iface, int index,
    const WCHAR **value, int *nValue)
{
    static const WCHAR attrvaluesW[][10] = {{'a','1','j','u','n','k',0},
                                            {'a','2','j','u','n','k',0},
                                            {'<','&','"','>',0}};
    static const int attrvalueslen[] = {2, 2, 4};

    ok(index >= 0 && index <= 2, "invalid index received %d\n", index);

    *value = attrvaluesW[index];
    *nValue = attrvalueslen[index];

    return S_OK;
}

static HRESULT WINAPI isaxattributes_getValueFromName(
    ISAXAttributes* iface,
    const WCHAR * pUri,
    int nUri,
    const WCHAR * pLocalName,
    int nLocalName,
    const WCHAR ** pValue,
    int * nValue)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxattributes_getValueFromQName(
    ISAXAttributes* iface,
    const WCHAR * pQName,
    int nQName,
    const WCHAR ** pValue,
    int * nValue)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const ISAXAttributesVtbl SAXAttributesVtbl =
{
    isaxattributes_QueryInterface,
    isaxattributes_AddRef,
    isaxattributes_Release,
    isaxattributes_getLength,
    isaxattributes_getURI,
    isaxattributes_getLocalName,
    isaxattributes_getQName,
    isaxattributes_getName,
    isaxattributes_getIndexFromName,
    isaxattributes_getIndexFromQName,
    isaxattributes_getType,
    isaxattributes_getTypeFromName,
    isaxattributes_getTypeFromQName,
    isaxattributes_getValue,
    isaxattributes_getValueFromName,
    isaxattributes_getValueFromQName
};

static ISAXAttributes saxattributes = { &SAXAttributesVtbl };

static int handler_addrefcalled;

static HRESULT WINAPI isaxlexical_QueryInterface(ISAXLexicalHandler* iface, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_ISAXLexicalHandler))
    {
        *ppvObject = iface;
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG WINAPI isaxlexical_AddRef(ISAXLexicalHandler* iface)
{
    handler_addrefcalled++;
    return 2;
}

static ULONG WINAPI isaxlexical_Release(ISAXLexicalHandler* iface)
{
    return 1;
}

static HRESULT WINAPI isaxlexical_startDTD(ISAXLexicalHandler* iface,
    const WCHAR * pName, int nName, const WCHAR * pPublicId,
    int nPublicId, const WCHAR * pSystemId, int nSystemId)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_endDTD(ISAXLexicalHandler* iface)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_startEntity(ISAXLexicalHandler *iface,
    const WCHAR * pName, int nName)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_endEntity(ISAXLexicalHandler *iface,
    const WCHAR * pName, int nName)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_startCDATA(ISAXLexicalHandler *iface)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_endCDATA(ISAXLexicalHandler *iface)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxlexical_comment(ISAXLexicalHandler *iface,
    const WCHAR * pChars, int nChars)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static const ISAXLexicalHandlerVtbl SAXLexicalHandlerVtbl =
{
   isaxlexical_QueryInterface,
   isaxlexical_AddRef,
   isaxlexical_Release,
   isaxlexical_startDTD,
   isaxlexical_endDTD,
   isaxlexical_startEntity,
   isaxlexical_endEntity,
   isaxlexical_startCDATA,
   isaxlexical_endCDATA,
   isaxlexical_comment
};

static ISAXLexicalHandler saxlexicalhandler = { &SAXLexicalHandlerVtbl };

static HRESULT WINAPI isaxdecl_QueryInterface(ISAXDeclHandler* iface, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_ISAXDeclHandler))
    {
        *ppvObject = iface;
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG WINAPI isaxdecl_AddRef(ISAXDeclHandler* iface)
{
    handler_addrefcalled++;
    return 2;
}

static ULONG WINAPI isaxdecl_Release(ISAXDeclHandler* iface)
{
    return 1;
}

static HRESULT WINAPI isaxdecl_elementDecl(ISAXDeclHandler* iface,
    const WCHAR * pName, int nName, const WCHAR * pModel, int nModel)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxdecl_attributeDecl(ISAXDeclHandler* iface,
    const WCHAR * pElementName, int nElementName, const WCHAR * pAttributeName,
    int nAttributeName, const WCHAR * pType, int nType, const WCHAR * pValueDefault,
    int nValueDefault, const WCHAR * pValue, int nValue)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxdecl_internalEntityDecl(ISAXDeclHandler* iface,
    const WCHAR * pName, int nName, const WCHAR * pValue, int nValue)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI isaxdecl_externalEntityDecl(ISAXDeclHandler* iface,
    const WCHAR * pName, int nName, const WCHAR * pPublicId, int nPublicId,
    const WCHAR * pSystemId, int nSystemId)
{
    ok(0, "call not expected\n");
    return E_NOTIMPL;
}

static const ISAXDeclHandlerVtbl SAXDeclHandlerVtbl =
{
   isaxdecl_QueryInterface,
   isaxdecl_AddRef,
   isaxdecl_Release,
   isaxdecl_elementDecl,
   isaxdecl_attributeDecl,
   isaxdecl_internalEntityDecl,
   isaxdecl_externalEntityDecl
};

static ISAXDeclHandler saxdeclhandler = { &SAXDeclHandlerVtbl };

typedef struct mxwriter_write_test_t {
    BOOL        last;
    const BYTE  *data;
    DWORD       cb;
    BOOL        null_written;
    BOOL        fail_write;
} mxwriter_write_test;

typedef struct mxwriter_stream_test_t {
    VARIANT_BOOL        bom;
    const char          *encoding;
    mxwriter_write_test expected_writes[4];
} mxwriter_stream_test;

static const mxwriter_write_test *current_write_test;
static DWORD current_stream_test_index;

static HRESULT WINAPI istream_QueryInterface(IStream *iface, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if(IsEqualGUID(riid, &IID_IStream) || IsEqualGUID(riid, &IID_IUnknown))
        *ppvObject = iface;
    else
        return E_NOINTERFACE;

    return S_OK;
}

static ULONG WINAPI istream_AddRef(IStream *iface)
{
    return 2;
}

static ULONG WINAPI istream_Release(IStream *iface)
{
    return 1;
}

static HRESULT WINAPI istream_Read(IStream *iface, void *pv, ULONG cb, ULONG *pcbRead)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_Write(IStream *iface, const void *pv, ULONG cb, ULONG *pcbWritten)
{
    BOOL fail = FALSE;

    ok(pv != NULL, "pv == NULL\n");

    if(current_write_test->last) {
        ok(0, "Too many Write calls made on test %d\n", current_stream_test_index);
        return E_FAIL;
    }

    fail = current_write_test->fail_write;

    ok(current_write_test->cb == cb, "Expected %d, but got %d on test %d\n",
        current_write_test->cb, cb, current_stream_test_index);

    if(!pcbWritten)
        ok(current_write_test->null_written, "pcbWritten was NULL on test %d\n", current_stream_test_index);
    else
        ok(!memcmp(current_write_test->data, pv, cb), "Unexpected data on test %d\n", current_stream_test_index);

    ++current_write_test;

    if(pcbWritten)
        *pcbWritten = cb;

    return fail ? E_FAIL : S_OK;
}

static HRESULT WINAPI istream_Seek(IStream *iface, LARGE_INTEGER dlibMove, DWORD dwOrigin,
        ULARGE_INTEGER *plibNewPosition)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_SetSize(IStream *iface, ULARGE_INTEGER libNewSize)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_CopyTo(IStream *iface, IStream *pstm, ULARGE_INTEGER cb,
        ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *plibWritten)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_Commit(IStream *iface, DWORD grfCommitFlags)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_Revert(IStream *iface)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_LockRegion(IStream *iface, ULARGE_INTEGER libOffset,
        ULARGE_INTEGER cb, DWORD dwLockType)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_UnlockRegion(IStream *iface, ULARGE_INTEGER libOffset,
        ULARGE_INTEGER cb, DWORD dwLockType)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_Stat(IStream *iface, STATSTG *pstatstg, DWORD grfStatFlag)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI istream_Clone(IStream *iface, IStream **ppstm)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const IStreamVtbl StreamVtbl = {
    istream_QueryInterface,
    istream_AddRef,
    istream_Release,
    istream_Read,
    istream_Write,
    istream_Seek,
    istream_SetSize,
    istream_CopyTo,
    istream_Commit,
    istream_Revert,
    istream_LockRegion,
    istream_UnlockRegion,
    istream_Stat,
    istream_Clone
};

static IStream mxstream = { &StreamVtbl };

static void test_saxreader(int version)
{
    HRESULT hr;
    ISAXXMLReader *reader = NULL;
    VARIANT var;
    ISAXContentHandler *lpContentHandler;
    ISAXErrorHandler *lpErrorHandler;
    SAFEARRAY *pSA;
    SAFEARRAYBOUND SADim[1];
    char *pSAData = NULL;
    IStream *iStream;
    ULARGE_INTEGER liSize;
    LARGE_INTEGER liPos;
    ULONG bytesWritten;
    HANDLE file;
    static const CHAR testXmlA[] = "test.xml";
    static const WCHAR testXmlW[] = {'t','e','s','t','.','x','m','l',0};
    IXMLDOMDocument *domDocument;
    BSTR bstrData;
    VARIANT_BOOL vBool;

    msxml_version = version;
    if(version == 3) {
        hr = CoCreateInstance(&CLSID_SAXXMLReader30, NULL, CLSCTX_INPROC_SERVER,
                &IID_ISAXXMLReader, (LPVOID*)&reader);
    } else if(version == 6) {
        hr = CoCreateInstance(&CLSID_SAXXMLReader60, NULL, CLSCTX_INPROC_SERVER,
                &IID_ISAXXMLReader, (LPVOID*)&reader);
        if(hr == REGDB_E_CLASSNOTREG) {
            win_skip("SAXXMLReader6 not registered\n");
            return;
        }
    } else {
        hr = CoCreateInstance(&CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER,
                &IID_ISAXXMLReader, (LPVOID*)&reader);
    }
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    if(version != 6) {
        hr = ISAXXMLReader_getContentHandler(reader, NULL);
        ok(hr == E_POINTER, "Expected E_POINTER, got %08x\n", hr);

        hr = ISAXXMLReader_getErrorHandler(reader, NULL);
        ok(hr == E_POINTER, "Expected E_POINTER, got %08x\n", hr);
    }

    hr = ISAXXMLReader_getContentHandler(reader, &lpContentHandler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(lpContentHandler == NULL, "Expected %p, got %p\n", NULL, lpContentHandler);

    hr = ISAXXMLReader_getErrorHandler(reader, &lpErrorHandler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(lpErrorHandler == NULL, "Expected %p, got %p\n", NULL, lpErrorHandler);

    hr = ISAXXMLReader_putContentHandler(reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = ISAXXMLReader_putContentHandler(reader, &contentHandler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = ISAXXMLReader_putErrorHandler(reader, &errorHandler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = ISAXXMLReader_getContentHandler(reader, &lpContentHandler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(lpContentHandler == &contentHandler, "Expected %p, got %p\n", &contentHandler, lpContentHandler);

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(szSimpleXML);

    expectCall = contentHandlerTest1;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    VariantClear(&var);

    SADim[0].lLbound= 0;
    SADim[0].cElements= sizeof(szTestXML)-1;
    pSA = SafeArrayCreate(VT_UI1, 1, SADim);
    SafeArrayAccessData(pSA, (void**)&pSAData);
    memcpy(pSAData, szTestXML, sizeof(szTestXML)-1);
    SafeArrayUnaccessData(pSA);
    V_VT(&var) = VT_ARRAY|VT_UI1;
    V_ARRAY(&var) = pSA;

    expectCall = contentHandlerTest1;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    SafeArrayDestroy(pSA);

    CreateStreamOnHGlobal(NULL, TRUE, &iStream);
    liSize.QuadPart = strlen(szTestXML);
    IStream_SetSize(iStream, liSize);
    IStream_Write(iStream, szTestXML, strlen(szTestXML), &bytesWritten);
    liPos.QuadPart = 0;
    IStream_Seek(iStream, liPos, STREAM_SEEK_SET, NULL);
    V_VT(&var) = VT_UNKNOWN|VT_DISPATCH;
    V_UNKNOWN(&var) = (IUnknown*)iStream;

    expectCall = contentHandlerTest1;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    IStream_Release(iStream);

    CreateStreamOnHGlobal(NULL, TRUE, &iStream);
    liSize.QuadPart = strlen(szTestAttributes);
    IStream_SetSize(iStream, liSize);
    IStream_Write(iStream, szTestAttributes, strlen(szTestAttributes), &bytesWritten);
    liPos.QuadPart = 0;
    IStream_Seek(iStream, liPos, STREAM_SEEK_SET, NULL);
    V_VT(&var) = VT_UNKNOWN|VT_DISPATCH;
    V_UNKNOWN(&var) = (IUnknown*)iStream;

    if(version >= 6)
        expectCall = contentHandlerTestAttributes6;
    else
        expectCall = contentHandlerTestAttributes;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    IStream_Release(iStream);

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(szCarriageRetTest);

    expectCall = contentHandlerTest2;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    VariantClear(&var);

    file = CreateFileA(testXmlA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ok(file != INVALID_HANDLE_VALUE, "Could not create file: %u\n", GetLastError());
    WriteFile(file, szTestXML, sizeof(szTestXML)-1, &bytesWritten, NULL);
    CloseHandle(file);

    expectCall = contentHandlerTest1;
    hr = ISAXXMLReader_parseURL(reader, testXmlW);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    expectCall = contentHandlerTestError;
    hr = ISAXXMLReader_parseURL(reader, testXmlW);
    ok(hr == E_FAIL, "Expected E_FAIL, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    if(version >= 6)
        expectCall = contentHandlerTestCallbackResult6;
    else
        expectCall = contentHandlerTestCallbackResults;
    hr = ISAXXMLReader_parseURL(reader, testXmlW);
    ok(hr == (version>=6 ? S_OK : S_FALSE), "Expected S_FALSE, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);

    DeleteFileA(testXmlA);

    hr = CoCreateInstance(&CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER,
            &IID_IXMLDOMDocument, (LPVOID*)&domDocument);
    if(FAILED(hr))
    {
        skip("Failed to create DOMDocument instance\n");
        return;
    }
    bstrData = SysAllocString(szSimpleXML);
    hr = IXMLDOMDocument_loadXML(domDocument, bstrData, &vBool);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    V_VT(&var) = VT_UNKNOWN;
    V_UNKNOWN(&var) = (IUnknown*)domDocument;

    expectCall = contentHandlerTest2;
    hr = ISAXXMLReader_parse(reader, var);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    test_expect_call(CH_ENDTEST);
    IXMLDOMDocument_Release(domDocument);

    expectCall = xmlspaceattr_test;
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = _bstr_(xmlspace_attr);
    hr = ISAXXMLReader_parse(reader, var);
    EXPECT_HR(hr, S_OK);
    test_expect_call(CH_ENDTEST);

    ISAXXMLReader_Release(reader);
    SysFreeString(bstrData);
    free_bstrs();
}

struct saxreader_props_test_t
{
    const char *prop_name;
    IUnknown   *iface;
};

static const struct saxreader_props_test_t props_test_data[] = {
    { "http://xml.org/sax/properties/lexical-handler", (IUnknown*)&saxlexicalhandler },
    { "http://xml.org/sax/properties/declaration-handler", (IUnknown*)&saxdeclhandler },
    { 0 }
};

static void test_saxreader_properties(void)
{
    const struct saxreader_props_test_t *ptr = props_test_data;
    ISAXXMLReader *reader;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER,
            &IID_ISAXXMLReader, (void**)&reader);
    EXPECT_HR(hr, S_OK);

    hr = ISAXXMLReader_getProperty(reader, _bstr_("http://xml.org/sax/properties/lexical-handler"), NULL);
    EXPECT_HR(hr, E_POINTER);

    while (ptr->prop_name)
    {
        VARIANT v;

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_getProperty(reader, _bstr_(ptr->prop_name), &v);
        EXPECT_HR(hr, S_OK);
        ok(V_VT(&v) == VT_UNKNOWN, "got %d\n", V_VT(&v));
        ok(V_UNKNOWN(&v) == NULL, "got %p\n", V_UNKNOWN(&v));

        V_VT(&v) = VT_UNKNOWN;
        V_UNKNOWN(&v) = ptr->iface;
        hr = ISAXXMLReader_putProperty(reader, _bstr_(ptr->prop_name), v);
        EXPECT_HR(hr, S_OK);

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        handler_addrefcalled = 0;
        hr = ISAXXMLReader_getProperty(reader, _bstr_(ptr->prop_name), &v);
        EXPECT_HR(hr, S_OK);
        ok(V_VT(&v) == VT_UNKNOWN, "got %d\n", V_VT(&v));
        ok(V_UNKNOWN(&v) == ptr->iface, "got %p\n", V_UNKNOWN(&v));
        ok(handler_addrefcalled == 1, "AddRef called %d times\n", handler_addrefcalled);
        VariantClear(&v);

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_putProperty(reader, _bstr_(ptr->prop_name), v);
        EXPECT_HR(hr, S_OK);

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_getProperty(reader, _bstr_(ptr->prop_name), &v);
        EXPECT_HR(hr, S_OK);
        ok(V_VT(&v) == VT_UNKNOWN, "got %d\n", V_VT(&v));
        ok(V_UNKNOWN(&v) == NULL, "got %p\n", V_UNKNOWN(&v));

        V_VT(&v) = VT_UNKNOWN;
        V_UNKNOWN(&v) = ptr->iface;
        hr = ISAXXMLReader_putProperty(reader, _bstr_(ptr->prop_name), v);
        EXPECT_HR(hr, S_OK);

        /* only VT_EMPTY seems to be valid to reset property */
        V_VT(&v) = VT_I4;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_putProperty(reader, _bstr_(ptr->prop_name), v);
        EXPECT_HR(hr, E_INVALIDARG);

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_getProperty(reader, _bstr_(ptr->prop_name), &v);
        EXPECT_HR(hr, S_OK);
        ok(V_VT(&v) == VT_UNKNOWN, "got %d\n", V_VT(&v));
        ok(V_UNKNOWN(&v) == ptr->iface, "got %p\n", V_UNKNOWN(&v));
        VariantClear(&v);

        V_VT(&v) = VT_UNKNOWN;
        V_UNKNOWN(&v) = NULL;
        hr = ISAXXMLReader_putProperty(reader, _bstr_(ptr->prop_name), v);
        EXPECT_HR(hr, S_OK);

        V_VT(&v) = VT_EMPTY;
        V_UNKNOWN(&v) = (IUnknown*)0xdeadbeef;
        hr = ISAXXMLReader_getProperty(reader, _bstr_(ptr->prop_name), &v);
        EXPECT_HR(hr, S_OK);
        ok(V_VT(&v) == VT_UNKNOWN, "got %d\n", V_VT(&v));
        ok(V_UNKNOWN(&v) == NULL, "got %p\n", V_UNKNOWN(&v));

        ptr++;
    }

    ISAXXMLReader_Release(reader);
    free_bstrs();
}

struct feature_ns_entry_t {
    const GUID *guid;
    const char *clsid;
    VARIANT_BOOL value;
};

static const struct feature_ns_entry_t feature_ns_entry_data[] = {
    { &CLSID_SAXXMLReader,   "CLSID_SAXXMLReader",   VARIANT_TRUE },
    { &CLSID_SAXXMLReader30, "CLSID_SAXXMLReader30", VARIANT_TRUE },
    { &CLSID_SAXXMLReader40, "CLSID_SAXXMLReader40", VARIANT_TRUE },
    { &CLSID_SAXXMLReader60, "CLSID_SAXXMLReader60", VARIANT_TRUE },
    { 0 }
};

static void test_saxreader_features(void)
{
    const struct feature_ns_entry_t *entry = feature_ns_entry_data;
    ISAXXMLReader *reader;

    while (entry->guid)
    {
        VARIANT_BOOL value;
        HRESULT hr;

        hr = CoCreateInstance(entry->guid, NULL, CLSCTX_INPROC_SERVER, &IID_ISAXXMLReader, (void**)&reader);
        if (hr != S_OK)
        {
            win_skip("can't create %s instance\n", entry->clsid);
            entry++;
            continue;
        }

        value = 0xc;
        hr = ISAXXMLReader_getFeature(reader, _bstr_("http://xml.org/sax/features/namespaces"), &value);
        EXPECT_HR(hr, S_OK);

        ok(entry->value == value, "%s: got wrong default value %x, expected %x\n", entry->clsid, value, entry->value);

        ISAXXMLReader_Release(reader);

        entry++;
    }
}

/* UTF-8 data with UTF-8 BOM and UTF-16 in prolog */
static const CHAR UTF8BOMTest[] =
"\xEF\xBB\xBF<?xml version = \"1.0\" encoding = \"UTF-16\"?>\n"
"<a></a>\n";

struct enc_test_entry_t {
    const GUID *guid;
    const char *clsid;
    const char *data;
    HRESULT hr;
    int todo;
};

static const struct enc_test_entry_t encoding_test_data[] = {
    { &CLSID_SAXXMLReader,   "CLSID_SAXXMLReader",   UTF8BOMTest, 0xc00ce56f, 1 },
    { &CLSID_SAXXMLReader30, "CLSID_SAXXMLReader30", UTF8BOMTest, 0xc00ce56f, 1 },
    { &CLSID_SAXXMLReader40, "CLSID_SAXXMLReader40", UTF8BOMTest, S_OK, 0 },
    { &CLSID_SAXXMLReader60, "CLSID_SAXXMLReader60", UTF8BOMTest, S_OK, 0 },
    { 0 }
};

static void test_encoding(void)
{
    const struct enc_test_entry_t *entry = encoding_test_data;
    static const WCHAR testXmlW[] = {'t','e','s','t','.','x','m','l',0};
    static const CHAR testXmlA[] = "test.xml";
    ISAXXMLReader *reader;
    DWORD written;
    HANDLE file;
    HRESULT hr;

    while (entry->guid)
    {
        hr = CoCreateInstance(entry->guid, NULL, CLSCTX_INPROC_SERVER, &IID_ISAXXMLReader, (void**)&reader);
        if (hr != S_OK)
        {
            win_skip("can't create %s instance\n", entry->clsid);
            entry++;
            continue;
        }

        file = CreateFileA(testXmlA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        ok(file != INVALID_HANDLE_VALUE, "Could not create file: %u\n", GetLastError());
        WriteFile(file, UTF8BOMTest, sizeof(UTF8BOMTest)-1, &written, NULL);
        CloseHandle(file);

        hr = ISAXXMLReader_parseURL(reader, testXmlW);
        if (entry->todo)
            todo_wine ok(hr == entry->hr, "Expected 0x%08x, got 0x%08x. CLSID %s\n", entry->hr, hr, entry->clsid);
        else
            ok(hr == entry->hr, "Expected 0x%08x, got 0x%08x. CLSID %s\n", entry->hr, hr, entry->clsid);

        DeleteFileA(testXmlA);
        ISAXXMLReader_Release(reader);

        entry++;
    }
}

static void test_mxwriter_handlers(void)
{
    ISAXContentHandler *handler;
    IMXWriter *writer, *writer2;
    ISAXLexicalHandler *lh;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    EXPECT_REF(writer, 1);

    /* ISAXContentHandler */
    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&handler);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    EXPECT_REF(writer, 2);
    EXPECT_REF(handler, 2);

    hr = ISAXContentHandler_QueryInterface(handler, &IID_IMXWriter, (void**)&writer2);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(writer2 == writer, "got %p, expected %p\n", writer2, writer);
    EXPECT_REF(writer, 3);
    EXPECT_REF(writer2, 3);
    IMXWriter_Release(writer2);
    ISAXContentHandler_Release(handler);

    /* ISAXLexicalHandler */
    hr = IMXWriter_QueryInterface(writer, &IID_ISAXLexicalHandler, (void**)&lh);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    EXPECT_REF(writer, 2);
    EXPECT_REF(lh, 2);

    hr = ISAXLexicalHandler_QueryInterface(lh, &IID_IMXWriter, (void**)&writer2);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(writer2 == writer, "got %p, expected %p\n", writer2, writer);
    EXPECT_REF(writer, 3);
    EXPECT_REF(writer2, 3);
    IMXWriter_Release(writer2);

    IMXWriter_Release(writer);
}

struct msxmlsupported_data_t
{
    const GUID *clsid;
    const char *name;
    BOOL supported;
};

static struct msxmlsupported_data_t mxwriter_support_data[] =
{
    { &CLSID_MXXMLWriter,   "MXXMLWriter" },
    { &CLSID_MXXMLWriter30, "MXXMLWriter30" },
    { &CLSID_MXXMLWriter40, "MXXMLWriter40" },
    { &CLSID_MXXMLWriter60, "MXXMLWriter60" },
    { NULL }
};

static struct msxmlsupported_data_t mxattributes_support_data[] =
{
    { &CLSID_SAXAttributes,   "SAXAttributes" },
    { &CLSID_SAXAttributes30, "SAXAttributes30" },
    { &CLSID_SAXAttributes40, "SAXAttributes40" },
    { &CLSID_SAXAttributes60, "SAXAttributes60" },
    { NULL }
};

static BOOL is_clsid_supported(const GUID *clsid, const struct msxmlsupported_data_t *table)
{
    while (table->clsid)
    {
        if (table->clsid == clsid) return table->supported;
        table++;
    }
    return FALSE;
}

struct mxwriter_props_t
{
    const GUID *clsid;
    VARIANT_BOOL bom;
    VARIANT_BOOL disable_escape;
    VARIANT_BOOL indent;
    VARIANT_BOOL omitdecl;
    VARIANT_BOOL standalone;
    const char *encoding;
};

static const struct mxwriter_props_t mxwriter_default_props[] =
{
    { &CLSID_MXXMLWriter,   VARIANT_TRUE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, "UTF-16" },
    { &CLSID_MXXMLWriter30, VARIANT_TRUE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, "UTF-16" },
    { &CLSID_MXXMLWriter40, VARIANT_TRUE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, "UTF-16" },
    { &CLSID_MXXMLWriter60, VARIANT_TRUE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, VARIANT_FALSE, "UTF-16" },
    { NULL }
};

static void test_mxwriter_default_properties(const struct mxwriter_props_t *table)
{
    int i = 0;

    while (table->clsid)
    {
        IMXWriter *writer;
        VARIANT_BOOL b;
        BSTR encoding;
        HRESULT hr;

        if (!is_clsid_supported(table->clsid, mxwriter_support_data))
        {
            table++;
            i++;
            continue;
        }

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
        EXPECT_HR(hr, S_OK);

        b = !table->bom;
        hr = IMXWriter_get_byteOrderMark(writer, &b);
        EXPECT_HR(hr, S_OK);
        ok(table->bom == b, "test %d: got BOM %d, expected %d\n", i, b, table->bom);

        b = !table->disable_escape;
        hr = IMXWriter_get_disableOutputEscaping(writer, &b);
        EXPECT_HR(hr, S_OK);
        ok(table->disable_escape == b, "test %d: got disable escape %d, expected %d\n", i, b,
           table->disable_escape);

        b = !table->indent;
        hr = IMXWriter_get_indent(writer, &b);
        EXPECT_HR(hr, S_OK);
        ok(table->indent == b, "test %d: got indent %d, expected %d\n", i, b, table->indent);

        b = !table->omitdecl;
        hr = IMXWriter_get_omitXMLDeclaration(writer, &b);
        EXPECT_HR(hr, S_OK);
        ok(table->omitdecl == b, "test %d: got omitdecl %d, expected %d\n", i, b, table->omitdecl);

        b = !table->standalone;
        hr = IMXWriter_get_standalone(writer, &b);
        EXPECT_HR(hr, S_OK);
        ok(table->standalone == b, "test %d: got standalone %d, expected %d\n", i, b, table->standalone);

        hr = IMXWriter_get_encoding(writer, &encoding);
        EXPECT_HR(hr, S_OK);
        ok(!lstrcmpW(encoding, _bstr_(table->encoding)), "test %d: got encoding %s, expected %s\n",
            i, wine_dbgstr_w(encoding), table->encoding);
        SysFreeString(encoding);

        IMXWriter_Release(writer);

        table++;
        i++;
    }
}

static void test_mxwriter_properties(void)
{
    static const WCHAR utf16W[] = {'U','T','F','-','1','6',0};
    static const WCHAR emptyW[] = {0};
    static const WCHAR testW[] = {'t','e','s','t',0};
    ISAXContentHandler *content;
    IMXWriter *writer;
    VARIANT_BOOL b;
    HRESULT hr;
    BSTR str, str2;
    VARIANT dest;

    test_mxwriter_default_properties(mxwriter_default_props);

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IMXWriter_get_disableOutputEscaping(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    hr = IMXWriter_get_byteOrderMark(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    hr = IMXWriter_get_indent(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    hr = IMXWriter_get_omitXMLDeclaration(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    hr = IMXWriter_get_standalone(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);

    /* set and check */
    hr = IMXWriter_put_standalone(writer, VARIANT_TRUE);
    ok(hr == S_OK, "got %08x\n", hr);

    b = VARIANT_FALSE;
    hr = IMXWriter_get_standalone(writer, &b);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(b == VARIANT_TRUE, "got %d\n", b);

    hr = IMXWriter_get_encoding(writer, NULL);
    EXPECT_HR(hr, E_POINTER);

    /* UTF-16 is a default setting apparently */
    str = (void*)0xdeadbeef;
    hr = IMXWriter_get_encoding(writer, &str);
    EXPECT_HR(hr, S_OK);
    ok(lstrcmpW(str, utf16W) == 0, "expected empty string, got %s\n", wine_dbgstr_w(str));

    str2 = (void*)0xdeadbeef;
    hr = IMXWriter_get_encoding(writer, &str2);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(str != str2, "expected newly allocated, got same %p\n", str);

    SysFreeString(str2);
    SysFreeString(str);

    /* put empty string */
    str = SysAllocString(emptyW);
    hr = IMXWriter_put_encoding(writer, str);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);
    SysFreeString(str);

    str = (void*)0xdeadbeef;
    hr = IMXWriter_get_encoding(writer, &str);
    EXPECT_HR(hr, S_OK);
    ok(!lstrcmpW(str, _bstr_("UTF-16")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    /* invalid encoding name */
    str = SysAllocString(testW);
    hr = IMXWriter_put_encoding(writer, str);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);
    SysFreeString(str);

    /* test case sensivity */
    hr = IMXWriter_put_encoding(writer, _bstr_("utf-8"));
    EXPECT_HR(hr, S_OK);
    str = (void*)0xdeadbeef;
    hr = IMXWriter_get_encoding(writer, &str);
    EXPECT_HR(hr, S_OK);
    ok(!lstrcmpW(str, _bstr_("utf-8")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    hr = IMXWriter_put_encoding(writer, _bstr_("uTf-16"));
    EXPECT_HR(hr, S_OK);
    str = (void*)0xdeadbeef;
    hr = IMXWriter_get_encoding(writer, &str);
    EXPECT_HR(hr, S_OK);
    ok(!lstrcmpW(str, _bstr_("uTf-16")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    /* how it affects document creation */
    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);
    hr = ISAXContentHandler_endDocument(content);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"yes\"?>\r\n"),
        V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);
    ISAXContentHandler_Release(content);

    hr = IMXWriter_get_version(writer, NULL);
    ok(hr == E_POINTER, "got %08x\n", hr);
    /* default version is 'surprisingly' 1.0 */
    hr = IMXWriter_get_version(writer, &str);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!lstrcmpW(str, _bstr_("1.0")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    /* store version string as is */
    hr = IMXWriter_put_version(writer, NULL);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);

    hr = IMXWriter_put_version(writer, _bstr_("1.0"));
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IMXWriter_put_version(writer, _bstr_(""));
    ok(hr == S_OK, "got %08x\n", hr);
    hr = IMXWriter_get_version(writer, &str);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!lstrcmpW(str, _bstr_("")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    hr = IMXWriter_put_version(writer, _bstr_("a.b"));
    ok(hr == S_OK, "got %08x\n", hr);
    hr = IMXWriter_get_version(writer, &str);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!lstrcmpW(str, _bstr_("a.b")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    hr = IMXWriter_put_version(writer, _bstr_("2.0"));
    ok(hr == S_OK, "got %08x\n", hr);
    hr = IMXWriter_get_version(writer, &str);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!lstrcmpW(str, _bstr_("2.0")), "got %s\n", wine_dbgstr_w(str));
    SysFreeString(str);

    IMXWriter_Release(writer);
    free_bstrs();
}

static void test_mxwriter_flush(void)
{
    ISAXContentHandler *content;
    IMXWriter *writer;
    LARGE_INTEGER pos;
    ULARGE_INTEGER pos2;
    IStream *stream;
    VARIANT dest;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    EXPECT_REF(stream, 1);

    /* detach when nothing was attached */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    /* attach stream */
    V_VT(&dest) = VT_UNKNOWN;
    V_UNKNOWN(&dest) = (IUnknown*)stream;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    todo_wine EXPECT_REF(stream, 3);

    /* detach setting VT_EMPTY destination */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    EXPECT_REF(stream, 1);

    V_VT(&dest) = VT_UNKNOWN;
    V_UNKNOWN(&dest) = (IUnknown*)stream;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    /* flush() doesn't detach a stream */
    hr = IMXWriter_flush(writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    todo_wine EXPECT_REF(stream, 3);

    pos.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_CUR, &pos2);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(pos2.QuadPart == 0, "expected stream beginning\n");

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    pos.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_CUR, &pos2);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(pos2.QuadPart != 0, "expected stream beginning\n");

    /* already started */
    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_endDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    /* flushed on endDocument() */
    pos.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_CUR, &pos2);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(pos2.QuadPart != 0, "expected stream position moved\n");

    ISAXContentHandler_Release(content);
    IStream_Release(stream);
    IMXWriter_Release(writer);
}

static void test_mxwriter_startenddocument(void)
{
    ISAXContentHandler *content;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_endDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"no\"?>\r\n"), V_BSTR(&dest)),
        "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    /* now try another startDocument */
    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);
    /* and get duplicated prolog */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"no\"?>\r\n"
                        "<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"no\"?>\r\n"), V_BSTR(&dest)),
        "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    /* now with omitted declaration */
    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_endDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_(""), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    free_bstrs();
}

enum startendtype
{
    StartElement,
    EndElement,
    StartEndElement
};

struct writer_startendelement_t {
    const GUID *clsid;
    enum startendtype type;
    const char *uri;
    const char *local_name;
    const char *qname;
    const char *output;
    HRESULT hr;
    ISAXAttributes *attr;
};

static const char startelement_xml[] = "<uri:local a:attr1=\"a1\" attr2=\"a2\" attr3=\"&lt;&amp;&quot;&gt;\">";
static const char startendelement_xml[] = "<uri:local a:attr1=\"a1\" attr2=\"a2\" attr3=\"&lt;&amp;&quot;&gt;\"/>";

static const struct writer_startendelement_t writer_startendelement[] = {
    /* 0 */
    { &CLSID_MXXMLWriter,   StartElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, StartElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, StartElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, StartElement, NULL, NULL, NULL, "<>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    /* 5 */
    { &CLSID_MXXMLWriter30, StartElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, StartElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, StartElement, "uri", NULL, NULL, "<>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, StartElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    /* 10 */
    { &CLSID_MXXMLWriter40, StartElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, StartElement, NULL, "local", NULL, "<>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, StartElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, StartElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    /* 15 */
    { &CLSID_MXXMLWriter60, StartElement, NULL, NULL, "qname", "<qname>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, "uri", "local", "qname", "<qname>", S_OK },
    { &CLSID_MXXMLWriter30, StartElement, "uri", "local", "qname", "<qname>", S_OK },
    { &CLSID_MXXMLWriter40, StartElement, "uri", "local", "qname", "<qname>", S_OK },
    { &CLSID_MXXMLWriter60, StartElement, "uri", "local", "qname", "<qname>", S_OK },
    /* 20 */
    { &CLSID_MXXMLWriter,   StartElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, StartElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, StartElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, StartElement, "uri", "local", NULL, "<>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, "uri", "local", "uri:local", "<uri:local>", S_OK },
    /* 25 */
    { &CLSID_MXXMLWriter30, StartElement, "uri", "local", "uri:local", "<uri:local>", S_OK },
    { &CLSID_MXXMLWriter40, StartElement, "uri", "local", "uri:local", "<uri:local>", S_OK },
    { &CLSID_MXXMLWriter60, StartElement, "uri", "local", "uri:local", "<uri:local>", S_OK },
    { &CLSID_MXXMLWriter,   StartElement, "uri", "local", "uri:local2", "<uri:local2>", S_OK },
    { &CLSID_MXXMLWriter30, StartElement, "uri", "local", "uri:local2", "<uri:local2>", S_OK },
    /* 30 */
    { &CLSID_MXXMLWriter40, StartElement, "uri", "local", "uri:local2", "<uri:local2>", S_OK },
    { &CLSID_MXXMLWriter60, StartElement, "uri", "local", "uri:local2", "<uri:local2>", S_OK },
    /* endElement tests */
    { &CLSID_MXXMLWriter,   EndElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, EndElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, EndElement, NULL, NULL, NULL, NULL, E_INVALIDARG },
    /* 35 */
    { &CLSID_MXXMLWriter60, EndElement, NULL, NULL, NULL, "</>", S_OK },
    { &CLSID_MXXMLWriter,   EndElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, EndElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, EndElement, "uri", NULL, NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, EndElement, "uri", NULL, NULL, "</>", S_OK },
    /* 40 */
    { &CLSID_MXXMLWriter,   EndElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, EndElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, EndElement, NULL, "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, EndElement, NULL, "local", NULL, "</>", S_OK },
    { &CLSID_MXXMLWriter,   EndElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    /* 45 */
    { &CLSID_MXXMLWriter30, EndElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, EndElement, NULL, NULL, "qname", NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter60, EndElement, NULL, NULL, "qname", "</qname>", S_OK },
    { &CLSID_MXXMLWriter,   EndElement, "uri", "local", "qname", "</qname>", S_OK },
    { &CLSID_MXXMLWriter30, EndElement, "uri", "local", "qname", "</qname>", S_OK },
    /* 50 */
    { &CLSID_MXXMLWriter40, EndElement, "uri", "local", "qname", "</qname>", S_OK },
    { &CLSID_MXXMLWriter60, EndElement, "uri", "local", "qname", "</qname>", S_OK },
    { &CLSID_MXXMLWriter,   EndElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter30, EndElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    { &CLSID_MXXMLWriter40, EndElement, "uri", "local", NULL, NULL, E_INVALIDARG },
    /* 55 */
    { &CLSID_MXXMLWriter60, EndElement, "uri", "local", NULL, "</>", S_OK },
    { &CLSID_MXXMLWriter,   EndElement, "uri", "local", "uri:local", "</uri:local>", S_OK },
    { &CLSID_MXXMLWriter30, EndElement, "uri", "local", "uri:local", "</uri:local>", S_OK },
    { &CLSID_MXXMLWriter40, EndElement, "uri", "local", "uri:local", "</uri:local>", S_OK },
    { &CLSID_MXXMLWriter60, EndElement, "uri", "local", "uri:local", "</uri:local>", S_OK },
    /* 60 */
    { &CLSID_MXXMLWriter,   EndElement, "uri", "local", "uri:local2", "</uri:local2>", S_OK },
    { &CLSID_MXXMLWriter30, EndElement, "uri", "local", "uri:local2", "</uri:local2>", S_OK },
    { &CLSID_MXXMLWriter40, EndElement, "uri", "local", "uri:local2", "</uri:local2>", S_OK },
    { &CLSID_MXXMLWriter60, EndElement, "uri", "local", "uri:local2", "</uri:local2>", S_OK },

    /* with attributes */
    { &CLSID_MXXMLWriter,   StartElement, "uri", "local", "uri:local", startelement_xml, S_OK, &saxattributes },
    /* 65 */
    { &CLSID_MXXMLWriter30, StartElement, "uri", "local", "uri:local", startelement_xml, S_OK, &saxattributes },
    { &CLSID_MXXMLWriter40, StartElement, "uri", "local", "uri:local", startelement_xml, S_OK, &saxattributes },
    { &CLSID_MXXMLWriter60, StartElement, "uri", "local", "uri:local", startelement_xml, S_OK, &saxattributes },
    /* empty elements */
    { &CLSID_MXXMLWriter,   StartEndElement, "uri", "local", "uri:local", startendelement_xml, S_OK, &saxattributes },
    { &CLSID_MXXMLWriter30, StartEndElement, "uri", "local", "uri:local", startendelement_xml, S_OK, &saxattributes },
    /* 70 */
    { &CLSID_MXXMLWriter40, StartEndElement, "uri", "local", "uri:local", startendelement_xml, S_OK, &saxattributes },
    { &CLSID_MXXMLWriter60, StartEndElement, "uri", "local", "uri:local", startendelement_xml, S_OK, &saxattributes },
    { &CLSID_MXXMLWriter,   StartEndElement, "", "", "", "</>", S_OK },
    { &CLSID_MXXMLWriter30, StartEndElement, "", "", "", "</>", S_OK },
    { &CLSID_MXXMLWriter40, StartEndElement, "", "", "", "</>", S_OK },
    /* 75 */
    { &CLSID_MXXMLWriter60, StartEndElement, "", "", "", "</>", S_OK },
    { NULL }
};

static void get_mxwriter_support_data(struct msxmlsupported_data_t *table)
{
    while (table->clsid)
    {
        IMXWriter *writer;
        HRESULT hr;

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
        if (hr == S_OK) IMXWriter_Release(writer);

        table->supported = hr == S_OK;
        if (hr != S_OK) win_skip("class %s not supported\n", table->name);

        table++;
    }
}

static void get_mxattributes_support_data(struct msxmlsupported_data_t *table)
{
    while (table->clsid)
    {
        IMXAttributes *attr;
        HRESULT hr;

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXAttributes, (void**)&attr);
        if (hr == S_OK) IMXAttributes_Release(attr);

        table->supported = hr == S_OK;
        if (hr != S_OK) skip("class %s not supported\n", table->name);

        table++;
    }
}

static void test_mxwriter_startendelement_batch(const struct writer_startendelement_t *table)
{
    int i = 0;

    while (table->clsid)
    {
        ISAXContentHandler *content;
        IMXWriter *writer;
        HRESULT hr;

        if (!is_clsid_supported(table->clsid, mxwriter_support_data))
        {
            table++;
            i++;
            continue;
        }

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
        EXPECT_HR(hr, S_OK);

        hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
        EXPECT_HR(hr, S_OK);

        hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
        EXPECT_HR(hr, S_OK);

        hr = ISAXContentHandler_startDocument(content);
        EXPECT_HR(hr, S_OK);

        if (table->type == StartElement)
        {
            hr = ISAXContentHandler_startElement(content, _bstr_(table->uri), lstrlen(table->uri),
                _bstr_(table->local_name), lstrlen(table->local_name), _bstr_(table->qname), lstrlen(table->qname), table->attr);
            ok(hr == table->hr, "test %d: got 0x%08x, expected 0x%08x\n", i, hr, table->hr);
        }
        else if (table->type == EndElement)
        {
            hr = ISAXContentHandler_endElement(content, _bstr_(table->uri), lstrlen(table->uri),
                _bstr_(table->local_name), lstrlen(table->local_name), _bstr_(table->qname), lstrlen(table->qname));
            ok(hr == table->hr, "test %d: got 0x%08x, expected 0x%08x\n", i, hr, table->hr);
        }
        else
        {
            hr = ISAXContentHandler_startElement(content, _bstr_(table->uri), lstrlen(table->uri),
                _bstr_(table->local_name), lstrlen(table->local_name), _bstr_(table->qname), lstrlen(table->qname), table->attr);
            ok(hr == table->hr, "test %d: got 0x%08x, expected 0x%08x\n", i, hr, table->hr);
            hr = ISAXContentHandler_endElement(content, _bstr_(table->uri), lstrlen(table->uri),
                _bstr_(table->local_name), lstrlen(table->local_name), _bstr_(table->qname), lstrlen(table->qname));
            ok(hr == table->hr, "test %d: got 0x%08x, expected 0x%08x\n", i, hr, table->hr);
        }

        /* test output */
        if (hr == S_OK)
        {
            VARIANT dest;

            V_VT(&dest) = VT_EMPTY;
            hr = IMXWriter_get_output(writer, &dest);
            EXPECT_HR(hr, S_OK);
            ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
            ok(!lstrcmpW(_bstr_(table->output), V_BSTR(&dest)),
                "test %d: got wrong content %s, expected %s\n", i, wine_dbgstr_w(V_BSTR(&dest)), table->output);
            VariantClear(&dest);
        }

        ISAXContentHandler_Release(content);
        IMXWriter_Release(writer);

        table++;
        i++;
    }

    free_bstrs();
}

static void test_mxwriter_startendelement(void)
{
    ISAXContentHandler *content;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;

    test_mxwriter_startendelement_batch(writer_startendelement);

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "got %08x\n", hr);

    /* all string pointers should be not null */
    hr = ISAXContentHandler_startElement(content, _bstr_(""), 0, _bstr_("b"), 1, _bstr_(""), 0, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXContentHandler_startElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("b"), 1, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<><b>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXContentHandler_endElement(content, NULL, 0, NULL, 0, _bstr_("a:b"), 3);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXContentHandler_endElement(content, NULL, 0, _bstr_("b"), 1, _bstr_("a:b"), 3);
    EXPECT_HR(hr, E_INVALIDARG);

    /* only local name is an error too */
    hr = ISAXContentHandler_endElement(content, NULL, 0, _bstr_("b"), 1, NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXContentHandler_endElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("b"), 1);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<><b></b>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXContentHandler_endDocument(content);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_put_output(writer, dest);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_(""), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("abcdef"), 3, NULL);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<abc>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_endDocument(content);
    IMXWriter_flush(writer);

    hr = ISAXContentHandler_endElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("abdcdef"), 3);
    EXPECT_HR(hr, S_OK);
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<abc></abd>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);
    free_bstrs();
}

struct writer_characters_t {
    const GUID *clsid;
    const char *data;
    const char *output;
};

static const struct writer_characters_t writer_characters[] = {
    { &CLSID_MXXMLWriter,   "< > & \"", "&lt; &gt; &amp; \"" },
    { &CLSID_MXXMLWriter30, "< > & \"", "&lt; &gt; &amp; \"" },
    { &CLSID_MXXMLWriter40, "< > & \"", "&lt; &gt; &amp; \"" },
    { &CLSID_MXXMLWriter60, "< > & \"", "&lt; &gt; &amp; \"" },
    { NULL }
};

static void test_mxwriter_characters(void)
{
    static const WCHAR chardataW[] = {'T','E','S','T','C','H','A','R','D','A','T','A',' ','.',0};
    const struct writer_characters_t *table = writer_characters;
    ISAXContentHandler *content;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;
    int i = 0;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_characters(content, NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXContentHandler_characters(content, chardataW, 0);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_characters(content, chardataW, sizeof(chardataW)/sizeof(WCHAR) - 1);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("TESTCHARDATA ."), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXContentHandler_endDocument(content);
    EXPECT_HR(hr, S_OK);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    /* try empty characters data to see if element is closed */
    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("a"), 1, NULL);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_characters(content, chardataW, 0);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_endElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("a"), 1);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<a></a>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    /* batch tests */
    while (table->clsid)
    {
        ISAXContentHandler *content;
        IMXWriter *writer;
        HRESULT hr;

        if (!is_clsid_supported(table->clsid, mxwriter_support_data))
        {
            table++;
            i++;
            continue;
        }

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
        EXPECT_HR(hr, S_OK);

        hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
        EXPECT_HR(hr, S_OK);

        hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
        EXPECT_HR(hr, S_OK);

        hr = ISAXContentHandler_startDocument(content);
        EXPECT_HR(hr, S_OK);

        hr = ISAXContentHandler_characters(content, _bstr_(table->data), strlen(table->data));
        EXPECT_HR(hr, S_OK);

        /* test output */
        if (hr == S_OK)
        {
            VARIANT dest;

            V_VT(&dest) = VT_EMPTY;
            hr = IMXWriter_get_output(writer, &dest);
            EXPECT_HR(hr, S_OK);
            ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
            ok(!lstrcmpW(_bstr_(table->output), V_BSTR(&dest)),
                "test %d: got wrong content %s, expected %s\n", i, wine_dbgstr_w(V_BSTR(&dest)), table->output);
            VariantClear(&dest);
        }

        table++;
        i++;
    }

    free_bstrs();
}

static const mxwriter_stream_test mxwriter_stream_tests[] = {
    {
        VARIANT_TRUE,"UTF-16",
        {
            {FALSE,(const BYTE*)szUtf16BOM,sizeof(szUtf16BOM),TRUE},
            {FALSE,(const BYTE*)szUtf16XML,sizeof(szUtf16XML)},
            {TRUE}
        }
    },
    {
        VARIANT_FALSE,"UTF-16",
        {
            {FALSE,(const BYTE*)szUtf16XML,sizeof(szUtf16XML)},
            {TRUE}
        }
    },
    {
        VARIANT_TRUE,"UTF-8",
        {
            {FALSE,(const BYTE*)szUtf8XML,sizeof(szUtf8XML)-1},
            /* For some reason Windows makes an empty write call when UTF-8 encoding is used
             * and the writer is released.
             */
            {FALSE,NULL,0},
            {TRUE}
        }
    },
    {
        VARIANT_TRUE,"utf-8",
        {
            {FALSE,(const BYTE*)utf8xml2,sizeof(utf8xml2)-1},
            /* For some reason Windows makes an empty write call when UTF-8 encoding is used
             * and the writer is released.
             */
            {FALSE,NULL,0},
            {TRUE}
        }
    },
    {
        VARIANT_TRUE,"UTF-16",
        {
            {FALSE,(const BYTE*)szUtf16BOM,sizeof(szUtf16BOM),TRUE},
            {FALSE,(const BYTE*)szUtf16XML,sizeof(szUtf16XML)},
            {TRUE}
        }
    },
    {
        VARIANT_TRUE,"UTF-16",
        {
            {FALSE,(const BYTE*)szUtf16BOM,sizeof(szUtf16BOM),TRUE,TRUE},
            {FALSE,(const BYTE*)szUtf16XML,sizeof(szUtf16XML)},
            {TRUE}
        }
    }
};

static void test_mxwriter_stream(void)
{
    IMXWriter *writer;
    ISAXContentHandler *content;
    HRESULT hr;
    VARIANT dest;
    IStream *stream;
    LARGE_INTEGER pos;
    ULARGE_INTEGER pos2;
    DWORD test_count = sizeof(mxwriter_stream_tests)/sizeof(mxwriter_stream_tests[0]);

    for(current_stream_test_index = 0; current_stream_test_index < test_count; ++current_stream_test_index) {
        const mxwriter_stream_test *test = mxwriter_stream_tests+current_stream_test_index;

        hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
                &IID_IMXWriter, (void**)&writer);
        ok(hr == S_OK, "CoCreateInstance failed: %08x\n", hr);

        hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
        ok(hr == S_OK, "QueryInterface(ISAXContentHandler) failed: %08x\n", hr);

        hr = IMXWriter_put_encoding(writer, _bstr_(test->encoding));
        ok(hr == S_OK, "put_encoding failed with %08x on test %d\n", hr, current_stream_test_index);

        V_VT(&dest) = VT_UNKNOWN;
        V_UNKNOWN(&dest) = (IUnknown*)&mxstream;
        hr = IMXWriter_put_output(writer, dest);
        ok(hr == S_OK, "put_output failed with %08x on test %d\n", hr, current_stream_test_index);
        VariantClear(&dest);

        hr = IMXWriter_put_byteOrderMark(writer, test->bom);
        ok(hr == S_OK, "put_byteOrderMark failed with %08x on test %d\n", hr, current_stream_test_index);

        current_write_test = test->expected_writes;

        hr = ISAXContentHandler_startDocument(content);
        ok(hr == S_OK, "startDocument failed with %08x on test %d\n", hr, current_stream_test_index);

        hr = ISAXContentHandler_endDocument(content);
        ok(hr == S_OK, "endDocument failed with %08x on test %d\n", hr, current_stream_test_index);

        ISAXContentHandler_Release(content);
        IMXWriter_Release(writer);

        ok(current_write_test->last, "The last %d write calls on test %d were missed\n",
            (int)(current_write_test-test->expected_writes), current_stream_test_index);
    }

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    ok(hr == S_OK, "CoCreateInstance failed: %08x\n", hr);

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    ok(hr == S_OK, "CreateStreamOnHGlobal failed: %08x\n", hr);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    ok(hr == S_OK, "QueryInterface(ISAXContentHandler) failed: %08x\n", hr);

    hr = IMXWriter_put_encoding(writer, _bstr_("UTF-8"));
    ok(hr == S_OK, "put_encoding failed: %08x\n", hr);

    V_VT(&dest) = VT_UNKNOWN;
    V_UNKNOWN(&dest) = (IUnknown*)stream;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "put_output failed: %08x\n", hr);

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "startDocument failed: %08x\n", hr);

    /* Setting output of the mxwriter causes the current output to be flushed,
     * and the writer to start over.
     */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_put_output(writer, dest);
    ok(hr == S_OK, "put_output failed: %08x\n", hr);

    pos.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_CUR, &pos2);
    ok(hr == S_OK, "Seek failed: %08x\n", hr);
    ok(pos2.QuadPart != 0, "expected stream position moved\n");

    hr = ISAXContentHandler_startDocument(content);
    ok(hr == S_OK, "startDocument failed: %08x\n", hr);

    hr = ISAXContentHandler_endDocument(content);
    ok(hr == S_OK, "endDocument failed: %08x\n", hr);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    ok(hr == S_OK, "get_output failed: %08x\n", hr);
    ok(V_VT(&dest) == VT_BSTR, "Expected VT_BSTR, got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"no\"?>\r\n"), V_BSTR(&dest)),
            "Got wrong content: %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    /* test when BOM is written to output stream */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_put_output(writer, dest);
    EXPECT_HR(hr, S_OK);

    pos.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_SET, NULL);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_UNKNOWN;
    V_UNKNOWN(&dest) = (IUnknown*)stream;
    hr = IMXWriter_put_output(writer, dest);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_byteOrderMark(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_encoding(writer, _bstr_("UTF-16"));
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    pos.QuadPart = 0;
    pos2.QuadPart = 0;
    hr = IStream_Seek(stream, pos, STREAM_SEEK_CUR, &pos2);
    EXPECT_HR(hr, S_OK);
    ok(pos2.QuadPart == 2, "got wrong position\n");

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    free_bstrs();
}

static void test_mxwriter_encoding(void)
{
    ISAXContentHandler *content;
    IMXWriter *writer;
    IStream *stream;
    VARIANT dest;
    HRESULT hr;
    HGLOBAL g;
    char *ptr;
    BSTR s;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_encoding(writer, _bstr_("UTF-8"));
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_endDocument(content);
    EXPECT_HR(hr, S_OK);

    /* The content is always re-encoded to UTF-16 when the output is
     * retrieved as a BSTR.
     */
    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "Expected VT_BSTR, got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"no\"?>\r\n"), V_BSTR(&dest)),
            "got wrong content: %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    /* switch encoding when something is written already */
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_UNKNOWN;
    V_UNKNOWN(&dest) = (IUnknown*)stream;
    hr = IMXWriter_put_output(writer, dest);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_encoding(writer, _bstr_("UTF-8"));
    EXPECT_HR(hr, S_OK);

    /* write empty element */
    hr = ISAXContentHandler_startElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("a"), 1, NULL);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_endElement(content, _bstr_(""), 0, _bstr_(""), 0, _bstr_("a"), 1);
    EXPECT_HR(hr, S_OK);

    /* switch */
    hr = IMXWriter_put_encoding(writer, _bstr_("UTF-16"));
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_flush(writer);
    EXPECT_HR(hr, S_OK);

    hr = GetHGlobalFromStream(stream, &g);
    EXPECT_HR(hr, S_OK);

    ptr = GlobalLock(g);
    ok(!strncmp(ptr, "<a/>", 4), "got %c%c%c%c\n", ptr[0],ptr[1],ptr[2],ptr[3]);
    GlobalUnlock(g);

    /* so output is unaffected, encoding name is stored however */
    hr = IMXWriter_get_encoding(writer, &s);
    EXPECT_HR(hr, S_OK);
    ok(!lstrcmpW(s, _bstr_("UTF-16")), "got %s\n", wine_dbgstr_w(s));
    SysFreeString(s);

    IStream_Release(stream);

    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);

    free_bstrs();
}

static void test_obj_dispex(IUnknown *obj)
{
    static const WCHAR starW[] = {'*',0};
    DISPID dispid = DISPID_SAX_XMLREADER_GETFEATURE;
    IDispatchEx *dispex;
    IUnknown *unk;
    DWORD props;
    UINT ticnt;
    HRESULT hr;
    BSTR name;

    hr = IUnknown_QueryInterface(obj, &IID_IDispatchEx, (void**)&dispex);
    EXPECT_HR(hr, S_OK);
    if (FAILED(hr)) return;

    ticnt = 0;
    hr = IDispatchEx_GetTypeInfoCount(dispex, &ticnt);
    EXPECT_HR(hr, S_OK);
    ok(ticnt == 1, "ticnt=%u\n", ticnt);

    name = SysAllocString(starW);
    hr = IDispatchEx_DeleteMemberByName(dispex, name, fdexNameCaseSensitive);
    EXPECT_HR(hr, E_NOTIMPL);
    SysFreeString(name);

    hr = IDispatchEx_DeleteMemberByDispID(dispex, dispid);
    EXPECT_HR(hr, E_NOTIMPL);

    props = 0;
    hr = IDispatchEx_GetMemberProperties(dispex, dispid, grfdexPropCanAll, &props);
    EXPECT_HR(hr, E_NOTIMPL);
    ok(props == 0, "expected 0 got %d\n", props);

    hr = IDispatchEx_GetMemberName(dispex, dispid, &name);
    EXPECT_HR(hr, E_NOTIMPL);
    if (SUCCEEDED(hr)) SysFreeString(name);

    hr = IDispatchEx_GetNextDispID(dispex, fdexEnumDefault, DISPID_SAX_XMLREADER_GETFEATURE, &dispid);
    EXPECT_HR(hr, E_NOTIMPL);

    hr = IDispatchEx_GetNameSpaceParent(dispex, &unk);
    EXPECT_HR(hr, E_NOTIMPL);
    if (hr == S_OK && unk) IUnknown_Release(unk);

    IDispatchEx_Release(dispex);
}

static void test_dispex(void)
{
     IVBSAXXMLReader *vbreader;
     ISAXXMLReader *reader;
     IUnknown *unk;
     HRESULT hr;

     hr = CoCreateInstance(&CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER,
                &IID_ISAXXMLReader, (void**)&reader);
     EXPECT_HR(hr, S_OK);

     hr = ISAXXMLReader_QueryInterface(reader, &IID_IUnknown, (void**)&unk);
     EXPECT_HR(hr, S_OK);
     test_obj_dispex(unk);
     IUnknown_Release(unk);

     hr = ISAXXMLReader_QueryInterface(reader, &IID_IVBSAXXMLReader, (void**)&vbreader);
     EXPECT_HR(hr, S_OK);
     hr = IVBSAXXMLReader_QueryInterface(vbreader, &IID_IUnknown, (void**)&unk);
     EXPECT_HR(hr, S_OK);
     test_obj_dispex(unk);
     IUnknown_Release(unk);
     IVBSAXXMLReader_Release(vbreader);

     ISAXXMLReader_Release(reader);
}

static void test_mxwriter_dispex(void)
{
    IDispatchEx *dispex;
    IMXWriter *writer;
    IUnknown *unk;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_IDispatchEx, (void**)&dispex);
    EXPECT_HR(hr, S_OK);
    hr = IDispatchEx_QueryInterface(dispex, &IID_IUnknown, (void**)&unk);
    test_obj_dispex(unk);
    IUnknown_Release(unk);
    IDispatchEx_Release(dispex);

    IMXWriter_Release(writer);
}

static void test_mxwriter_comment(void)
{
    static const WCHAR commentW[] = {'c','o','m','m','e','n','t',0};
    ISAXContentHandler *content;
    ISAXLexicalHandler *lexical;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXLexicalHandler, (void**)&lexical);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXLexicalHandler_comment(lexical, NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_comment(lexical, commentW, 0);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<!---->\r\n"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXLexicalHandler_comment(lexical, commentW, sizeof(commentW)/sizeof(WCHAR)-1);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<!---->\r\n<!--comment-->\r\n"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    ISAXLexicalHandler_Release(lexical);
    IMXWriter_Release(writer);
    free_bstrs();
}

static void test_mxwriter_cdata(void)
{
    ISAXContentHandler *content;
    ISAXLexicalHandler *lexical;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXLexicalHandler, (void**)&lexical);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXLexicalHandler_startCDATA(lexical);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<![CDATA["), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXLexicalHandler_startCDATA(lexical);
    EXPECT_HR(hr, S_OK);

    /* all these are escaped for text nodes */
    hr = ISAXContentHandler_characters(content, _bstr_("< > & \""), 7);
    EXPECT_HR(hr, S_OK);

    hr = ISAXLexicalHandler_endCDATA(lexical);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<![CDATA[<![CDATA[< > & \"]]>"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    ISAXLexicalHandler_Release(lexical);
    IMXWriter_Release(writer);
    free_bstrs();
}

static void test_mxwriter_dtd(void)
{
    static const WCHAR nameW[] = {'n','a','m','e'};
    static const WCHAR pubW[] = {'p','u','b'};
    static const WCHAR sysW[] = {'s','y','s'};
    ISAXContentHandler *content;
    ISAXLexicalHandler *lexical;
    IMXWriter *writer;
    VARIANT dest;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MXXMLWriter, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXWriter, (void**)&writer);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void**)&content);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_QueryInterface(writer, &IID_ISAXLexicalHandler, (void**)&lexical);
    EXPECT_HR(hr, S_OK);

    hr = IMXWriter_put_omitXMLDeclaration(writer, VARIANT_TRUE);
    EXPECT_HR(hr, S_OK);

    hr = ISAXContentHandler_startDocument(content);
    EXPECT_HR(hr, S_OK);

    hr = ISAXLexicalHandler_startDTD(lexical, NULL, 0, NULL, 0, NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_startDTD(lexical, NULL, 0, pubW, sizeof(pubW)/sizeof(WCHAR), NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_startDTD(lexical, NULL, 0, NULL, 0, sysW, sizeof(sysW)/sizeof(WCHAR));
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_startDTD(lexical, NULL, 0, pubW, sizeof(pubW)/sizeof(WCHAR), sysW, sizeof(sysW)/sizeof(WCHAR));
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_startDTD(lexical, nameW, sizeof(nameW)/sizeof(WCHAR), NULL, 0, NULL, 0);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<!DOCTYPE name [\r\n"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    /* system id is required if public is present */
    hr = ISAXLexicalHandler_startDTD(lexical, nameW, sizeof(nameW)/sizeof(WCHAR), pubW, sizeof(pubW)/sizeof(WCHAR), NULL, 0);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXLexicalHandler_startDTD(lexical, nameW, sizeof(nameW)/sizeof(WCHAR),
        pubW, sizeof(pubW)/sizeof(WCHAR), sysW, sizeof(sysW)/sizeof(WCHAR));
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<!DOCTYPE name [\r\n<!DOCTYPE name PUBLIC \"pub\""
        "<!DOCTYPE name PUBLIC \"pub\" \"sys\" [\r\n"), V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    hr = ISAXLexicalHandler_endDTD(lexical);
    EXPECT_HR(hr, S_OK);

    hr = ISAXLexicalHandler_endDTD(lexical);
    EXPECT_HR(hr, S_OK);

    V_VT(&dest) = VT_EMPTY;
    hr = IMXWriter_get_output(writer, &dest);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&dest) == VT_BSTR, "got %d\n", V_VT(&dest));
    ok(!lstrcmpW(_bstr_("<!DOCTYPE name [\r\n<!DOCTYPE name PUBLIC \"pub\""
         "<!DOCTYPE name PUBLIC \"pub\" \"sys\" [\r\n]>\r\n]>\r\n"),
        V_BSTR(&dest)), "got wrong content %s\n", wine_dbgstr_w(V_BSTR(&dest)));
    VariantClear(&dest);

    ISAXContentHandler_Release(content);
    ISAXLexicalHandler_Release(lexical);
    IMXWriter_Release(writer);
    free_bstrs();
}

typedef struct {
    const CLSID *clsid;
    const char *uri;
    const char *local;
    const char *qname;
    const char *type;
    const char *value;
    HRESULT hr;
} addattribute_test_t;

static const addattribute_test_t addattribute_data[] = {
    { &CLSID_SAXAttributes,   NULL, NULL, "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes30, NULL, NULL, "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes40, NULL, NULL, "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes60, NULL, NULL, "ns:qname", NULL, "value", S_OK },

    { &CLSID_SAXAttributes,   NULL, "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes30, NULL, "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes40, NULL, "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes60, NULL, "qname", "ns:qname", NULL, "value", S_OK },

    { &CLSID_SAXAttributes,   "uri", "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes30, "uri", "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes40, "uri", "qname", "ns:qname", NULL, "value", E_INVALIDARG },
    { &CLSID_SAXAttributes60, "uri", "qname", "ns:qname", NULL, "value", S_OK },

    { &CLSID_SAXAttributes,   "uri", "qname", "ns:qname", "type", "value", S_OK },
    { &CLSID_SAXAttributes30, "uri", "qname", "ns:qname", "type", "value", S_OK },
    { &CLSID_SAXAttributes40, "uri", "qname", "ns:qname", "type", "value", S_OK },
    { &CLSID_SAXAttributes60, "uri", "qname", "ns:qname", "type", "value", S_OK },

    { NULL }
};

static void test_mxattr_addAttribute(void)
{
    const addattribute_test_t *table = addattribute_data;
    int i = 0;

    while (table->clsid)
    {
        ISAXAttributes *saxattr;
        IMXAttributes *mxattr;
        HRESULT hr;
        int len;

        if (!is_clsid_supported(table->clsid, mxattributes_support_data))
        {
            table++;
            i++;
            continue;
        }

        hr = CoCreateInstance(table->clsid, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMXAttributes, (void**)&mxattr);
        EXPECT_HR(hr, S_OK);

        hr = IMXAttributes_QueryInterface(mxattr, &IID_ISAXAttributes, (void**)&saxattr);
        EXPECT_HR(hr, S_OK);

        /* SAXAttributes30 and SAXAttributes60 both crash on this test */
        if (IsEqualGUID(table->clsid, &CLSID_SAXAttributes) ||
            IsEqualGUID(table->clsid, &CLSID_SAXAttributes30))
        {
            hr = ISAXAttributes_getLength(saxattr, NULL);
            EXPECT_HR(hr, E_POINTER);
        }

        len = -1;
        hr = ISAXAttributes_getLength(saxattr, &len);
        EXPECT_HR(hr, S_OK);
        ok(len == 0, "got %d\n", len);

        hr = IMXAttributes_addAttribute(mxattr, _bstr_(table->uri), _bstr_(table->local),
            _bstr_(table->qname), _bstr_(table->type), _bstr_(table->value));
        ok(hr == table->hr, "%d: got 0x%08x, expected 0x%08x\n", i, hr, table->hr);

        len = -1;
        hr = ISAXAttributes_getLength(saxattr, &len);
        EXPECT_HR(hr, S_OK);
        if (table->hr == S_OK)
            ok(len == 1, "%d: got %d length, expected 0\n", i, len);
        else
            ok(len == 0, "%d: got %d length, expected 1\n", i, len);

        ISAXAttributes_Release(saxattr);
        IMXAttributes_Release(mxattr);

        table++;
        i++;
    }

    free_bstrs();
}

static void test_mxattr_clear(void)
{
    ISAXAttributes *saxattr;
    IMXAttributes *mxattr;
    const WCHAR *ptr;
    HRESULT hr;
    int len;

    hr = CoCreateInstance(&CLSID_SAXAttributes, NULL, CLSCTX_INPROC_SERVER,
        &IID_IMXAttributes, (void**)&mxattr);
    EXPECT_HR(hr, S_OK);

    hr = IMXAttributes_QueryInterface(mxattr, &IID_ISAXAttributes, (void**)&saxattr);
    EXPECT_HR(hr, S_OK);

    hr = ISAXAttributes_getQName(saxattr, 0, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = ISAXAttributes_getQName(saxattr, 0, &ptr, &len);
    EXPECT_HR(hr, E_INVALIDARG);

    hr = IMXAttributes_addAttribute(mxattr, _bstr_("uri"), _bstr_("local"),
        _bstr_("qname"), _bstr_("type"), _bstr_("value"));
    EXPECT_HR(hr, S_OK);

    len = -1;
    hr = ISAXAttributes_getLength(saxattr, &len);
    EXPECT_HR(hr, S_OK);
    ok(len == 1, "got %d\n", len);

    len = -1;
    hr = ISAXAttributes_getQName(saxattr, 0, NULL, &len);
    EXPECT_HR(hr, E_POINTER);
    ok(len == -1, "got %d\n", len);

    ptr = (void*)0xdeadbeef;
    hr = ISAXAttributes_getQName(saxattr, 0, &ptr, NULL);
    EXPECT_HR(hr, E_POINTER);
    ok(ptr == (void*)0xdeadbeef, "got %p\n", ptr);

    len = 0;
    hr = ISAXAttributes_getQName(saxattr, 0, &ptr, &len);
    EXPECT_HR(hr, S_OK);
    ok(len == 5, "got %d\n", len);
    ok(!lstrcmpW(ptr, _bstr_("qname")), "got %s\n", wine_dbgstr_w(ptr));

    hr = IMXAttributes_clear(mxattr);
    EXPECT_HR(hr, S_OK);

    len = -1;
    hr = ISAXAttributes_getLength(saxattr, &len);
    EXPECT_HR(hr, S_OK);
    ok(len == 0, "got %d\n", len);

    len = -1;
    ptr = (void*)0xdeadbeef;
    hr = ISAXAttributes_getQName(saxattr, 0, &ptr, &len);
    EXPECT_HR(hr, E_INVALIDARG);
    ok(len == -1, "got %d\n", len);
    ok(ptr == (void*)0xdeadbeef, "got %p\n", ptr);

    IMXAttributes_Release(mxattr);
    ISAXAttributes_Release(saxattr);
    free_bstrs();
}

START_TEST(saxreader)
{
    ISAXXMLReader *reader;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "failed to init com\n");

    hr = CoCreateInstance(&CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER,
            &IID_ISAXXMLReader, (void**)&reader);

    if(FAILED(hr))
    {
        skip("Failed to create SAXXMLReader instance\n");
        CoUninitialize();
        return;
    }
    ISAXXMLReader_Release(reader);

    test_saxreader(0);
    test_saxreader(3);
    test_saxreader(6);
    test_saxreader_properties();
    test_saxreader_features();
    test_encoding();
    test_dispex();

    /* MXXMLWriter tests */
    get_mxwriter_support_data(mxwriter_support_data);
    if (is_clsid_supported(&CLSID_MXXMLWriter, mxwriter_support_data))
    {
        test_mxwriter_handlers();
        test_mxwriter_startenddocument();
        test_mxwriter_startendelement();
        test_mxwriter_characters();
        test_mxwriter_comment();
        test_mxwriter_cdata();
        test_mxwriter_dtd();
        test_mxwriter_properties();
        test_mxwriter_flush();
        test_mxwriter_stream();
        test_mxwriter_encoding();
        test_mxwriter_dispex();
    }
    else
        win_skip("MXXMLWriter not supported\n");

    /* SAXAttributes tests */
    get_mxattributes_support_data(mxattributes_support_data);
    if (is_clsid_supported(&CLSID_SAXAttributes, mxattributes_support_data))
    {
        test_mxattr_addAttribute();
        test_mxattr_clear();
    }
    else
        skip("SAXAttributes not supported\n");

    CoUninitialize();
}
