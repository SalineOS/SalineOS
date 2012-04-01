/*
 * Copyright (c) 2011 Lucas Fialho Zawacki
 * Copyright (c) 2006 Vitaliy Margolen
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

#define DIRECTINPUT_VERSION 0x0800

#define COBJMACROS
#include <windows.h>

#include "wine/test.h"
#include "windef.h"
#include "dinput.h"

struct enum_data {
    LPDIRECTINPUT8 pDI;
    LPDIACTIONFORMAT lpdiaf;
    LPDIRECTINPUTDEVICE8 keyboard;
    LPDIRECTINPUTDEVICE8 mouse;
    const char* username;
    int ndevices;
};

/* Dummy GUID */
static const GUID ACTION_MAPPING_GUID = { 0x1, 0x2, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };

enum {
    DITEST_AXIS,
    DITEST_BUTTON,
    DITEST_KEYBOARDSPACE,
    DITEST_MOUSEBUTTON0,
    DITEST_YAXIS
};

static DIACTION actionMapping[]=
{
  /* axis */
  { 0, 0x01008A01 /* DIAXIS_DRIVINGR_STEER */ , 0, { "Steer" } },
  /* button */
  { 1, 0x01000C01 /* DIBUTTON_DRIVINGR_SHIFTUP */ , 0, { "Upshift" } },
  /* keyboard key */
  { 2, DIKEYBOARD_SPACE , 0, { "Missile" } },
  /* mouse button */
  { 3, DIMOUSE_BUTTON0, 0, { "Select" } },
  /* mouse axis */
  { 4, DIMOUSE_YAXIS, 0, { "Y Axis" } }
};

static void test_device_input(
    LPDIRECTINPUTDEVICE8 lpdid,
    DWORD event_type,
    DWORD event,
    DWORD expected
)
{
    HRESULT hr;
    DIDEVICEOBJECTDATA obj_data;
    DWORD data_size = 1;
    int i;

    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Failed to acquire device hr=%08x\n", hr);

    if (event_type == INPUT_KEYBOARD)
        keybd_event( event, DIK_SPACE, 0, 0);

    if (event_type == INPUT_MOUSE)
        mouse_event( event, 0, 0, 0, 0);

    IDirectInputDevice8_Poll(lpdid);
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);

    if (data_size != 1)
    {
        win_skip("We're not able to inject input into Windows dinput8 with events\n");
        return;
    }

    ok (obj_data.uAppData == expected, "Retrieval of action failed uAppData=%lu expected=%d\n", obj_data.uAppData, expected);

    /* Check for buffer owerflow */
    for (i = 0; i < 17; i++)
        if (event_type == INPUT_KEYBOARD)
        {
            keybd_event( VK_SPACE, DIK_SPACE, 0, 0);
            keybd_event( VK_SPACE, DIK_SPACE, KEYEVENTF_KEYUP, 0);
        }
        else if (event_type == INPUT_MOUSE)
        {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 1, 1, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 1, 1, 0, 0);
        }

    IDirectInputDevice8_Poll(lpdid);

    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_BUFFEROVERFLOW, "GetDeviceData() failed: %08x\n", hr);
    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_OK && data_size == 1, "GetDeviceData() failed: %08x cnt:%d\n", hr, data_size);
}

static void test_build_action_map(
    LPDIRECTINPUTDEVICE8 lpdid,
    LPDIACTIONFORMAT lpdiaf,
    int action_index,
    DWORD expected_type,
    DWORD expected_inst
)
{
    HRESULT hr;
    DIACTION *actions;
    DWORD instance, type, how;
    GUID assigned_to;
    DIDEVICEINSTANCEA ddi;

    ddi.dwSize = sizeof(ddi);
    IDirectInputDevice_GetDeviceInfo(lpdid, &ddi);

    hr = IDirectInputDevice8_BuildActionMap(lpdid, lpdiaf, NULL, DIDBAM_INITIALIZE);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    actions = lpdiaf->rgoAction;
    instance = DIDFT_GETINSTANCE(actions[action_index].dwObjID);
    type = DIDFT_GETTYPE(actions[action_index].dwObjID);
    how = actions[action_index].dwHow;
    assigned_to = actions[action_index].guidInstance;

    ok (how == DIAH_USERCONFIG || how == DIAH_DEFAULT, "Action was not set dwHow=%08x\n", how);
    ok (instance == expected_inst, "Action not mapped correctly instance=%08x expected=%08x\n", instance, expected_inst);
    ok (type == expected_type, "Action type not mapped correctly type=%08x expected=%08x\n", type, expected_type);
    ok (IsEqualGUID(&assigned_to, &ddi.guidInstance), "Action and device GUID do not match action=%d\n", action_index);
}

static BOOL CALLBACK enumeration_callback(
    LPCDIDEVICEINSTANCE lpddi,
    LPDIRECTINPUTDEVICE8 lpdid,
    DWORD dwFlags,
    DWORD dwRemaining,
    LPVOID pvRef)
{
    HRESULT hr;
    DIPROPDWORD dp;
    DIPROPRANGE dpr;
    DIPROPSTRING dps;
    WCHAR usernameW[MAX_PATH];
    DWORD username_size = MAX_PATH;
    struct enum_data *data = pvRef;
    DWORD cnt;
    DIDEVICEOBJECTDATA buffer[5];

    if (!data) return DIENUM_CONTINUE;

    data->ndevices++;

    /* Convert username to WCHAR */
    if (data->username != NULL)
    {
        username_size = MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, 0);
        MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, username_size);
    }
    else
        GetUserNameW(usernameW, &username_size);

    /* collect the mouse and keyboard */
    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysKeyboard))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->keyboard = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Keyboard should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysMouse))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->mouse = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Mouse should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    /* Building and setting an action map */
    /* It should not use any pre-stored mappings so we use DIDBAM_INITIALIZE */
    hr = IDirectInputDevice8_BuildActionMap(lpdid, data->lpdiaf, NULL, DIDBAM_INITIALIZE);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    /* Device has no data format and thus can't be acquired */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (hr == DIERR_INVALIDPARAM, "Device was acquired before SetActionMap hr=%08x\n", hr);

    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, data->username, 0);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    /* Some joysticks may have no suitable actions and thus should not be tested */
    if (hr == DI_NOEFFECT) return DIENUM_CONTINUE;

    /* Test username after SetActionMap */
    dps.diph.dwSize = sizeof(dps);
    dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dps.diph.dwObj = 0;
    dps.diph.dwHow  = DIPH_DEVICE;
    dps.wsz[0] = '\0';

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_USERNAME, &dps.diph);
    todo_wine ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    todo_wine ok (!lstrcmpW(usernameW, dps.wsz), "Username not set correctly expected=%s, got=%s\n", wine_dbgstr_wn(usernameW, -1), wine_dbgstr_wn(dps.wsz, -1));

    /* Test buffer size */
    memset(&dp, 0, sizeof(dp));
    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_BUFFERSIZE, &dp.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (dp.dwData == data->lpdiaf->dwBufferSize, "SetActionMap must set the buffer, buffersize=%d\n", dp.dwData);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DIERR_NOTACQUIRED, "GetDeviceData() failed hr=%08x\n", hr);

    /* Test axis range */
    memset(&dpr, 0, sizeof(dpr));
    dpr.diph.dwSize = sizeof(dpr);
    dpr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dpr.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_RANGE, &dpr.diph);
    /* Only test if device supports the range property */
    if (SUCCEEDED(hr))
    {
        ok (dpr.lMin == data->lpdiaf->lAxisMin, "SetActionMap must set the min axis range expected=%d got=%d\n", data->lpdiaf->lAxisMin, dpr.lMin);
        ok (dpr.lMax == data->lpdiaf->lAxisMax, "SetActionMap must set the max axis range expected=%d got=%d\n", data->lpdiaf->lAxisMax, dpr.lMax);
    }

    /* SetActionMap has set the data format so now it should work */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Acquire failed hr=%08x\n", hr);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DI_OK, "GetDeviceData() failed hr=%08x\n", hr);

    /* SetActionMap should not work on an acquired device */
    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, NULL, 0);
    ok (hr == DIERR_ACQUIRED, "SetActionMap succeeded with an acquired device hr=%08x\n", hr);

    return DIENUM_CONTINUE;
}

static void test_action_mapping(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandle(NULL);
    LPDIRECTINPUT8 pDI = NULL;
    DIACTIONFORMAT af;
    struct enum_data data =  {pDI, &af, NULL, NULL, NULL, 0};

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, 1, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    memset (&af, 0, sizeof(af));
    af.dwSize = sizeof(af);
    af.dwActionSize = sizeof(DIACTION);
    af.dwDataSize = 4 * sizeof(actionMapping) / sizeof(actionMapping[0]);
    af.dwNumActions = sizeof(actionMapping) / sizeof(actionMapping[0]);
    af.rgoAction = actionMapping;
    af.guidActionMap = ACTION_MAPPING_GUID;
    af.dwGenre = 0x01000000; /* DIVIRTUAL_DRIVING_RACE */
    af.dwBufferSize = 32;

    /* This enumeration builds and sets the action map for all devices */
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, 0, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    /* Repeat tests with a non NULL user */
    data.username = "Ninja Brian";
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, NULL, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    if (data.keyboard != NULL)
    {
        /* Test keyboard BuildActionMap */
        test_build_action_map(data.keyboard, data.lpdiaf, DITEST_KEYBOARDSPACE, DIDFT_PSHBUTTON, DIK_SPACE);
        /* Test keyboard input */
        test_device_input(data.keyboard, INPUT_KEYBOARD, VK_SPACE, 2);

        /* Test BuildActionMap with no suitable actions for a device */
        IDirectInputDevice_Unacquire(data.keyboard);
        af.dwDataSize = 4 * DITEST_KEYBOARDSPACE;
        af.dwNumActions = DITEST_KEYBOARDSPACE;

        hr = IDirectInputDevice8_BuildActionMap(data.keyboard, data.lpdiaf, NULL, DIDBAM_INITIALIZE);
        ok (hr == DI_NOEFFECT, "BuildActionMap should have no effect with no actions hr=%08x\n", hr);

        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, 0);
        ok (hr == DI_NOEFFECT, "SetActionMap should have no effect with no actions to map hr=%08x\n", hr);

        af.dwDataSize = 4 * sizeof(actionMapping) / sizeof(actionMapping[0]);
        af.dwNumActions = sizeof(actionMapping) / sizeof(actionMapping[0]);
    }

    if (data.mouse != NULL)
    {
        /* Test mouse BuildActionMap */
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_MOUSEBUTTON0, DIDFT_PSHBUTTON, 0x03);
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_YAXIS, DIDFT_RELAXIS, 0x01);

        test_device_input(data.mouse, INPUT_MOUSE, MOUSEEVENTF_LEFTDOWN, 3);
    }
}

START_TEST(device)
{
    CoInitialize(NULL);

    test_action_mapping();

    CoUninitialize();
}
