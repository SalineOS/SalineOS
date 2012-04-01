/*
 * Registry editing UI functions.
 *
 * Copyright (C) 2003 Dimitrie O. Paun
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

#define WIN32_LEAN_AND_MEAN     /* Exclude rarely-used stuff from Windows headers */

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cderr.h>
#include <stdlib.h>
#include <stdio.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "wine/unicode.h"
#include "main.h"
#include "regproc.h"
#include "resource.h"

static const WCHAR* editValueName;
static WCHAR* stringValueData;
static BOOL isDecimal;

struct edit_params
{
    HKEY    hKey;
    LPCWSTR lpszValueName;
    void   *pData;
    LONG    cbData;
};

static INT vmessagebox(HWND hwnd, INT buttons, INT titleId, INT resId, va_list ap)
{
    static const WCHAR errorW[] = {'E','r','r','o','r',0};
    static const WCHAR unknownW[] = {'U','n','k','n','o','w','n',' ','e','r','r','o','r',' ','s','t','r','i','n','g','!',0};

    WCHAR title[256];
    WCHAR errfmt[1024];
    WCHAR errstr[1024];

    if (!LoadStringW(hInst, titleId, title, COUNT_OF(title))) lstrcpyW(title, errorW);
    if (!LoadStringW(hInst, resId, errfmt, COUNT_OF(errfmt))) lstrcpyW(errfmt, unknownW);

    vsnprintfW(errstr, COUNT_OF(errstr), errfmt, ap);

    return MessageBoxW(hwnd, errstr, title, buttons);
}

static INT messagebox(HWND hwnd, INT buttons, INT titleId, INT resId, ...)
{
    va_list ap;
    INT result;

    va_start(ap, resId);
    result = vmessagebox(hwnd, buttons, titleId, resId, ap);
    va_end(ap);

    return result;
}

void error(HWND hwnd, INT resId, ...)
{
    va_list ap;

    va_start(ap, resId);
    vmessagebox(hwnd, MB_OK | MB_ICONERROR, IDS_ERROR, resId, ap);
    va_end(ap);
}

static void error_code_messagebox(HWND hwnd, DWORD error_code)
{
    LPWSTR lpMsgBuf;
    DWORD status;
    WCHAR title[256];
    static WCHAR fallback[] = {'E','r','r','o','r',' ','d','i','s','p','l','a','y','i','n','g',' ','e','r','r','o','r',' ','m','e','s','s','a','g','e','.','\n',0};
    static const WCHAR title_error[] = {'E','r','r','o','r',0};

    if (!LoadStringW(hInst, IDS_ERROR, title, COUNT_OF(title)))
        lstrcpyW(title, title_error);
    status = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL, error_code, 0, (LPWSTR)&lpMsgBuf, 0, NULL);
    if (!status)
        lpMsgBuf = fallback;
    MessageBoxW(hwnd, lpMsgBuf, title, MB_OK | MB_ICONERROR);
    if (lpMsgBuf != fallback)
        LocalFree(lpMsgBuf);
}

static BOOL change_dword_base(HWND hwndDlg, BOOL toHex)
{
    static const WCHAR percent_u[] = {'%','u',0};
    static const WCHAR percent_x[] = {'%','x',0};

    WCHAR buf[128];
    DWORD val;

    if (!GetDlgItemTextW(hwndDlg, IDC_VALUE_DATA, buf, COUNT_OF(buf))) return FALSE;
    if (!swscanf(buf, toHex ? percent_u : percent_x, &val)) return FALSE;
    wsprintfW(buf, toHex ? percent_x : percent_u, val);
    return SetDlgItemTextW(hwndDlg, IDC_VALUE_DATA, buf);
}

static INT_PTR CALLBACK modify_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WCHAR* valueData;
    HWND hwndValue;
    int len;

    switch(uMsg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(hwndDlg, IDC_VALUE_NAME, editValueName);
        SetDlgItemTextW(hwndDlg, IDC_VALUE_DATA, stringValueData);
	CheckRadioButton(hwndDlg, IDC_DWORD_HEX, IDC_DWORD_DEC, isDecimal ? IDC_DWORD_DEC : IDC_DWORD_HEX);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_DWORD_HEX:
	    if (isDecimal && change_dword_base(hwndDlg, TRUE)) isDecimal = FALSE;
	break;
        case IDC_DWORD_DEC:
	    if (!isDecimal && change_dword_base(hwndDlg, FALSE)) isDecimal = TRUE;
	break;
        case IDOK:
            if ((hwndValue = GetDlgItem(hwndDlg, IDC_VALUE_DATA))) {
                len = GetWindowTextLengthW(hwndValue);
                if ((valueData = HeapReAlloc(GetProcessHeap(), 0, stringValueData, (len + 1) * sizeof(WCHAR)))) {
                    stringValueData = valueData;
                    if (!GetWindowTextW(hwndValue, stringValueData, len + 1))
                        *stringValueData = 0;
                }
            }
            /* Fall through */
        case IDCANCEL:
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
    }
    return FALSE;
}

static INT_PTR CALLBACK bin_modify_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    struct edit_params *params;
    LPBYTE pData;
    LONG cbData;
    LONG lRet;

    switch(uMsg) {
    case WM_INITDIALOG:
        params = (struct edit_params *)lParam;
        SetWindowLongPtrW(hwndDlg, DWLP_USER, (ULONG_PTR)params);
        if (params->lpszValueName)
            SetDlgItemTextW(hwndDlg, IDC_VALUE_NAME, params->lpszValueName);
        else
            SetDlgItemTextW(hwndDlg, IDC_VALUE_NAME, g_pszDefaultValueName);
        SendDlgItemMessageW(hwndDlg, IDC_VALUE_DATA, HEM_SETDATA, (WPARAM)params->cbData, (LPARAM)params->pData);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            params = (struct edit_params *)GetWindowLongPtrW(hwndDlg, DWLP_USER);
            cbData = SendDlgItemMessageW(hwndDlg, IDC_VALUE_DATA, HEM_GETDATA, 0, 0);
            pData = HeapAlloc(GetProcessHeap(), 0, cbData);

            if (pData)
            {
                SendDlgItemMessageW(hwndDlg, IDC_VALUE_DATA, HEM_GETDATA, (WPARAM)cbData, (LPARAM)pData);
                lRet = RegSetValueExW(params->hKey, params->lpszValueName, 0, REG_BINARY, pData, cbData);
            }
            else
                lRet = ERROR_OUTOFMEMORY;

            if (lRet == ERROR_SUCCESS)
                EndDialog(hwndDlg, 1);
            else
            {
                error_code_messagebox(hwndDlg, lRet);
                EndDialog(hwndDlg, 0);
            }
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL check_value(HWND hwnd, HKEY hKey, LPCWSTR valueName)
{
    WCHAR empty = 0;
    LONG lRet = RegQueryValueExW(hKey, valueName ? valueName : &empty, 0, NULL, 0, NULL);
    if(lRet != ERROR_SUCCESS) return FALSE;
    return TRUE;
}

static LPWSTR read_value(HWND hwnd, HKEY hKey, LPCWSTR valueName, DWORD *lpType, LONG *len)
{
    DWORD valueDataLen;
    LPWSTR buffer = NULL;
    LONG lRet;
	WCHAR empty = 0;

    lRet = RegQueryValueExW(hKey, valueName ? valueName : &empty, 0, lpType, 0, &valueDataLen);
    if (lRet != ERROR_SUCCESS) {
        if (lRet == ERROR_FILE_NOT_FOUND && !valueName) { /* no default value here, make it up */
            if (len) *len = 1;
            if (lpType) *lpType = REG_SZ;
            buffer = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR));
            *buffer = '\0';
            return buffer;
        }
        error(hwnd, IDS_BAD_VALUE, valueName);
        goto done;
    }
    if ( *lpType == REG_DWORD ) valueDataLen = sizeof(DWORD);
    if (!(buffer = HeapAlloc(GetProcessHeap(), 0, valueDataLen+sizeof(WCHAR)))) {
        error(hwnd, IDS_TOO_BIG_VALUE, valueDataLen);
        goto done;
    }
    lRet = RegQueryValueExW(hKey, valueName, 0, 0, (LPBYTE)buffer, &valueDataLen);
    if (lRet != ERROR_SUCCESS) {
        error(hwnd, IDS_BAD_VALUE, valueName);
        goto done;
    }
    if((valueDataLen % sizeof(WCHAR)) == 0)
        buffer[valueDataLen / sizeof(WCHAR)] = 0;
    if(len) *len = valueDataLen;
    return buffer;

done:
    HeapFree(GetProcessHeap(), 0, buffer);
    return NULL;
}

BOOL CreateKey(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath, LPWSTR keyName)
{
    BOOL result = FALSE;
    LONG lRet = ERROR_SUCCESS;
    HKEY retKey = NULL;
    WCHAR newKey[MAX_NEW_KEY_LEN - 4];
    int keyNum;
    HKEY hKey;
         
    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_CREATE_SUB_KEY, &hKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	goto done;
    }

    if (!LoadStringW(GetModuleHandleW(0), IDS_NEWKEY, newKey, COUNT_OF(newKey))) goto done;

    /* try to find out a name for the newly create key (max 100 times) */
    for (keyNum = 1; keyNum < 100; keyNum++) {
	wsprintfW(keyName, newKey, keyNum);
	lRet = RegOpenKeyW(hKey, keyName, &retKey);
	if (lRet != ERROR_SUCCESS) break;
	RegCloseKey(retKey);
    }
    if (lRet == ERROR_SUCCESS) goto done;
    
    lRet = RegCreateKeyW(hKey, keyName, &retKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	goto done;
    }

    result = TRUE;

done:
    RegCloseKey(retKey);
    return result;
}

BOOL ModifyValue(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath, LPCWSTR valueName)
{
    BOOL result = FALSE;
    DWORD type;
    LONG lRet;
    HKEY hKey;
    LONG len;

    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_READ | KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	return FALSE;
    }

    editValueName = valueName ? valueName : g_pszDefaultValueName;
    if(!(stringValueData = read_value(hwnd, hKey, valueName, &type, &len))) goto done;

    if ( (type == REG_SZ) || (type == REG_EXPAND_SZ) ) {
        if (DialogBoxW(0, MAKEINTRESOURCEW(IDD_EDIT_STRING), hwnd, modify_dlgproc) == IDOK) {
            lRet = RegSetValueExW(hKey, valueName, 0, type, (LPBYTE)stringValueData, (lstrlenW(stringValueData) + 1) * sizeof(WCHAR));
            if (lRet == ERROR_SUCCESS) result = TRUE;
            else error_code_messagebox(hwnd, lRet);
        }
    } else if ( type == REG_DWORD ) {
	const WCHAR u[] = {'%','u',0};
	const WCHAR x[] = {'%','x',0};
	wsprintfW(stringValueData, isDecimal ? u : x, *((DWORD*)stringValueData));
	if (DialogBoxW(0, MAKEINTRESOURCEW(IDD_EDIT_DWORD), hwnd, modify_dlgproc) == IDOK) {
	    DWORD val;
	    CHAR* valueA = GetMultiByteString(stringValueData);
	    if (sscanf(valueA, isDecimal ? "%u" : "%x", &val)) {
		lRet = RegSetValueExW(hKey, valueName, 0, type, (BYTE*)&val, sizeof(val));
		if (lRet == ERROR_SUCCESS) result = TRUE;
		else error_code_messagebox(hwnd, lRet);
	    }
	    HeapFree(GetProcessHeap(), 0, valueA);
	}
    } else if ( type == REG_BINARY ) {
        struct edit_params params;
        params.hKey = hKey;
        params.lpszValueName = valueName;
        params.pData = stringValueData;
        params.cbData = len;
        result = DialogBoxParamW(NULL, MAKEINTRESOURCEW(IDD_EDIT_BINARY), hwnd,
            bin_modify_dlgproc, (LPARAM)&params);
    } else if ( type == REG_MULTI_SZ ) {
        WCHAR char1 = '\r', char2 = '\n';
        WCHAR *tmpValueData = NULL;
        INT i, j, count;

        for ( i = 0, count = 0; i < len - 1; i++)
            if ( !stringValueData[i] && stringValueData[i + 1] )
                count++;
        tmpValueData = HeapAlloc( GetProcessHeap(), 0, ( len + count ) * sizeof(WCHAR));
        if ( !tmpValueData ) goto done;

        for ( i = 0, j = 0; i < len - 1; i++)
        {
            if ( !stringValueData[i] && stringValueData[i + 1])
            {
                tmpValueData[j++] = char1;
                tmpValueData[j++] = char2;
            }
            else
                tmpValueData[j++] = stringValueData[i];
        }
        tmpValueData[j] = stringValueData[i];
        HeapFree( GetProcessHeap(), 0, stringValueData);
        stringValueData = tmpValueData;
        tmpValueData = NULL;

        if (DialogBoxW(0, MAKEINTRESOURCEW(IDD_EDIT_MULTI_STRING), hwnd, modify_dlgproc) == IDOK)
        {
            len = lstrlenW( stringValueData );
            tmpValueData = HeapAlloc( GetProcessHeap(), 0, (len + 2) * sizeof(WCHAR));
            if ( !tmpValueData ) goto done;

            for ( i = 0, j = 0; i < len - 1; i++)
            {
                if ( stringValueData[i] == char1 && stringValueData[i + 1] == char2)
                {
                    if ( tmpValueData[j - 1] != 0)
                        tmpValueData[j++] = 0;
                    i++;
                }
                else
                    tmpValueData[j++] = stringValueData[i];
            }
            tmpValueData[j++] = stringValueData[i];
            tmpValueData[j++] = 0;
            tmpValueData[j++] = 0;
            HeapFree( GetProcessHeap(), 0, stringValueData);
            stringValueData = tmpValueData;

            lRet = RegSetValueExW(hKey, valueName, 0, type, (LPBYTE)stringValueData, j * sizeof(WCHAR));
            if (lRet == ERROR_SUCCESS) result = TRUE;
            else error_code_messagebox(hwnd, lRet);
        }
    } else {
        error(hwnd, IDS_UNSUPPORTED_TYPE, type);
    }

done:
    HeapFree(GetProcessHeap(), 0, stringValueData);
    stringValueData = NULL;
    RegCloseKey(hKey);
    return result;
}

BOOL DeleteKey(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath)
{
    BOOL result = FALSE;
    LONG lRet;
    HKEY hKey;

    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_READ|KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	return FALSE;
    }
    
    if (messagebox(hwnd, MB_YESNO | MB_ICONEXCLAMATION, IDS_DELETE_BOX_TITLE, IDS_DELETE_BOX_TEXT, keyPath) != IDYES)
	goto done;
	
    lRet = SHDeleteKeyW(hKeyRoot, keyPath);
    if (lRet != ERROR_SUCCESS) {
	error(hwnd, IDS_BAD_KEY, keyPath);
	goto done;
    }
    result = TRUE;
    
done:
    RegCloseKey(hKey);
    return result;
}

BOOL DeleteValue(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath, LPCWSTR valueName, BOOL showMessageBox)
{
    BOOL result = FALSE;
    LONG lRet;
    HKEY hKey;
    LPCWSTR visibleValueName = valueName ? valueName : g_pszDefaultValueName;
    WCHAR empty = 0;

    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_READ | KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) return FALSE;

    if (showMessageBox)
    {
        if (messagebox(hwnd, MB_YESNO | MB_ICONEXCLAMATION, IDS_DELETE_BOX_TITLE, IDS_DELETE_BOX_TEXT, visibleValueName) != IDYES)
            goto done;
    }

    lRet = RegDeleteValueW(hKey, valueName ? valueName : &empty);
    if (lRet != ERROR_SUCCESS && valueName) {
        error(hwnd, IDS_BAD_VALUE, valueName);
    }
    if (lRet != ERROR_SUCCESS) goto done;
    result = TRUE;

done:
    RegCloseKey(hKey);
    return result;
}

BOOL CreateValue(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath, DWORD valueType, LPWSTR valueName)
{
    LONG lRet = ERROR_SUCCESS;
    WCHAR newValue[256];
    DWORD valueDword = 0;
    BOOL result = FALSE;
    int valueNum;
    HKEY hKey;
         
    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_READ | KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	return FALSE;
    }

    if (!LoadStringW(GetModuleHandleW(0), IDS_NEWVALUE, newValue, COUNT_OF(newValue))) goto done;

    /* try to find out a name for the newly create key (max 100 times) */
    for (valueNum = 1; valueNum < 100; valueNum++) {
	wsprintfW(valueName, newValue, valueNum);
	lRet = RegQueryValueExW(hKey, valueName, 0, 0, 0, 0);
	if (lRet == ERROR_FILE_NOT_FOUND) break;
    }
    if (lRet != ERROR_FILE_NOT_FOUND) {
	error_code_messagebox(hwnd, lRet);
	goto done;
    }
   
    lRet = RegSetValueExW(hKey, valueName, 0, valueType, (BYTE*)&valueDword, sizeof(DWORD));
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	goto done;
    }
    result = TRUE;

done:
    RegCloseKey(hKey);
    return result;
}

BOOL RenameValue(HWND hwnd, HKEY hKeyRoot, LPCWSTR keyPath, LPCWSTR oldName, LPCWSTR newName)
{
    LPWSTR value = NULL;
    DWORD type;
    LONG len, lRet;
    BOOL result = FALSE;
    HKEY hKey;

    if (!oldName) return FALSE;
    if (!newName) return FALSE;

    lRet = RegOpenKeyExW(hKeyRoot, keyPath, 0, KEY_READ | KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	return FALSE;
    }
    /* check if value already exists */
    if (check_value(hwnd, hKey, newName)) goto done;
    value = read_value(hwnd, hKey, oldName, &type, &len);
    if(!value) goto done;
    lRet = RegSetValueExW(hKey, newName, 0, type, (BYTE*)value, len);
    if (lRet != ERROR_SUCCESS) {
	error_code_messagebox(hwnd, lRet);
	goto done;
    }
    lRet = RegDeleteValueW(hKey, oldName);
    if (lRet != ERROR_SUCCESS) {
	RegDeleteValueW(hKey, newName);
	error_code_messagebox(hwnd, lRet);
	goto done;
    }
    result = TRUE;

done:
    HeapFree(GetProcessHeap(), 0, value);
    RegCloseKey(hKey);
    return result;
}


BOOL RenameKey(HWND hwnd, HKEY hRootKey, LPCWSTR keyPath, LPCWSTR newName)
{
    LPWSTR parentPath = 0;
    LPCWSTR srcSubKey = 0;
    HKEY parentKey = 0;
    HKEY destKey = 0;
    BOOL result = FALSE;
    LONG lRet;
    DWORD disposition;

    if (!keyPath || !newName) return FALSE;

    if (!strrchrW(keyPath, '\\')) {
	parentKey = hRootKey;
	srcSubKey = keyPath;
    } else {
	LPWSTR srcSubKey_copy;

	parentPath = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(keyPath)+1)*sizeof(WCHAR));
	lstrcpyW(parentPath, keyPath);
	srcSubKey_copy = strrchrW(parentPath, '\\');
	*srcSubKey_copy = 0;
	srcSubKey = srcSubKey_copy + 1;
	lRet = RegOpenKeyExW(hRootKey, parentPath, 0, KEY_READ | KEY_CREATE_SUB_KEY, &parentKey);
	if (lRet != ERROR_SUCCESS) {
	    error_code_messagebox(hwnd, lRet);
	    goto done;
	}
    }

    /* The following fails if the old name is the same as the new name. */
    if (!lstrcmpW(srcSubKey, newName)) goto done;

    lRet = RegCreateKeyExW(parentKey, newName, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL /* FIXME */, &destKey, &disposition);
    if (disposition == REG_OPENED_EXISTING_KEY)
        lRet = ERROR_FILE_EXISTS; /* FIXME: we might want a better error message than this */
    if (lRet != ERROR_SUCCESS) {
        error_code_messagebox(hwnd, lRet);
        goto done;
    }

    /* FIXME: SHCopyKey does not copy the security attributes */
    lRet = SHCopyKeyW(parentKey, srcSubKey, destKey, 0);
    if (lRet != ERROR_SUCCESS) {
        RegCloseKey(destKey);
        RegDeleteKeyW(parentKey, newName);
        error_code_messagebox(hwnd, lRet);
        goto done;
    }

    lRet = SHDeleteKeyW(hRootKey, keyPath);
    if (lRet != ERROR_SUCCESS) {
        error_code_messagebox(hwnd, lRet);
        goto done;
    }

    result = TRUE;

done:
    RegCloseKey(destKey);
    if (parentKey) {
        RegCloseKey(parentKey); 
        HeapFree(GetProcessHeap(), 0, parentPath);
    }
    return result;
}
