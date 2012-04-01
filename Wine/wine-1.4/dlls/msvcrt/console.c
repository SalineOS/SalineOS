/*
 * msvcrt.dll console functions
 *
 * Copyright 2000 Jon Griffiths
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
 * Note: init and free don't need MT locking since they are called at DLL
 * (de)attachment time, which is synchronised for us
 */

#include "msvcrt.h"
#include "winnls.h"
#include "wincon.h"
#include "mtdll.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);



/* MT */
#define LOCK_CONSOLE   _mlock(_CONIO_LOCK)
#define UNLOCK_CONSOLE _munlock(_CONIO_LOCK)

static HANDLE MSVCRT_console_in = INVALID_HANDLE_VALUE;
static HANDLE MSVCRT_console_out= INVALID_HANDLE_VALUE;
static int __MSVCRT_console_buffer = MSVCRT_EOF;

/* INTERNAL: Initialise console handles */
void msvcrt_init_console(void)
{
  TRACE(":Opening console handles\n");

  MSVCRT_console_in = CreateFileA("CONIN$", GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, 0, NULL);
  MSVCRT_console_out= CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
				    NULL, OPEN_EXISTING, 0, NULL);

  if ((MSVCRT_console_in == INVALID_HANDLE_VALUE) ||
      (MSVCRT_console_out== INVALID_HANDLE_VALUE))
    WARN(":Console handle Initialisation FAILED!\n");
}

/* INTERNAL: Free console handles */
void msvcrt_free_console(void)
{
  TRACE(":Closing console handles\n");
  CloseHandle(MSVCRT_console_in);
  CloseHandle(MSVCRT_console_out);
}

/*********************************************************************
 *		_cputs (MSVCRT.@)
 */
int CDECL _cputs(const char* str)
{
  DWORD count;
  int retval = MSVCRT_EOF;

  LOCK_CONSOLE;
  if (WriteConsoleA(MSVCRT_console_out, str, strlen(str), &count, NULL)
      && count == 1)
    retval = 0;
  UNLOCK_CONSOLE;
  return retval;
}

/*********************************************************************
 *		_cputws (MSVCRT.@)
 */
int CDECL _cputws(const MSVCRT_wchar_t* str)
{
  DWORD count;
  int retval = MSVCRT_EOF;

  LOCK_CONSOLE;
  if (WriteConsoleW(MSVCRT_console_out, str, lstrlenW(str), &count, NULL)
      && count == 1)
    retval = 0;
  UNLOCK_CONSOLE;
  return retval;
}

#define NORMAL_CHAR     0
#define ALT_CHAR        1
#define CTRL_CHAR       2
#define SHIFT_CHAR      3

static const struct {unsigned vk; unsigned ch[4][2];} enh_map[] = {
    {0x47, {{0xE0, 0x47}, {0x00, 0x97}, {0xE0, 0x77}, {0xE0, 0x47}}},
    {0x48, {{0xE0, 0x48}, {0x00, 0x98}, {0xE0, 0x8D}, {0xE0, 0x48}}},
    {0x49, {{0xE0, 0x49}, {0x00, 0x99}, {0xE0, 0x86}, {0xE0, 0x49}}},
    {0x4B, {{0xE0, 0x4B}, {0x00, 0x9B}, {0xE0, 0x73}, {0xE0, 0x4B}}},
    {0x4D, {{0xE0, 0x4D}, {0x00, 0x9D}, {0xE0, 0x74}, {0xE0, 0x4D}}},
    {0x4F, {{0xE0, 0x4F}, {0x00, 0x9F}, {0xE0, 0x75}, {0xE0, 0x4F}}},
    {0x50, {{0xE0, 0x50}, {0x00, 0xA0}, {0xE0, 0x91}, {0xE0, 0x50}}},
    {0x51, {{0xE0, 0x51}, {0x00, 0xA1}, {0xE0, 0x76}, {0xE0, 0x51}}},
    {0x52, {{0xE0, 0x52}, {0x00, 0xA2}, {0xE0, 0x92}, {0xE0, 0x52}}},
    {0x53, {{0xE0, 0x53}, {0x00, 0xA3}, {0xE0, 0x93}, {0xE0, 0x53}}},
};

/*********************************************************************
 *		_getch (MSVCRT.@)
 */
int CDECL _getch(void)
{
  int retval = MSVCRT_EOF;

  LOCK_CONSOLE;
  if (__MSVCRT_console_buffer != MSVCRT_EOF)
  {
    retval = __MSVCRT_console_buffer;
    __MSVCRT_console_buffer = MSVCRT_EOF;
  }
  else
  {
    INPUT_RECORD ir;
    DWORD count;
    DWORD mode = 0;

    GetConsoleMode(MSVCRT_console_in, &mode);
    if(mode)
      SetConsoleMode(MSVCRT_console_in, 0);

    do {
      if (ReadConsoleInputA(MSVCRT_console_in, &ir, 1, &count))
      {
          unsigned int i;
        /* Only interested in ASCII chars */
        if (ir.EventType == KEY_EVENT &&
            ir.Event.KeyEvent.bKeyDown)
        {
            if (ir.Event.KeyEvent.uChar.AsciiChar)
            {
                retval = ir.Event.KeyEvent.uChar.AsciiChar;
                break;
            }
            for (i = 0; i < sizeof(enh_map) / sizeof(enh_map[0]); i++)
            {
                if (ir.Event.KeyEvent.wVirtualScanCode == enh_map[i].vk) break;
            }
            if (i < sizeof(enh_map) / sizeof(enh_map[0]))
            {
                unsigned idx;

                if (ir.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
                    idx = ALT_CHAR;
                else if (ir.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED) )
                    idx = CTRL_CHAR;
                else if (ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)
                    idx = SHIFT_CHAR;
                else
                    idx = NORMAL_CHAR;

                retval = enh_map[i].ch[idx][0];
                __MSVCRT_console_buffer = enh_map[i].ch[idx][1];
                break;
            }
            WARN("Unmapped char keyState=%x vk=%x\n",
                 ir.Event.KeyEvent.dwControlKeyState, ir.Event.KeyEvent.wVirtualScanCode);
        }
      }
      else
        break;
    } while(1);
    if (mode)
      SetConsoleMode(MSVCRT_console_in, mode);
  }
  UNLOCK_CONSOLE;
  return retval;
}

/*********************************************************************
 *		_putch (MSVCRT.@)
 */
int CDECL _putch(int c)
{
  int retval = MSVCRT_EOF;
  DWORD count;
  LOCK_CONSOLE;
  if (WriteConsoleA(MSVCRT_console_out, &c, 1, &count, NULL) && count == 1)
    retval = c;
  UNLOCK_CONSOLE;
  return retval;
}

/*********************************************************************
 *		_getche (MSVCRT.@)
 */
int CDECL _getche(void)
{
  int retval;
  LOCK_CONSOLE;
  retval = _getch();
  if (retval != MSVCRT_EOF)
    retval = _putch(retval);
  UNLOCK_CONSOLE;
  return retval;
}

/*********************************************************************
 *		_cgets (MSVCRT.@)
 */
char* CDECL _cgets(char* str)
{
  char *buf = str + 2;
  DWORD got;
  DWORD conmode = 0;

  TRACE("(%p)\n", str);
  str[1] = 0; /* Length */
  LOCK_CONSOLE;
  GetConsoleMode(MSVCRT_console_in, &conmode);
  SetConsoleMode(MSVCRT_console_in, ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);

  if(ReadConsoleA(MSVCRT_console_in, buf, str[0], &got, NULL)) {
    if(buf[got-2] == '\r') {
      buf[got-2] = 0;
      str[1] = got-2;
    }
    else if(got == 1 && buf[got-1] == '\n') {
      buf[0] = 0;
      str[1] = 0;
    }
    else if(got == str[0] && buf[got-1] == '\r') {
      buf[got-1] = 0;
      str[1] = got-1;
    }
    else
      str[1] = got;
  }
  else
    buf = NULL;
  SetConsoleMode(MSVCRT_console_in, conmode);
  UNLOCK_CONSOLE;
  return buf;
}

/*********************************************************************
 *		_ungetch (MSVCRT.@)
 */
int CDECL _ungetch(int c)
{
  int retval = MSVCRT_EOF;
  LOCK_CONSOLE;
  if (c != MSVCRT_EOF && __MSVCRT_console_buffer == MSVCRT_EOF)
    retval = __MSVCRT_console_buffer = c;
  UNLOCK_CONSOLE;
  return retval;
}

/*********************************************************************
 *		_kbhit (MSVCRT.@)
 */
int CDECL _kbhit(void)
{
  int retval = 0;

  LOCK_CONSOLE;
  if (__MSVCRT_console_buffer != MSVCRT_EOF)
    retval = 1;
  else
  {
    /* FIXME: There has to be a faster way than this in Win32.. */
    INPUT_RECORD *ir = NULL;
    DWORD count = 0, i;

    GetNumberOfConsoleInputEvents(MSVCRT_console_in, &count);

    if (count && (ir = MSVCRT_malloc(count * sizeof(INPUT_RECORD))) &&
        PeekConsoleInputA(MSVCRT_console_in, ir, count, &count))
      for(i = 0; i < count - 1; i++)
      {
        if (ir[i].EventType == KEY_EVENT &&
            ir[i].Event.KeyEvent.bKeyDown &&
            ir[i].Event.KeyEvent.uChar.AsciiChar)
        {
          retval = 1;
          break;
        }
      }
    MSVCRT_free(ir);
  }
  UNLOCK_CONSOLE;
  return retval;
}

static int puts_clbk_console_a(void *ctx, int len, const char *str)
{
    LOCK_CONSOLE;
    if(!WriteConsoleA(MSVCRT_console_out, str, len, NULL, NULL))
        len = -1;
    UNLOCK_CONSOLE;
    return len;
}

static int puts_clbk_console_w(void *ctx, int len, const MSVCRT_wchar_t *str)
{
    LOCK_CONSOLE;
    if(!WriteConsoleW(MSVCRT_console_out, str, len, NULL, NULL))
        len = -1;
    UNLOCK_CONSOLE;
    return len;
}

/*********************************************************************
 *		_vcprintf (MSVCRT.@)
 */
int CDECL _vcprintf(const char* format, __ms_va_list valist)
{
    return pf_printf_a(puts_clbk_console_a, NULL, format, NULL, FALSE, FALSE, arg_clbk_valist, NULL, &valist);
}

/*********************************************************************
 *		_cprintf (MSVCRT.@)
 */
int CDECL _cprintf(const char* format, ...)
{
  int retval;
  __ms_va_list valist;

  __ms_va_start( valist, format );
  retval = _vcprintf(format, valist);
  __ms_va_end(valist);

  return retval;
}


/*********************************************************************
 *		_vcwprintf (MSVCRT.@)
 */
int CDECL _vcwprintf(const MSVCRT_wchar_t* format, __ms_va_list valist)
{
    return pf_printf_w(puts_clbk_console_w, NULL, format, NULL, FALSE, FALSE, arg_clbk_valist, NULL, &valist);
}

/*********************************************************************
 *		_cwprintf (MSVCRT.@)
 */
int CDECL _cwprintf(const MSVCRT_wchar_t* format, ...)
{
  int retval;
  __ms_va_list valist;

  __ms_va_start( valist, format );
  retval = _vcwprintf(format, valist);
  __ms_va_end(valist);

  return retval;
}
