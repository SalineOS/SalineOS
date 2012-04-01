/*
 * Window related functions
 *
 * Copyright 1993, 1994, 1995, 1996, 2001 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
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

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_LIBXSHAPE
#include <X11/extensions/shape.h>
#endif /* HAVE_LIBXSHAPE */

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/unicode.h"

#include "x11drv.h"
#include "xcomposite.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "mwm.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

#define _NET_WM_STATE_REMOVE  0
#define _NET_WM_STATE_ADD     1
#define _NET_WM_STATE_TOGGLE  2

#define SWP_AGG_NOPOSCHANGE (SWP_NOSIZE | SWP_NOMOVE | SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE | SWP_NOZORDER)

/* is cursor clipping active? */
int clipping_cursor = 0;

/* X context to associate a hwnd to an X window */
XContext winContext = 0;

/* X context to associate a struct x11drv_win_data to an hwnd */
static XContext win_data_context;

/* time of last user event and window where it's stored */
static Time last_user_time;
static Window user_time_window;

static const char foreign_window_prop[] = "__wine_x11_foreign_window";
static const char whole_window_prop[] = "__wine_x11_whole_window";
static const char client_window_prop[]= "__wine_x11_client_window";
static const char icon_window_prop[]  = "__wine_x11_icon_window";
static const char clip_window_prop[]  = "__wine_x11_clip_window";
static const char fbconfig_id_prop[]  = "__wine_x11_fbconfig_id";
static const char gl_drawable_prop[]  = "__wine_x11_gl_drawable";
static const char pixmap_prop[]       = "__wine_x11_pixmap";
static const char managed_prop[]      = "__wine_x11_managed";


/***********************************************************************
 * http://standards.freedesktop.org/startup-notification-spec
 */
static void remove_startup_notification(Display *display, Window window)
{
    static LONG startup_notification_removed = 0;
    char id[1024];
    char message[1024];
    int i;
    int pos;
    XEvent xevent;
    const char *src;
    int srclen;

    if (InterlockedCompareExchange(&startup_notification_removed, 1, 0) != 0)
        return;

    if (GetEnvironmentVariableA("DESKTOP_STARTUP_ID", id, sizeof(id)) == 0)
        return;
    SetEnvironmentVariableA("DESKTOP_STARTUP_ID", NULL);

    if ((src = strstr( id, "_TIME" ))) update_user_time( atol( src + 5 ));

    pos = snprintf(message, sizeof(message), "remove: ID=");
    message[pos++] = '"';
    for (i = 0; id[i] && pos < sizeof(message) - 2; i++)
    {
        if (id[i] == '"' || id[i] == '\\')
            message[pos++] = '\\';
        message[pos++] = id[i];
    }
    message[pos++] = '"';
    message[pos++] = '\0';

    xevent.xclient.type = ClientMessage;
    xevent.xclient.message_type = x11drv_atom(_NET_STARTUP_INFO_BEGIN);
    xevent.xclient.display = display;
    xevent.xclient.window = window;
    xevent.xclient.format = 8;

    src = message;
    srclen = strlen(src) + 1;

    wine_tsx11_lock();
    while (srclen > 0)
    {
        int msglen = srclen;
        if (msglen > 20)
            msglen = 20;
        memset(&xevent.xclient.data.b[0], 0, 20);
        memcpy(&xevent.xclient.data.b[0], src, msglen);
        src += msglen;
        srclen -= msglen;

        XSendEvent( display, DefaultRootWindow( display ), False, PropertyChangeMask, &xevent );
        xevent.xclient.message_type = x11drv_atom(_NET_STARTUP_INFO);
    }
    wine_tsx11_unlock();
}


struct has_popup_result
{
    HWND hwnd;
    BOOL found;
};

static BOOL CALLBACK has_managed_popup( HWND hwnd, LPARAM lparam )
{
    struct has_popup_result *result = (struct has_popup_result *)lparam;
    struct x11drv_win_data *data;

    if (hwnd == result->hwnd) return FALSE;  /* popups are always above owner */
    if (!(data = X11DRV_get_win_data( hwnd ))) return TRUE;
    if (GetWindow( hwnd, GW_OWNER ) != result->hwnd) return TRUE;
    result->found = data->managed;
    return !result->found;
}

static BOOL has_owned_popups( HWND hwnd )
{
    struct has_popup_result result;

    result.hwnd = hwnd;
    result.found = FALSE;
    EnumWindows( has_managed_popup, (LPARAM)&result );
    return result.found;
}

/***********************************************************************
 *		is_window_managed
 *
 * Check if a given window should be managed
 */
static BOOL is_window_managed( HWND hwnd, UINT swp_flags, const RECT *window_rect )
{
    DWORD style, ex_style;

    if (!managed_mode) return FALSE;

    /* child windows are not managed */
    style = GetWindowLongW( hwnd, GWL_STYLE );
    if ((style & (WS_CHILD|WS_POPUP)) == WS_CHILD) return FALSE;
    /* activated windows are managed */
    if (!(swp_flags & (SWP_NOACTIVATE|SWP_HIDEWINDOW))) return TRUE;
    if (hwnd == GetActiveWindow()) return TRUE;
    /* windows with caption are managed */
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;
    /* windows with thick frame are managed */
    if (style & WS_THICKFRAME) return TRUE;
    if (style & WS_POPUP)
    {
        HMONITOR hmon;
        MONITORINFO mi;

        /* popup with sysmenu == caption are managed */
        if (style & WS_SYSMENU) return TRUE;
        /* full-screen popup windows are managed */
        hmon = MonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY );
        mi.cbSize = sizeof( mi );
        GetMonitorInfoW( hmon, &mi );
        if (window_rect->left <= mi.rcWork.left && window_rect->right >= mi.rcWork.right &&
            window_rect->top <= mi.rcWork.top && window_rect->bottom >= mi.rcWork.bottom)
            return TRUE;
    }
    /* application windows are managed */
    ex_style = GetWindowLongW( hwnd, GWL_EXSTYLE );
    if (ex_style & WS_EX_APPWINDOW) return TRUE;
    /* windows that own popups are managed */
    if (has_owned_popups( hwnd )) return TRUE;
    /* default: not managed */
    return FALSE;
}


/***********************************************************************
 *		is_window_resizable
 *
 * Check if window should be made resizable by the window manager
 */
static inline BOOL is_window_resizable( struct x11drv_win_data *data, DWORD style )
{
    if (style & WS_THICKFRAME) return TRUE;
    /* Metacity needs the window to be resizable to make it fullscreen */
    return is_window_rect_fullscreen( &data->whole_rect );
}


/***********************************************************************
 *              get_mwm_decorations
 */
static unsigned long get_mwm_decorations( struct x11drv_win_data *data,
                                          DWORD style, DWORD ex_style )
{
    unsigned long ret = 0;

    if (!decorated_mode) return 0;

    if (IsRectEmpty( &data->window_rect )) return 0;
    if (data->shaped) return 0;

    if (ex_style & WS_EX_TOOLWINDOW) return 0;

    if ((style & WS_CAPTION) == WS_CAPTION)
    {
        ret |= MWM_DECOR_TITLE | MWM_DECOR_BORDER;
        if (style & WS_SYSMENU) ret |= MWM_DECOR_MENU;
        if (style & WS_MINIMIZEBOX) ret |= MWM_DECOR_MINIMIZE;
        if (style & WS_MAXIMIZEBOX) ret |= MWM_DECOR_MAXIMIZE;
    }
    if (ex_style & WS_EX_DLGMODALFRAME) ret |= MWM_DECOR_BORDER;
    else if (style & WS_THICKFRAME) ret |= MWM_DECOR_BORDER | MWM_DECOR_RESIZEH;
    else if ((style & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME) ret |= MWM_DECOR_BORDER;
    return ret;
}


/***********************************************************************
 *		get_x11_rect_offset
 *
 * Helper for X11DRV_window_to_X_rect and X11DRV_X_to_window_rect.
 */
static void get_x11_rect_offset( struct x11drv_win_data *data, RECT *rect )
{
    DWORD style, ex_style, style_mask = 0, ex_style_mask = 0;
    unsigned long decor;

    rect->top = rect->bottom = rect->left = rect->right = 0;

    style = GetWindowLongW( data->hwnd, GWL_STYLE );
    ex_style = GetWindowLongW( data->hwnd, GWL_EXSTYLE );
    decor = get_mwm_decorations( data, style, ex_style );

    if (decor & MWM_DECOR_TITLE) style_mask |= WS_CAPTION;
    if (decor & MWM_DECOR_BORDER)
    {
        style_mask |= WS_DLGFRAME | WS_THICKFRAME;
        ex_style_mask |= WS_EX_DLGMODALFRAME;
    }

    AdjustWindowRectEx( rect, style & style_mask, FALSE, ex_style & ex_style_mask );
}


/***********************************************************************
 *              get_window_attributes
 *
 * Fill the window attributes structure for an X window.
 */
static int get_window_attributes( Display *display, struct x11drv_win_data *data,
                                  XSetWindowAttributes *attr )
{
    attr->override_redirect = !data->managed;
    attr->colormap          = X11DRV_PALETTE_PaletteXColormap;
    attr->save_under        = ((GetClassLongW( data->hwnd, GCL_STYLE ) & CS_SAVEBITS) != 0);
    attr->bit_gravity       = NorthWestGravity;
    attr->win_gravity       = StaticGravity;
    attr->backing_store     = NotUseful;
    attr->event_mask        = (ExposureMask | PointerMotionMask |
                               ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                               KeyPressMask | KeyReleaseMask | FocusChangeMask |
                               KeymapStateMask | StructureNotifyMask);
    if (data->managed) attr->event_mask |= PropertyChangeMask;

    return (CWOverrideRedirect | CWSaveUnder | CWColormap |
            CWEventMask | CWBitGravity | CWBackingStore);
}


/***********************************************************************
 *              create_client_window
 */
static Window create_client_window( Display *display, struct x11drv_win_data *data, XVisualInfo *vis )
{
    int cx, cy, mask;
    XSetWindowAttributes attr;
    Window client;
    Visual *client_visual = vis ? vis->visual : visual;

    attr.bit_gravity = NorthWestGravity;
    attr.win_gravity = NorthWestGravity;
    attr.backing_store = NotUseful;
    attr.event_mask = (ExposureMask | PointerMotionMask |
                       ButtonPressMask | ButtonReleaseMask | EnterWindowMask);
    mask = CWEventMask | CWBitGravity | CWWinGravity | CWBackingStore;

    if ((cx = data->client_rect.right - data->client_rect.left) <= 0) cx = 1;
    else if (cx > 65535) cx = 65535;
    if ((cy = data->client_rect.bottom - data->client_rect.top) <= 0) cy = 1;
    else if (cy > 65535) cy = 65535;

    wine_tsx11_lock();

    if (vis)
    {
        attr.colormap = XCreateColormap( display, root_window, vis->visual,
                                         (vis->class == PseudoColor || vis->class == GrayScale ||
                                          vis->class == DirectColor) ? AllocAll : AllocNone );
        mask |= CWColormap;
    }

    client = XCreateWindow( display, data->whole_window,
                            data->client_rect.left - data->whole_rect.left,
                            data->client_rect.top - data->whole_rect.top,
                            cx, cy, 0, screen_depth, InputOutput,
                            client_visual, mask, &attr );
    if (!client)
    {
        wine_tsx11_unlock();
        return 0;
    }

    if (data->client_window)
    {
        XDeleteContext( display, data->client_window, winContext );
        XDestroyWindow( display, data->client_window );
    }
    data->client_window = client;
    data->visualid = XVisualIDFromVisual( client_visual );

    if (data->colormap) XFreeColormap( display, data->colormap );
    data->colormap = vis ? attr.colormap : 0;

    XMapWindow( display, data->client_window );
    XSaveContext( display, data->client_window, winContext, (char *)data->hwnd );
    wine_tsx11_unlock();

    SetPropA( data->hwnd, client_window_prop, (HANDLE)data->client_window );
    return data->client_window;
}


/***********************************************************************
 *              sync_window_style
 *
 * Change the X window attributes when the window style has changed.
 */
static void sync_window_style( Display *display, struct x11drv_win_data *data )
{
    if (data->whole_window != root_window)
    {
        XSetWindowAttributes attr;
        int mask = get_window_attributes( display, data, &attr );

        wine_tsx11_lock();
        XChangeWindowAttributes( display, data->whole_window, mask, &attr );
        wine_tsx11_unlock();
    }
}


/***********************************************************************
 *              sync_window_region
 *
 * Update the X11 window region.
 */
static void sync_window_region( Display *display, struct x11drv_win_data *data, HRGN win_region )
{
#ifdef HAVE_LIBXSHAPE
    HRGN hrgn = win_region;

    if (!data->whole_window) return;
    data->shaped = FALSE;

    if (IsRectEmpty( &data->window_rect ))  /* set an empty shape */
    {
        static XRectangle empty_rect;
        wine_tsx11_lock();
        XShapeCombineRectangles( display, data->whole_window, ShapeBounding, 0, 0,
                                 &empty_rect, 1, ShapeSet, YXBanded );
        wine_tsx11_unlock();
        return;
    }

    if (hrgn == (HRGN)1)  /* hack: win_region == 1 means retrieve region from server */
    {
        if (!(hrgn = CreateRectRgn( 0, 0, 0, 0 ))) return;
        if (GetWindowRgn( data->hwnd, hrgn ) == ERROR)
        {
            DeleteObject( hrgn );
            hrgn = 0;
        }
    }

    if (!hrgn)
    {
        wine_tsx11_lock();
        XShapeCombineMask( display, data->whole_window, ShapeBounding, 0, 0, None, ShapeSet );
        wine_tsx11_unlock();
    }
    else
    {
        RGNDATA *pRegionData;

        if (GetWindowLongW( data->hwnd, GWL_EXSTYLE ) & WS_EX_LAYOUTRTL) MirrorRgn( data->hwnd, hrgn );
        if ((pRegionData = X11DRV_GetRegionData( hrgn, 0 )))
        {
            wine_tsx11_lock();
            XShapeCombineRectangles( display, data->whole_window, ShapeBounding,
                                     data->window_rect.left - data->whole_rect.left,
                                     data->window_rect.top - data->whole_rect.top,
                                     (XRectangle *)pRegionData->Buffer,
                                     pRegionData->rdh.nCount, ShapeSet, YXBanded );
            wine_tsx11_unlock();
            HeapFree(GetProcessHeap(), 0, pRegionData);
            data->shaped = TRUE;
        }
    }
    if (hrgn && hrgn != win_region) DeleteObject( hrgn );
#endif  /* HAVE_LIBXSHAPE */
}


/***********************************************************************
 *              sync_window_opacity
 */
static void sync_window_opacity( Display *display, Window win,
                                 COLORREF key, BYTE alpha, DWORD flags )
{
    unsigned long opacity = 0xffffffff;

    if (flags & LWA_ALPHA) opacity = (0xffffffff / 0xff) * alpha;

    if (flags & LWA_COLORKEY) FIXME("LWA_COLORKEY not supported\n");

    wine_tsx11_lock();
    if (opacity == 0xffffffff)
        XDeleteProperty( display, win, x11drv_atom(_NET_WM_WINDOW_OPACITY) );
    else
        XChangeProperty( display, win, x11drv_atom(_NET_WM_WINDOW_OPACITY),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1 );
    wine_tsx11_unlock();
}


/***********************************************************************
 *              sync_window_text
 */
static void sync_window_text( Display *display, Window win, const WCHAR *text )
{
    UINT count;
    char *buffer, *utf8_buffer;
    XTextProperty prop;

    /* allocate new buffer for window text */
    count = WideCharToMultiByte(CP_UNIXCP, 0, text, -1, NULL, 0, NULL, NULL);
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, count ))) return;
    WideCharToMultiByte(CP_UNIXCP, 0, text, -1, buffer, count, NULL, NULL);

    count = WideCharToMultiByte(CP_UTF8, 0, text, strlenW(text), NULL, 0, NULL, NULL);
    if (!(utf8_buffer = HeapAlloc( GetProcessHeap(), 0, count )))
    {
        HeapFree( GetProcessHeap(), 0, buffer );
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, text, strlenW(text), utf8_buffer, count, NULL, NULL);

    wine_tsx11_lock();
    if (XmbTextListToTextProperty( display, &buffer, 1, XStdICCTextStyle, &prop ) == Success)
    {
        XSetWMName( display, win, &prop );
        XSetWMIconName( display, win, &prop );
        XFree( prop.value );
    }
    /*
      Implements a NET_WM UTF-8 title. It should be without a trailing \0,
      according to the standard
      ( http://www.pps.jussieu.fr/~jch/software/UTF8_STRING/UTF8_STRING.text ).
    */
    XChangeProperty( display, win, x11drv_atom(_NET_WM_NAME), x11drv_atom(UTF8_STRING),
                     8, PropModeReplace, (unsigned char *) utf8_buffer, count);
    wine_tsx11_unlock();

    HeapFree( GetProcessHeap(), 0, utf8_buffer );
    HeapFree( GetProcessHeap(), 0, buffer );
}


/***********************************************************************
 *              set_win_format
 */
static BOOL set_win_format( HWND hwnd, XID fbconfig_id )
{
    struct x11drv_win_data *data;
    XVisualInfo *vis;
    int w, h;

    if (!(data = X11DRV_get_win_data(hwnd)) &&
        !(data = X11DRV_create_win_data(hwnd))) return FALSE;

    if (!(vis = visual_from_fbconfig_id(fbconfig_id))) return FALSE;

    if (data->whole_window)
    {
        Display *display = thread_display();
        Window client = data->client_window;

        if (vis->visualid != data->visualid)
        {
            client = create_client_window( display, data, vis );
            TRACE( "re-created client window %lx for %p fbconfig %lx\n", client, data->hwnd, fbconfig_id );
        }
        wine_tsx11_lock();
        XFree(vis);
        XFlush( display );
        wine_tsx11_unlock();
        if (client) goto done;
        return FALSE;
    }

    w = data->client_rect.right - data->client_rect.left;
    h = data->client_rect.bottom - data->client_rect.top;

    if(w <= 0) w = 1;
    if(h <= 0) h = 1;

#ifdef SONAME_LIBXCOMPOSITE
    if(usexcomposite)
    {
        XSetWindowAttributes attrib;
        static Window dummy_parent;

        wine_tsx11_lock();
        attrib.override_redirect = True;
        if (!dummy_parent)
        {
            dummy_parent = XCreateWindow( gdi_display, root_window, -1, -1, 1, 1, 0, screen_depth,
                                         InputOutput, visual, CWOverrideRedirect, &attrib );
            XMapWindow( gdi_display, dummy_parent );
        }
        data->colormap = XCreateColormap(gdi_display, dummy_parent, vis->visual,
                                         (vis->class == PseudoColor ||
                                          vis->class == GrayScale ||
                                          vis->class == DirectColor) ?
                                         AllocAll : AllocNone);
        attrib.colormap = data->colormap;
        XInstallColormap(gdi_display, attrib.colormap);

        if(data->gl_drawable) XDestroyWindow(gdi_display, data->gl_drawable);
        data->gl_drawable = XCreateWindow(gdi_display, dummy_parent, 0, 0, w, h, 0,
                                          vis->depth, InputOutput, vis->visual,
                                          CWColormap | CWOverrideRedirect,
                                          &attrib);
        if(data->gl_drawable)
        {
            pXCompositeRedirectWindow(gdi_display, data->gl_drawable,
                                      CompositeRedirectManual);
            XMapWindow(gdi_display, data->gl_drawable);
        }
        XFree(vis);
        XFlush( gdi_display );
        wine_tsx11_unlock();
    }
    else
#endif
    {
        WARN("XComposite is not available, using GLXPixmap hack\n");

        wine_tsx11_lock();

        if(data->pixmap) XFreePixmap(gdi_display, data->pixmap);
        data->pixmap = XCreatePixmap(gdi_display, root_window, w, h, vis->depth);
        if(!data->pixmap)
        {
            XFree(vis);
            wine_tsx11_unlock();
            return FALSE;
        }

        if(data->gl_drawable) destroy_glxpixmap(gdi_display, data->gl_drawable);
        data->gl_drawable = create_glxpixmap(gdi_display, vis, data->pixmap);
        if(!data->gl_drawable)
        {
            XFreePixmap(gdi_display, data->pixmap);
            data->pixmap = 0;
        }
        XFree(vis);
        XFlush( gdi_display );
        wine_tsx11_unlock();
        if (data->pixmap) SetPropA(hwnd, pixmap_prop, (HANDLE)data->pixmap);
    }

    if (!data->gl_drawable) return FALSE;

    TRACE("Created GL drawable 0x%lx, using FBConfigID 0x%lx\n",
          data->gl_drawable, fbconfig_id);
    SetPropA(hwnd, gl_drawable_prop, (HANDLE)data->gl_drawable);

done:
    data->fbconfig_id = fbconfig_id;
    SetPropA(hwnd, fbconfig_id_prop, (HANDLE)data->fbconfig_id);
    /* force DCE invalidation */
    SetWindowPos( hwnd, 0, 0, 0, 0, 0,
                  SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE |
                  SWP_NOREDRAW | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_STATECHANGED);
    return TRUE;
}

/***********************************************************************
 *              sync_gl_drawable
 */
static void sync_gl_drawable(struct x11drv_win_data *data)
{
    int w = data->client_rect.right - data->client_rect.left;
    int h = data->client_rect.bottom - data->client_rect.top;
    XVisualInfo *vis;
    Drawable glxp;
    Pixmap pix;

    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    TRACE("Resizing GL drawable 0x%lx to %dx%d\n", data->gl_drawable, w, h);
#ifdef SONAME_LIBXCOMPOSITE
    if(usexcomposite)
    {
        wine_tsx11_lock();
        XMoveResizeWindow(gdi_display, data->gl_drawable, 0, 0, w, h);
        wine_tsx11_unlock();
        return;
    }
#endif

    if (!(vis = visual_from_fbconfig_id(data->fbconfig_id))) return;

    wine_tsx11_lock();
    pix = XCreatePixmap(gdi_display, root_window, w, h, vis->depth);
    if(!pix)
    {
        ERR("Failed to create pixmap for offscreen rendering\n");
        XFree(vis);
        wine_tsx11_unlock();
        return;
    }

    glxp = create_glxpixmap(gdi_display, vis, pix);
    if(!glxp)
    {
        ERR("Failed to create drawable for offscreen rendering\n");
        XFreePixmap(gdi_display, pix);
        XFree(vis);
        wine_tsx11_unlock();
        return;
    }

    XFree(vis);

    mark_drawable_dirty(data->gl_drawable, glxp);

    XFreePixmap(gdi_display, data->pixmap);
    destroy_glxpixmap(gdi_display, data->gl_drawable);
    TRACE( "Recreated GL drawable %lx to replace %lx\n", glxp, data->gl_drawable );

    data->pixmap = pix;
    data->gl_drawable = glxp;

    XFlush( gdi_display );
    wine_tsx11_unlock();

    SetPropA(data->hwnd, gl_drawable_prop, (HANDLE)data->gl_drawable);
    SetPropA(data->hwnd, pixmap_prop, (HANDLE)data->pixmap);
}


/***********************************************************************
 *              get_window_changes
 *
 * fill the window changes structure
 */
static int get_window_changes( XWindowChanges *changes, const RECT *old, const RECT *new )
{
    int mask = 0;

    if (old->right - old->left != new->right - new->left )
    {
        if ((changes->width = new->right - new->left) <= 0) changes->width = 1;
        else if (changes->width > 65535) changes->width = 65535;
        mask |= CWWidth;
    }
    if (old->bottom - old->top != new->bottom - new->top)
    {
        if ((changes->height = new->bottom - new->top) <= 0) changes->height = 1;
        else if (changes->height > 65535) changes->height = 65535;
        mask |= CWHeight;
    }
    if (old->left != new->left)
    {
        changes->x = new->left;
        mask |= CWX;
    }
    if (old->top != new->top)
    {
        changes->y = new->top;
        mask |= CWY;
    }
    return mask;
}


/***********************************************************************
 *              create_icon_window
 */
static Window create_icon_window( Display *display, struct x11drv_win_data *data )
{
    XSetWindowAttributes attr;

    attr.event_mask = (ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask |
                       ButtonPressMask | ButtonReleaseMask | EnterWindowMask);
    attr.bit_gravity = NorthWestGravity;
    attr.backing_store = NotUseful/*WhenMapped*/;
    attr.colormap      = X11DRV_PALETTE_PaletteXColormap; /* Needed due to our visual */

    wine_tsx11_lock();
    data->icon_window = XCreateWindow( display, root_window, 0, 0,
                                       GetSystemMetrics( SM_CXICON ),
                                       GetSystemMetrics( SM_CYICON ),
                                       0, screen_depth,
                                       InputOutput, visual,
                                       CWEventMask | CWBitGravity | CWBackingStore | CWColormap, &attr );
    XSaveContext( display, data->icon_window, winContext, (char *)data->hwnd );
    XFlush( display );  /* make sure the window exists before we start painting to it */
    wine_tsx11_unlock();

    TRACE( "created %lx\n", data->icon_window );
    SetPropA( data->hwnd, icon_window_prop, (HANDLE)data->icon_window );
    return data->icon_window;
}



/***********************************************************************
 *              destroy_icon_window
 */
static void destroy_icon_window( Display *display, struct x11drv_win_data *data )
{
    if (!data->icon_window) return;
    wine_tsx11_lock();
    XDeleteContext( display, data->icon_window, winContext );
    XDestroyWindow( display, data->icon_window );
    data->icon_window = 0;
    wine_tsx11_unlock();
    RemovePropA( data->hwnd, icon_window_prop );
}


/***********************************************************************
 *              get_bitmap_argb
 *
 * Return the bitmap bits in ARGB format. Helper for setting icon hints.
 */
static unsigned long *get_bitmap_argb( HDC hdc, HBITMAP color, HBITMAP mask, unsigned int *size )
{
    BITMAP bm;
    BITMAPINFO *info;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j, has_alpha = 0;

    if (!GetObjectW( color, sizeof(bm), &bm )) return NULL;
    if (!(info = HeapAlloc( GetProcessHeap(), 0, FIELD_OFFSET( BITMAPINFO, bmiColors[256] )))) return NULL;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    *size = bm.bmWidth * bm.bmHeight + 2;
    if (!(bits = HeapAlloc( GetProcessHeap(), 0, *size * sizeof(long) ))) goto failed;
    if (!GetDIBits( hdc, color, 0, bm.bmHeight, bits + 2, info, DIB_RGB_COLORS )) goto failed;

    bits[0] = bm.bmWidth;
    bits[1] = bm.bmHeight;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i + 2] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = HeapAlloc( GetProcessHeap(), 0, info->bmiHeader.biSizeImage ))) goto failed;
        if (!GetDIBits( hdc, mask, 0, bm.bmHeight, mask_bits, info, DIB_RGB_COLORS )) goto failed;
        ptr = bits + 2;
        for (i = 0; i < bm.bmHeight; i++)
            for (j = 0; j < bm.bmWidth; j++, ptr++)
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80)) *ptr |= 0xff000000;
        HeapFree( GetProcessHeap(), 0, mask_bits );
    }
    HeapFree( GetProcessHeap(), 0, info );

    /* convert to array of longs */
    if (bits && sizeof(long) > sizeof(int))
        for (i = *size - 1; i >= 0; i--) ((unsigned long *)bits)[i] = bits[i];

    return (unsigned long *)bits;

failed:
    HeapFree( GetProcessHeap(), 0, info );
    HeapFree( GetProcessHeap(), 0, bits );
    HeapFree( GetProcessHeap(), 0, mask_bits );
    return NULL;
}


/***********************************************************************
 *              set_icon_hints
 *
 * Set the icon wm hints
 */
static void set_icon_hints( Display *display, struct x11drv_win_data *data,
                            HICON icon_big, HICON icon_small )
{
    XWMHints *hints = data->wm_hints;

    if (!icon_big)
    {
        icon_big = (HICON)SendMessageW( data->hwnd, WM_GETICON, ICON_BIG, 0 );
        if (!icon_big) icon_big = (HICON)GetClassLongPtrW( data->hwnd, GCLP_HICON );
    }
    if (!icon_small)
    {
        icon_small = (HICON)SendMessageW( data->hwnd, WM_GETICON, ICON_SMALL, 0 );
        if (!icon_small) icon_small = (HICON)GetClassLongPtrW( data->hwnd, GCLP_HICONSM );
    }

    if (data->hWMIconBitmap) DeleteObject( data->hWMIconBitmap );
    if (data->hWMIconMask) DeleteObject( data->hWMIconMask);
    data->hWMIconBitmap = 0;
    data->hWMIconMask = 0;

    if (!icon_big)
    {
        if (!data->icon_window) create_icon_window( display, data );
        hints->icon_window = data->icon_window;
        hints->flags = (hints->flags & ~(IconPixmapHint | IconMaskHint)) | IconWindowHint;
    }
    else
    {
        HBITMAP hbmOrig;
        RECT rcMask;
        BITMAP bm;
        ICONINFO ii, ii_small;
        HDC hDC;
        unsigned int size;
        unsigned long *bits;

        if (!GetIconInfo(icon_big, &ii)) return;

        GetObjectW(ii.hbmMask, sizeof(bm), &bm);
        rcMask.top    = 0;
        rcMask.left   = 0;
        rcMask.right  = bm.bmWidth;
        rcMask.bottom = bm.bmHeight;

        hDC = CreateCompatibleDC(0);
        bits = get_bitmap_argb( hDC, ii.hbmColor, ii.hbmMask, &size );
        if (bits && GetIconInfo( icon_small, &ii_small ))
        {
            unsigned int size_small;
            unsigned long *bits_small, *new;

            if ((bits_small = get_bitmap_argb( hDC, ii_small.hbmColor, ii_small.hbmMask, &size_small )) &&
                (bits_small[0] != bits[0] || bits_small[1] != bits[1]))  /* size must be different */
            {
                if ((new = HeapReAlloc( GetProcessHeap(), 0, bits,
                                        (size + size_small) * sizeof(unsigned long) )))
                {
                    bits = new;
                    memcpy( bits + size, bits_small, size_small * sizeof(unsigned long) );
                    size += size_small;
                }
            }
            HeapFree( GetProcessHeap(), 0, bits_small );
            DeleteObject( ii_small.hbmColor );
            DeleteObject( ii_small.hbmMask );
        }
        wine_tsx11_lock();
        if (bits)
            XChangeProperty( display, data->whole_window, x11drv_atom(_NET_WM_ICON),
                             XA_CARDINAL, 32, PropModeReplace, (unsigned char *)bits, size );
        else
            XDeleteProperty( display, data->whole_window, x11drv_atom(_NET_WM_ICON) );
        wine_tsx11_unlock();
        HeapFree( GetProcessHeap(), 0, bits );

        hbmOrig = SelectObject(hDC, ii.hbmMask);
        InvertRect(hDC, &rcMask);
        SelectObject(hDC, ii.hbmColor);  /* force the color bitmap to x11drv mode too */
        SelectObject(hDC, hbmOrig);

        data->hWMIconBitmap = ii.hbmColor;
        data->hWMIconMask = ii.hbmMask;

        hints->icon_pixmap = X11DRV_get_pixmap(data->hWMIconBitmap);
        hints->icon_mask = X11DRV_get_pixmap(data->hWMIconMask);
        destroy_icon_window( display, data );
        hints->flags = (hints->flags & ~IconWindowHint) | IconPixmapHint | IconMaskHint;

        DeleteDC(hDC);
    }
}


/***********************************************************************
 *              set_size_hints
 *
 * set the window size hints
 */
static void set_size_hints( Display *display, struct x11drv_win_data *data, DWORD style )
{
    XSizeHints* size_hints;

    if (!(size_hints = XAllocSizeHints())) return;

    size_hints->win_gravity = StaticGravity;
    size_hints->flags |= PWinGravity;

    /* don't update size hints if window is not in normal state */
    if (!(style & (WS_MINIMIZE | WS_MAXIMIZE)))
    {
        if (data->hwnd != GetDesktopWindow())  /* don't force position of desktop */
        {
            size_hints->x = data->whole_rect.left;
            size_hints->y = data->whole_rect.top;
            size_hints->flags |= PPosition;
        }
        else size_hints->win_gravity = NorthWestGravity;

        if (!is_window_resizable( data, style ))
        {
            size_hints->max_width = data->whole_rect.right - data->whole_rect.left;
            size_hints->max_height = data->whole_rect.bottom - data->whole_rect.top;
            if (size_hints->max_width <= 0 ||size_hints->max_height <= 0)
                size_hints->max_width = size_hints->max_height = 1;
            size_hints->min_width = size_hints->max_width;
            size_hints->min_height = size_hints->max_height;
            size_hints->flags |= PMinSize | PMaxSize;
        }
    }
    XSetWMNormalHints( display, data->whole_window, size_hints );
    XFree( size_hints );
}


/***********************************************************************
 *              set_mwm_hints
 */
static void set_mwm_hints( Display *display, struct x11drv_win_data *data, DWORD style, DWORD ex_style )
{
    MwmHints mwm_hints;

    if (data->hwnd == GetDesktopWindow())
    {
        mwm_hints.decorations = MWM_DECOR_TITLE | MWM_DECOR_BORDER | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE;
        mwm_hints.functions   = MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE;
    }
    else
    {
        mwm_hints.decorations = get_mwm_decorations( data, style, ex_style );
        mwm_hints.functions = MWM_FUNC_MOVE;
        if (is_window_resizable( data, style )) mwm_hints.functions |= MWM_FUNC_RESIZE;
        if (!(style & WS_DISABLED))
        {
            if (style & WS_MINIMIZEBOX) mwm_hints.functions |= MWM_FUNC_MINIMIZE;
            if (style & WS_MAXIMIZEBOX) mwm_hints.functions |= MWM_FUNC_MAXIMIZE;
            if (style & WS_SYSMENU)     mwm_hints.functions |= MWM_FUNC_CLOSE;
        }
    }

    TRACE( "%p setting mwm hints to %lx,%lx (style %x exstyle %x)\n",
           data->hwnd, mwm_hints.decorations, mwm_hints.functions, style, ex_style );

    mwm_hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    XChangeProperty( display, data->whole_window, x11drv_atom(_MOTIF_WM_HINTS),
                     x11drv_atom(_MOTIF_WM_HINTS), 32, PropModeReplace,
                     (unsigned char*)&mwm_hints, sizeof(mwm_hints)/sizeof(long) );
}


/***********************************************************************
 *              get_process_name
 *
 * get the name of the current process for setting class hints
 */
static char *get_process_name(void)
{
    static char *name;

    if (!name)
    {
        WCHAR module[MAX_PATH];
        DWORD len = GetModuleFileNameW( 0, module, MAX_PATH );
        if (len && len < MAX_PATH)
        {
            char *ptr;
            WCHAR *p, *appname = module;

            if ((p = strrchrW( appname, '/' ))) appname = p + 1;
            if ((p = strrchrW( appname, '\\' ))) appname = p + 1;
            len = WideCharToMultiByte( CP_UNIXCP, 0, appname, -1, NULL, 0, NULL, NULL );
            if ((ptr = HeapAlloc( GetProcessHeap(), 0, len )))
            {
                WideCharToMultiByte( CP_UNIXCP, 0, appname, -1, ptr, len, NULL, NULL );
                name = ptr;
            }
        }
    }
    return name;
}


/***********************************************************************
 *              set_initial_wm_hints
 *
 * Set the window manager hints that don't change over the lifetime of a window.
 */
static void set_initial_wm_hints( Display *display, struct x11drv_win_data *data )
{
    long i;
    Atom protocols[3];
    Atom dndVersion = WINE_XDND_VERSION;
    XClassHint *class_hints;
    char *process_name = get_process_name();

    wine_tsx11_lock();

    /* wm protocols */
    i = 0;
    protocols[i++] = x11drv_atom(WM_DELETE_WINDOW);
    protocols[i++] = x11drv_atom(_NET_WM_PING);
    if (use_take_focus) protocols[i++] = x11drv_atom(WM_TAKE_FOCUS);
    XChangeProperty( display, data->whole_window, x11drv_atom(WM_PROTOCOLS),
                     XA_ATOM, 32, PropModeReplace, (unsigned char *)protocols, i );

    /* class hints */
    if ((class_hints = XAllocClassHint()))
    {
        static char wine[] = "Wine";

        class_hints->res_name = process_name;
        class_hints->res_class = wine;
        XSetClassHint( display, data->whole_window, class_hints );
        XFree( class_hints );
    }

    /* set the WM_CLIENT_MACHINE and WM_LOCALE_NAME properties */
    XSetWMProperties(display, data->whole_window, NULL, NULL, NULL, 0, NULL, NULL, NULL);
    /* set the pid. together, these properties are needed so the window manager can kill us if we freeze */
    i = getpid();
    XChangeProperty(display, data->whole_window, x11drv_atom(_NET_WM_PID),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&i, 1);

    XChangeProperty( display, data->whole_window, x11drv_atom(XdndAware),
                     XA_ATOM, 32, PropModeReplace, (unsigned char*)&dndVersion, 1 );

    update_user_time( 0 );  /* make sure that the user time window exists */
    if (user_time_window)
        XChangeProperty( display, data->whole_window, x11drv_atom(_NET_WM_USER_TIME_WINDOW),
                         XA_WINDOW, 32, PropModeReplace, (unsigned char *)&user_time_window, 1 );

    data->wm_hints = XAllocWMHints();
    wine_tsx11_unlock();

    if (data->wm_hints)
    {
        data->wm_hints->flags = 0;
        set_icon_hints( display, data, 0, 0 );
    }
}


/***********************************************************************
 *              get_owner_whole_window
 *
 * Retrieve an owner's window, creating it if necessary.
 */
static Window get_owner_whole_window( HWND owner, BOOL force_managed )
{
    struct x11drv_win_data *data;

    if (!owner) return 0;

    if (!(data = X11DRV_get_win_data( owner )))
    {
        if (GetWindowThreadProcessId( owner, NULL ) != GetCurrentThreadId() ||
            !(data = X11DRV_create_win_data( owner )))
            return (Window)GetPropA( owner, whole_window_prop );
    }
    else if (!data->managed && force_managed)  /* make it managed */
    {
        SetWindowPos( owner, 0, 0, 0, 0, 0,
                      SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE |
                      SWP_NOREDRAW | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_STATECHANGED );
    }
    return data->whole_window;
}


/***********************************************************************
 *              set_wm_hints
 *
 * Set the window manager hints for a newly-created window
 */
static void set_wm_hints( Display *display, struct x11drv_win_data *data )
{
    Window group_leader = data->whole_window;
    Window owner_win = 0;
    Atom window_type;
    DWORD style, ex_style;
    HWND owner;

    if (data->hwnd == GetDesktopWindow())
    {
        /* force some styles for the desktop to get the correct decorations */
        style = WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        ex_style = WS_EX_APPWINDOW;
        owner = 0;
    }
    else
    {
        style = GetWindowLongW( data->hwnd, GWL_STYLE );
        ex_style = GetWindowLongW( data->hwnd, GWL_EXSTYLE );
        owner = GetWindow( data->hwnd, GW_OWNER );
        if ((owner_win = get_owner_whole_window( owner, data->managed ))) group_leader = owner_win;
    }

    wine_tsx11_lock();

    if (owner_win) XSetTransientForHint( display, data->whole_window, owner_win );

    /* size hints */
    set_size_hints( display, data, style );
    set_mwm_hints( display, data, style, ex_style );

    /* Only use dialog type for owned popups. Metacity allows making fullscreen
     * only normal windows, and doesn't handle correctly TRANSIENT_FOR hint for
     * dialogs owned by fullscreen windows.
     */
    if (((style & WS_POPUP) || (ex_style & WS_EX_DLGMODALFRAME)) && owner) window_type = x11drv_atom(_NET_WM_WINDOW_TYPE_DIALOG);
    else window_type = x11drv_atom(_NET_WM_WINDOW_TYPE_NORMAL);

    XChangeProperty(display, data->whole_window, x11drv_atom(_NET_WM_WINDOW_TYPE),
		    XA_ATOM, 32, PropModeReplace, (unsigned char*)&window_type, 1);

    /* wm hints */
    if (data->wm_hints)
    {
        data->wm_hints->flags |= InputHint | StateHint | WindowGroupHint;
        data->wm_hints->input = !use_take_focus && !(style & WS_DISABLED);
        data->wm_hints->initial_state = (style & WS_MINIMIZE) ? IconicState : NormalState;
        data->wm_hints->window_group = group_leader;
        XSetWMHints( display, data->whole_window, data->wm_hints );
    }

    wine_tsx11_unlock();
}


/***********************************************************************
 *     init_clip_window
 */
Window init_clip_window(void)
{
    struct x11drv_thread_data *data = x11drv_init_thread_data();

    if (!data->clip_window &&
        (data->clip_window = (Window)GetPropA( GetDesktopWindow(), clip_window_prop )))
    {
        wine_tsx11_lock();
        XSelectInput( data->display, data->clip_window, StructureNotifyMask );
        wine_tsx11_unlock();
    }
    return data->clip_window;
}


/***********************************************************************
 *     update_user_time
 */
void update_user_time( Time time )
{
    wine_tsx11_lock();
    if (!user_time_window)
    {
        user_time_window = XCreateWindow( gdi_display, root_window, -1, -1, 1, 1, 0, 0, InputOnly,
                                          DefaultVisual(gdi_display,DefaultScreen(gdi_display)), 0, NULL );
        TRACE( "user time window %lx\n", user_time_window );
    }
    if (time && (!last_user_time || (long)(time - last_user_time) > 0))
    {
        last_user_time = time;
        XChangeProperty( gdi_display, user_time_window, x11drv_atom(_NET_WM_USER_TIME),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&time, 1 );
    }
    wine_tsx11_unlock();
}

/***********************************************************************
 *     update_net_wm_states
 */
void update_net_wm_states( Display *display, struct x11drv_win_data *data )
{
    static const unsigned int state_atoms[NB_NET_WM_STATES] =
    {
        XATOM__NET_WM_STATE_FULLSCREEN,
        XATOM__NET_WM_STATE_ABOVE,
        XATOM__NET_WM_STATE_MAXIMIZED_VERT,
        XATOM__NET_WM_STATE_SKIP_PAGER,
        XATOM__NET_WM_STATE_SKIP_TASKBAR
    };

    DWORD i, style, ex_style, new_state = 0;

    if (!data->managed) return;
    if (data->whole_window == root_window) return;

    style = GetWindowLongW( data->hwnd, GWL_STYLE );
    if (is_window_rect_fullscreen( &data->whole_rect ))
    {
        if ((style & WS_MAXIMIZE) && (style & WS_CAPTION) == WS_CAPTION)
            new_state |= (1 << NET_WM_STATE_MAXIMIZED);
        else if (!(style & WS_MINIMIZE))
            new_state |= (1 << NET_WM_STATE_FULLSCREEN);
    }
    else if (style & WS_MAXIMIZE)
        new_state |= (1 << NET_WM_STATE_MAXIMIZED);

    ex_style = GetWindowLongW( data->hwnd, GWL_EXSTYLE );
    if (ex_style & WS_EX_TOPMOST)
        new_state |= (1 << NET_WM_STATE_ABOVE);
    if (ex_style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))
        new_state |= (1 << NET_WM_STATE_SKIP_TASKBAR) | (1 << NET_WM_STATE_SKIP_PAGER);
    if (!(ex_style & WS_EX_APPWINDOW) && GetWindow( data->hwnd, GW_OWNER ))
        new_state |= (1 << NET_WM_STATE_SKIP_TASKBAR);

    if (!data->mapped)  /* set the _NET_WM_STATE atom directly */
    {
        Atom atoms[NB_NET_WM_STATES+1];
        DWORD count;

        for (i = count = 0; i < NB_NET_WM_STATES; i++)
        {
            if (!(new_state & (1 << i))) continue;
            TRACE( "setting wm state %u for unmapped window %p/%lx\n",
                   i, data->hwnd, data->whole_window );
            atoms[count++] = X11DRV_Atoms[state_atoms[i] - FIRST_XATOM];
            if (state_atoms[i] == XATOM__NET_WM_STATE_MAXIMIZED_VERT)
                atoms[count++] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ);
        }
        wine_tsx11_lock();
        XChangeProperty( display, data->whole_window, x11drv_atom(_NET_WM_STATE), XA_ATOM,
                         32, PropModeReplace, (unsigned char *)atoms, count );
        wine_tsx11_unlock();
    }
    else  /* ask the window manager to do it for us */
    {
        XEvent xev;

        xev.xclient.type = ClientMessage;
        xev.xclient.window = data->whole_window;
        xev.xclient.message_type = x11drv_atom(_NET_WM_STATE);
        xev.xclient.serial = 0;
        xev.xclient.display = display;
        xev.xclient.send_event = True;
        xev.xclient.format = 32;
        xev.xclient.data.l[3] = 1;

        for (i = 0; i < NB_NET_WM_STATES; i++)
        {
            if (!((data->net_wm_state ^ new_state) & (1 << i))) continue;  /* unchanged */

            TRACE( "setting wm state %u for window %p/%lx to %u prev %u\n",
                   i, data->hwnd, data->whole_window,
                   (new_state & (1 << i)) != 0, (data->net_wm_state & (1 << i)) != 0 );

            xev.xclient.data.l[0] = (new_state & (1 << i)) ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
            xev.xclient.data.l[1] = X11DRV_Atoms[state_atoms[i] - FIRST_XATOM];
            xev.xclient.data.l[2] = ((state_atoms[i] == XATOM__NET_WM_STATE_MAXIMIZED_VERT) ?
                                     x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ) : 0);
            wine_tsx11_lock();
            XSendEvent( display, root_window, False,
                        SubstructureRedirectMask | SubstructureNotifyMask, &xev );
            wine_tsx11_unlock();
        }
    }
    data->net_wm_state = new_state;
}


/***********************************************************************
 *     set_xembed_flags
 */
static void set_xembed_flags( Display *display, struct x11drv_win_data *data, unsigned long flags )
{
    unsigned long info[2];

    if (!data->whole_window) return;

    info[0] = 0; /* protocol version */
    info[1] = flags;
    wine_tsx11_lock();
    XChangeProperty( display, data->whole_window, x11drv_atom(_XEMBED_INFO),
                     x11drv_atom(_XEMBED_INFO), 32, PropModeReplace, (unsigned char*)info, 2 );
    wine_tsx11_unlock();
}


/***********************************************************************
 *     map_window
 */
static void map_window( Display *display, struct x11drv_win_data *data, DWORD new_style )
{
    TRACE( "win %p/%lx\n", data->hwnd, data->whole_window );

    remove_startup_notification( display, data->whole_window );

    wait_for_withdrawn_state( display, data, TRUE );

    if (!data->embedded)
    {
        update_net_wm_states( display, data );
        sync_window_style( display, data );
        wine_tsx11_lock();
        XMapWindow( display, data->whole_window );
        wine_tsx11_unlock();
    }
    else set_xembed_flags( display, data, XEMBED_MAPPED );

    data->mapped = TRUE;
    data->iconic = (new_style & WS_MINIMIZE) != 0;
}


/***********************************************************************
 *     unmap_window
 */
static void unmap_window( Display *display, struct x11drv_win_data *data )
{
    TRACE( "win %p/%lx\n", data->hwnd, data->whole_window );

    if (!data->embedded)
    {
        wait_for_withdrawn_state( display, data, FALSE );
        wine_tsx11_lock();
        if (data->managed) XWithdrawWindow( display, data->whole_window, DefaultScreen(display) );
        else XUnmapWindow( display, data->whole_window );
        wine_tsx11_unlock();
    }
    else set_xembed_flags( display, data, 0 );

    data->mapped = FALSE;
    data->net_wm_state = 0;
}


/***********************************************************************
 *     make_window_embedded
 */
void make_window_embedded( Display *display, struct x11drv_win_data *data )
{
    BOOL was_mapped = data->mapped;
    /* the window cannot be mapped before being embedded */
    if (data->mapped) unmap_window( display, data );

    data->embedded = TRUE;
    data->managed = TRUE;
    SetPropA( data->hwnd, managed_prop, (HANDLE)1 );
    sync_window_style( display, data );

    if (was_mapped)
        map_window( display, data, 0 );
    else
        set_xembed_flags( display, data, 0 );
}


/***********************************************************************
 *		X11DRV_window_to_X_rect
 *
 * Convert a rect from client to X window coordinates
 */
static void X11DRV_window_to_X_rect( struct x11drv_win_data *data, RECT *rect )
{
    RECT rc;

    if (!data->managed) return;
    if (IsRectEmpty( rect )) return;

    get_x11_rect_offset( data, &rc );

    rect->left   -= rc.left;
    rect->right  -= rc.right;
    rect->top    -= rc.top;
    rect->bottom -= rc.bottom;
    if (rect->top >= rect->bottom) rect->bottom = rect->top + 1;
    if (rect->left >= rect->right) rect->right = rect->left + 1;
}


/***********************************************************************
 *		X11DRV_X_to_window_rect
 *
 * Opposite of X11DRV_window_to_X_rect
 */
void X11DRV_X_to_window_rect( struct x11drv_win_data *data, RECT *rect )
{
    RECT rc;

    if (!data->managed) return;
    if (IsRectEmpty( rect )) return;

    get_x11_rect_offset( data, &rc );

    rect->left   += rc.left;
    rect->right  += rc.right;
    rect->top    += rc.top;
    rect->bottom += rc.bottom;
    if (rect->top >= rect->bottom) rect->bottom = rect->top + 1;
    if (rect->left >= rect->right) rect->right = rect->left + 1;
}


/***********************************************************************
 *		sync_window_position
 *
 * Synchronize the X window position with the Windows one
 */
static void sync_window_position( Display *display, struct x11drv_win_data *data,
                                  UINT swp_flags, const RECT *old_window_rect,
                                  const RECT *old_whole_rect, const RECT *old_client_rect )
{
    DWORD style = GetWindowLongW( data->hwnd, GWL_STYLE );
    DWORD ex_style = GetWindowLongW( data->hwnd, GWL_EXSTYLE );
    XWindowChanges changes;
    unsigned int mask = 0;

    if (data->managed && data->iconic) return;

    /* resizing a managed maximized window is not allowed */
    if (!(style & WS_MAXIMIZE) || !data->managed)
    {
        changes.width = data->whole_rect.right - data->whole_rect.left;
        changes.height = data->whole_rect.bottom - data->whole_rect.top;
        /* if window rect is empty force size to 1x1 */
        if (changes.width <= 0 || changes.height <= 0) changes.width = changes.height = 1;
        if (changes.width > 65535) changes.width = 65535;
        if (changes.height > 65535) changes.height = 65535;
        mask |= CWWidth | CWHeight;
    }

    /* only the size is allowed to change for the desktop window */
    if (data->whole_window != root_window)
    {
        changes.x = data->whole_rect.left - virtual_screen_rect.left;
        changes.y = data->whole_rect.top - virtual_screen_rect.top;
        mask |= CWX | CWY;
    }

    if (!(swp_flags & SWP_NOZORDER) || (swp_flags & SWP_SHOWWINDOW))
    {
        /* find window that this one must be after */
        HWND prev = GetWindow( data->hwnd, GW_HWNDPREV );
        while (prev && !(GetWindowLongW( prev, GWL_STYLE ) & WS_VISIBLE))
            prev = GetWindow( prev, GW_HWNDPREV );
        if (!prev)  /* top child */
        {
            changes.stack_mode = Above;
            mask |= CWStackMode;
        }
        /* should use stack_mode Below but most window managers don't get it right */
        /* and Above with a sibling doesn't work so well either, so we ignore it */
    }

    wine_tsx11_lock();
    set_size_hints( display, data, style );
    set_mwm_hints( display, data, style, ex_style );
    data->configure_serial = NextRequest( display );
    XReconfigureWMWindow( display, data->whole_window,
                          DefaultScreen(display), mask, &changes );
    wine_tsx11_unlock();
#ifdef HAVE_LIBXSHAPE
    if (IsRectEmpty( old_window_rect ) != IsRectEmpty( &data->window_rect ))
        sync_window_region( display, data, (HRGN)1 );
    if (data->shaped)
    {
        int old_x_offset = old_window_rect->left - old_whole_rect->left;
        int old_y_offset = old_window_rect->top - old_whole_rect->top;
        int new_x_offset = data->window_rect.left - data->whole_rect.left;
        int new_y_offset = data->window_rect.top - data->whole_rect.top;
        if (old_x_offset != new_x_offset || old_y_offset != new_y_offset)
        {
            wine_tsx11_lock();
            XShapeOffsetShape( display, data->whole_window, ShapeBounding,
                               new_x_offset - old_x_offset, new_y_offset - old_y_offset );
            wine_tsx11_unlock();
        }
    }
#endif

    TRACE( "win %p/%lx pos %d,%d,%dx%d after %lx changes=%x serial=%lu\n",
           data->hwnd, data->whole_window, data->whole_rect.left, data->whole_rect.top,
           data->whole_rect.right - data->whole_rect.left,
           data->whole_rect.bottom - data->whole_rect.top,
           changes.sibling, mask, data->configure_serial );
}


/***********************************************************************
 *		sync_client_position
 *
 * Synchronize the X client window position with the Windows one
 */
static void sync_client_position( Display *display, struct x11drv_win_data *data,
                                  UINT swp_flags, const RECT *old_client_rect,
                                  const RECT *old_whole_rect )
{
    int mask;
    XWindowChanges changes;
    RECT old = *old_client_rect;
    RECT new = data->client_rect;

    OffsetRect( &old, -old_whole_rect->left, -old_whole_rect->top );
    OffsetRect( &new, -data->whole_rect.left, -data->whole_rect.top );
    if (!(mask = get_window_changes( &changes, &old, &new ))) return;

    if (data->client_window)
    {
        TRACE( "setting client win %lx pos %d,%d,%dx%d changes=%x\n",
               data->client_window, new.left, new.top,
               new.right - new.left, new.bottom - new.top, mask );
        wine_tsx11_lock();
        XConfigureWindow( display, data->client_window, mask, &changes );
        wine_tsx11_unlock();
    }

    if (data->gl_drawable && (mask & (CWWidth|CWHeight))) sync_gl_drawable( data );
}


/***********************************************************************
 *		move_window_bits
 *
 * Move the window bits when a window is moved.
 */
static void move_window_bits( struct x11drv_win_data *data, const RECT *old_rect, const RECT *new_rect,
                              const RECT *old_client_rect )
{
    RECT src_rect = *old_rect;
    RECT dst_rect = *new_rect;
    HDC hdc_src, hdc_dst;
    INT code;
    HRGN rgn;
    HWND parent = 0;

    if (!data->whole_window)
    {
        OffsetRect( &dst_rect, -data->window_rect.left, -data->window_rect.top );
        parent = GetAncestor( data->hwnd, GA_PARENT );
        hdc_src = GetDCEx( parent, 0, DCX_CACHE );
        hdc_dst = GetDCEx( data->hwnd, 0, DCX_CACHE | DCX_WINDOW );
    }
    else
    {
        OffsetRect( &dst_rect, -data->client_rect.left, -data->client_rect.top );
        /* make src rect relative to the old position of the window */
        OffsetRect( &src_rect, -old_client_rect->left, -old_client_rect->top );
        if (dst_rect.left == src_rect.left && dst_rect.top == src_rect.top) return;
        hdc_src = hdc_dst = GetDCEx( data->hwnd, 0, DCX_CACHE );
    }

    rgn = CreateRectRgnIndirect( &dst_rect );
    SelectClipRgn( hdc_dst, rgn );
    DeleteObject( rgn );
    ExcludeUpdateRgn( hdc_dst, data->hwnd );

    code = X11DRV_START_EXPOSURES;
    ExtEscape( hdc_dst, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code, 0, NULL );

    TRACE( "copying bits for win %p/%lx/%lx %s -> %s\n",
           data->hwnd, data->whole_window, data->client_window,
           wine_dbgstr_rect(&src_rect), wine_dbgstr_rect(&dst_rect) );
    BitBlt( hdc_dst, dst_rect.left, dst_rect.top,
            dst_rect.right - dst_rect.left, dst_rect.bottom - dst_rect.top,
            hdc_src, src_rect.left, src_rect.top, SRCCOPY );

    rgn = 0;
    code = X11DRV_END_EXPOSURES;
    ExtEscape( hdc_dst, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code, sizeof(rgn), (LPSTR)&rgn );

    ReleaseDC( data->hwnd, hdc_dst );
    if (hdc_src != hdc_dst) ReleaseDC( parent, hdc_src );

    if (rgn)
    {
        if (!data->whole_window)
        {
            /* map region to client rect since we are using DCX_WINDOW */
            OffsetRgn( rgn, data->window_rect.left - data->client_rect.left,
                       data->window_rect.top - data->client_rect.top );
            RedrawWindow( data->hwnd, NULL, rgn,
                          RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_ALLCHILDREN );
        }
        else RedrawWindow( data->hwnd, NULL, rgn, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
        DeleteObject( rgn );
    }
}


/**********************************************************************
 *		create_whole_window
 *
 * Create the whole X window for a given window
 */
static Window create_whole_window( Display *display, struct x11drv_win_data *data )
{
    int cx, cy, mask;
    XSetWindowAttributes attr;
    WCHAR text[1024];
    COLORREF key;
    BYTE alpha;
    DWORD layered_flags;
    HRGN win_rgn;

    if (!data->managed && is_window_managed( data->hwnd, SWP_NOACTIVATE, &data->window_rect ))
    {
        TRACE( "making win %p/%lx managed\n", data->hwnd, data->whole_window );
        data->managed = TRUE;
        SetPropA( data->hwnd, managed_prop, (HANDLE)1 );
    }

    if ((win_rgn = CreateRectRgn( 0, 0, 0, 0 )) &&
        GetWindowRgn( data->hwnd, win_rgn ) == ERROR)
    {
        DeleteObject( win_rgn );
        win_rgn = 0;
    }
    data->shaped = (win_rgn != 0);

    mask = get_window_attributes( display, data, &attr );

    data->whole_rect = data->window_rect;
    X11DRV_window_to_X_rect( data, &data->whole_rect );
    if (!(cx = data->whole_rect.right - data->whole_rect.left)) cx = 1;
    else if (cx > 65535) cx = 65535;
    if (!(cy = data->whole_rect.bottom - data->whole_rect.top)) cy = 1;
    else if (cy > 65535) cy = 65535;

    wine_tsx11_lock();
    data->whole_window = XCreateWindow( display, root_window,
                                        data->whole_rect.left - virtual_screen_rect.left,
                                        data->whole_rect.top - virtual_screen_rect.top,
                                        cx, cy, 0, screen_depth, InputOutput,
                                        visual, mask, &attr );

    if (data->whole_window) XSaveContext( display, data->whole_window, winContext, (char *)data->hwnd );
    wine_tsx11_unlock();

    if (!data->whole_window) goto done;

    if (!create_client_window( display, data, NULL ))
    {
        wine_tsx11_lock();
        XDeleteContext( display, data->whole_window, winContext );
        XDestroyWindow( display, data->whole_window );
        data->whole_window = 0;
        wine_tsx11_unlock();
        goto done;
    }

    set_initial_wm_hints( display, data );
    set_wm_hints( display, data );

    SetPropA( data->hwnd, whole_window_prop, (HANDLE)data->whole_window );

    /* set the window text */
    if (!InternalGetWindowText( data->hwnd, text, sizeof(text)/sizeof(WCHAR) )) text[0] = 0;
    sync_window_text( display, data->whole_window, text );

    /* set the window region */
    if (win_rgn || IsRectEmpty( &data->window_rect )) sync_window_region( display, data, win_rgn );

    /* set the window opacity */
    if (!GetLayeredWindowAttributes( data->hwnd, &key, &alpha, &layered_flags )) layered_flags = 0;
    sync_window_opacity( display, data->whole_window, key, alpha, layered_flags );

    init_clip_window();  /* make sure the clip window is initialized in this thread */

    wine_tsx11_lock();
    XFlush( display );  /* make sure the window exists before we start painting to it */
    wine_tsx11_unlock();

    sync_window_cursor( data->whole_window );

done:
    if (win_rgn) DeleteObject( win_rgn );
    return data->whole_window;
}


/**********************************************************************
 *		destroy_whole_window
 *
 * Destroy the whole X window for a given window.
 */
static void destroy_whole_window( Display *display, struct x11drv_win_data *data, BOOL already_destroyed )
{
    if (!data->whole_window)
    {
        if (data->embedded)
        {
            Window xwin = (Window)GetPropA( data->hwnd, foreign_window_prop );
            if (xwin)
            {
                wine_tsx11_lock();
                if (!already_destroyed) XSelectInput( display, xwin, 0 );
                XDeleteContext( display, xwin, winContext );
                wine_tsx11_unlock();
                RemovePropA( data->hwnd, foreign_window_prop );
            }
        }
        return;
    }


    TRACE( "win %p xwin %lx/%lx\n", data->hwnd, data->whole_window, data->client_window );
    wine_tsx11_lock();
    XDeleteContext( display, data->whole_window, winContext );
    XDeleteContext( display, data->client_window, winContext );
    if (!already_destroyed) XDestroyWindow( display, data->whole_window );
    data->whole_window = data->client_window = 0;
    data->wm_state = WithdrawnState;
    data->net_wm_state = 0;
    data->mapped = FALSE;
    if (data->xic)
    {
        XUnsetICFocus( data->xic );
        XDestroyIC( data->xic );
        data->xic = 0;
    }
    /* Outlook stops processing messages after destroying a dialog, so we need an explicit flush */
    XFlush( display );
    XFree( data->wm_hints );
    data->wm_hints = NULL;
    wine_tsx11_unlock();
    RemovePropA( data->hwnd, whole_window_prop );
    RemovePropA( data->hwnd, client_window_prop );
}


/*****************************************************************
 *		SetWindowText   (X11DRV.@)
 */
void CDECL X11DRV_SetWindowText( HWND hwnd, LPCWSTR text )
{
    Window win;

    if ((win = X11DRV_get_whole_window( hwnd )) && win != DefaultRootWindow(gdi_display))
    {
        Display *display = thread_init_display();
        sync_window_text( display, win, text );
    }
}


/***********************************************************************
 *		SetWindowStyle   (X11DRV.@)
 *
 * Update the X state of a window to reflect a style change
 */
void CDECL X11DRV_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );
    DWORD changed;

    if (hwnd == GetDesktopWindow()) return;
    changed = style->styleNew ^ style->styleOld;

    /* if WS_VISIBLE was set through WM_SETREDRAW, map the window if it's the first time */
    if (offset == GWL_STYLE && (changed & WS_VISIBLE) && (style->styleNew & WS_VISIBLE) && !data)
    {
        if (!(data = X11DRV_create_win_data( hwnd ))) return;

        if (data->whole_window && is_window_rect_mapped( &data->window_rect ))
        {
            Display *display = thread_display();
            set_wm_hints( display, data );
            if (!data->mapped) map_window( display, data, style->styleNew );
        }
    }
    if (!data || !data->whole_window) return;

    if (offset == GWL_STYLE && (changed & WS_DISABLED))
        set_wm_hints( thread_display(), data );

    if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYERED)) /* changing WS_EX_LAYERED resets attributes */
        sync_window_opacity( thread_display(), data->whole_window, 0, 0, 0 );
}


/***********************************************************************
 *		DestroyWindow   (X11DRV.@)
 */
void CDECL X11DRV_DestroyWindow( HWND hwnd )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_win_data *data;

    if (!(data = X11DRV_get_win_data( hwnd ))) return;

    if (data->pixmap)
    {
        wine_tsx11_lock();
        destroy_glxpixmap(gdi_display, data->gl_drawable);
        XFreePixmap(gdi_display, data->pixmap);
        wine_tsx11_unlock();
    }
    else if (data->gl_drawable)
    {
        wine_tsx11_lock();
        XDestroyWindow(gdi_display, data->gl_drawable);
        wine_tsx11_unlock();
    }

    destroy_whole_window( thread_data->display, data, FALSE );
    destroy_icon_window( thread_data->display, data );

    if (data->colormap)
    {
        wine_tsx11_lock();
        XFreeColormap( thread_data->display, data->colormap );
        wine_tsx11_unlock();
    }

    if (thread_data->last_focus == hwnd) thread_data->last_focus = 0;
    if (thread_data->last_xic_hwnd == hwnd) thread_data->last_xic_hwnd = 0;
    if (data->hWMIconBitmap) DeleteObject( data->hWMIconBitmap );
    if (data->hWMIconMask) DeleteObject( data->hWMIconMask);
    wine_tsx11_lock();
    XDeleteContext( thread_data->display, (XID)hwnd, win_data_context );
    wine_tsx11_unlock();
    HeapFree( GetProcessHeap(), 0, data );
}


/***********************************************************************
 *		X11DRV_DestroyNotify
 */
void X11DRV_DestroyNotify( HWND hwnd, XEvent *event )
{
    Display *display = event->xdestroywindow.display;
    struct x11drv_win_data *data;

    if (!(data = X11DRV_get_win_data( hwnd ))) return;
    if (!data->embedded) FIXME( "window %p/%lx destroyed from the outside\n", hwnd, data->whole_window );

    destroy_whole_window( display, data, TRUE );
    if (data->embedded) SendMessageW( hwnd, WM_CLOSE, 0, 0 );
}


static struct x11drv_win_data *alloc_win_data( Display *display, HWND hwnd )
{
    struct x11drv_win_data *data;

    if ((data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data))))
    {
        data->hwnd = hwnd;
        wine_tsx11_lock();
        if (!winContext) winContext = XUniqueContext();
        if (!win_data_context) win_data_context = XUniqueContext();
        XSaveContext( display, (XID)hwnd, win_data_context, (char *)data );
        wine_tsx11_unlock();
    }
    return data;
}


/* initialize the desktop window id in the desktop manager process */
static struct x11drv_win_data *create_desktop_win_data( Display *display, HWND hwnd )
{
    struct x11drv_win_data *data;

    if (!(data = alloc_win_data( display, hwnd ))) return NULL;
    data->whole_window = data->client_window = root_window;
    data->managed = TRUE;
    SetPropA( data->hwnd, managed_prop, (HANDLE)1 );
    SetPropA( data->hwnd, whole_window_prop, (HANDLE)root_window );
    SetPropA( data->hwnd, client_window_prop, (HANDLE)root_window );
    set_initial_wm_hints( display, data );
    return data;
}

/**********************************************************************
 *		CreateDesktopWindow   (X11DRV.@)
 */
BOOL CDECL X11DRV_CreateDesktopWindow( HWND hwnd )
{
    unsigned int width, height;

    /* retrieve the real size of the desktop */
    SERVER_START_REQ( get_window_rectangles )
    {
        req->handle = wine_server_user_handle( hwnd );
        req->relative = COORDS_CLIENT;
        wine_server_call( req );
        width  = reply->window.right;
        height = reply->window.bottom;
    }
    SERVER_END_REQ;

    if (!width && !height)  /* not initialized yet */
    {
        SERVER_START_REQ( set_window_pos )
        {
            req->handle        = wine_server_user_handle( hwnd );
            req->previous      = 0;
            req->flags         = SWP_NOZORDER;
            req->window.left   = virtual_screen_rect.left;
            req->window.top    = virtual_screen_rect.top;
            req->window.right  = virtual_screen_rect.right;
            req->window.bottom = virtual_screen_rect.bottom;
            req->client        = req->window;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }
    else
    {
        Window win = (Window)GetPropA( hwnd, whole_window_prop );
        if (win && win != root_window) X11DRV_init_desktop( win, width, height );
    }
    return TRUE;
}


/**********************************************************************
 *		CreateWindow   (X11DRV.@)
 */
BOOL CDECL X11DRV_CreateWindow( HWND hwnd )
{
    if (hwnd == GetDesktopWindow())
    {
        struct x11drv_thread_data *data = x11drv_init_thread_data();
        XSetWindowAttributes attr;

        if (root_window != DefaultRootWindow( gdi_display ))
        {
            /* the desktop win data can't be created lazily */
            if (!create_desktop_win_data( data->display, hwnd )) return FALSE;
        }

        /* create the cursor clipping window */
        attr.override_redirect = TRUE;
        attr.event_mask = StructureNotifyMask | FocusChangeMask;
        wine_tsx11_lock();
        data->clip_window = XCreateWindow( data->display, root_window, 0, 0, 1, 1, 0, 0,
                                           InputOnly, visual, CWOverrideRedirect | CWEventMask, &attr );
        XFlush( data->display );
        wine_tsx11_unlock();
        SetPropA( hwnd, clip_window_prop, (HANDLE)data->clip_window );
    }
    return TRUE;
}


/***********************************************************************
 *		X11DRV_get_win_data
 *
 * Return the X11 data structure associated with a window.
 */
struct x11drv_win_data *X11DRV_get_win_data( HWND hwnd )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    char *data;

    if (!thread_data) return NULL;
    if (!hwnd) return NULL;
    if (XFindContext( thread_data->display, (XID)hwnd, win_data_context, &data )) data = NULL;
    return (struct x11drv_win_data *)data;
}


/***********************************************************************
 *		X11DRV_create_win_data
 *
 * Create an X11 data window structure for an existing window.
 */
struct x11drv_win_data *X11DRV_create_win_data( HWND hwnd )
{
    Display *display;
    struct x11drv_win_data *data;
    HWND parent;

    if (!(parent = GetAncestor( hwnd, GA_PARENT ))) return NULL;  /* desktop */

    /* don't create win data for HWND_MESSAGE windows */
    if (parent != GetDesktopWindow() && !GetAncestor( parent, GA_PARENT )) return NULL;

    display = thread_init_display();
    if (!(data = alloc_win_data( display, hwnd ))) return NULL;

    GetWindowRect( hwnd, &data->window_rect );
    MapWindowPoints( 0, parent, (POINT *)&data->window_rect, 2 );
    data->whole_rect = data->window_rect;
    GetClientRect( hwnd, &data->client_rect );
    MapWindowPoints( hwnd, parent, (POINT *)&data->client_rect, 2 );

    if (parent == GetDesktopWindow())
    {
        if (!create_whole_window( display, data ))
        {
            HeapFree( GetProcessHeap(), 0, data );
            return NULL;
        }
        TRACE( "win %p/%lx/%lx window %s whole %s client %s\n",
               hwnd, data->whole_window, data->client_window, wine_dbgstr_rect( &data->window_rect ),
               wine_dbgstr_rect( &data->whole_rect ), wine_dbgstr_rect( &data->client_rect ));
    }
    return data;
}


/* window procedure for foreign windows */
static LRESULT WINAPI foreign_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    switch(msg)
    {
    case WM_WINDOWPOSCHANGED:
        update_systray_balloon_position();
        break;
    case WM_PARENTNOTIFY:
        if (LOWORD(wparam) == WM_DESTROY)
        {
            TRACE( "%p: got parent notify destroy for win %lx\n", hwnd, lparam );
            PostMessageW( hwnd, WM_CLOSE, 0, 0 );  /* so that we come back here once the child is gone */
        }
        return 0;
    case WM_CLOSE:
        if (GetWindow( hwnd, GW_CHILD )) return 0;  /* refuse to die if we still have children */
        break;
    }
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}


/***********************************************************************
 *		create_foreign_window
 *
 * Create a foreign window for the specified X window and its ancestors
 */
HWND create_foreign_window( Display *display, Window xwin )
{
    static const WCHAR classW[] = {'_','_','w','i','n','e','_','x','1','1','_','f','o','r','e','i','g','n','_','w','i','n','d','o','w',0};
    static BOOL class_registered;
    struct x11drv_win_data *data;
    HWND hwnd, parent;
    Window xparent, xroot;
    Window *xchildren;
    unsigned int nchildren;
    XWindowAttributes attr;
    DWORD style = WS_CLIPCHILDREN;

    if (!class_registered)
    {
        WNDCLASSEXW class;

        memset( &class, 0, sizeof(class) );
        class.cbSize        = sizeof(class);
        class.lpfnWndProc   = foreign_window_proc;
        class.lpszClassName = classW;
        if (!RegisterClassExW( &class ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            ERR( "Could not register foreign window class\n" );
            return FALSE;
        }
        class_registered = TRUE;
    }

    wine_tsx11_lock();
    if (XFindContext( display, xwin, winContext, (char **)&hwnd )) hwnd = 0;
    if (hwnd)  /* already created */
    {
        wine_tsx11_unlock();
        return hwnd;
    }

    XSelectInput( display, xwin, StructureNotifyMask );
    if (!XGetWindowAttributes( display, xwin, &attr ) ||
        !XQueryTree( display, xwin, &xroot, &xparent, &xchildren, &nchildren ))
    {
        XSelectInput( display, xwin, 0 );
        wine_tsx11_unlock();
        return 0;
    }
    XFree( xchildren );
    wine_tsx11_unlock();

    if (xparent == xroot)
    {
        parent = GetDesktopWindow();
        style |= WS_POPUP;
        attr.x += virtual_screen_rect.left;
        attr.y += virtual_screen_rect.top;
    }
    else
    {
        parent = create_foreign_window( display, xparent );
        style |= WS_CHILD;
    }

    hwnd = CreateWindowW( classW, NULL, style, attr.x, attr.y, attr.width, attr.height,
                          parent, 0, 0, NULL );

    if (!(data = alloc_win_data( display, hwnd )))
    {
        DestroyWindow( hwnd );
        return 0;
    }
    SetRect( &data->window_rect, attr.x, attr.y, attr.x + attr.width, attr.y + attr.height );
    data->whole_rect = data->client_rect = data->window_rect;
    data->whole_window = data->client_window = 0;
    data->embedded = TRUE;
    data->mapped = TRUE;

    SetPropA( hwnd, foreign_window_prop, (HANDLE)xwin );
    wine_tsx11_lock();
    XSaveContext( display, xwin, winContext, (char *)data->hwnd );
    wine_tsx11_unlock();

    ShowWindow( hwnd, SW_SHOW );

    TRACE( "win %lx parent %p style %08x %s -> hwnd %p\n",
           xwin, parent, style, wine_dbgstr_rect(&data->window_rect), hwnd );

    return hwnd;
}


/***********************************************************************
 *		X11DRV_get_whole_window
 *
 * Return the X window associated with the full area of a window
 */
Window X11DRV_get_whole_window( HWND hwnd )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );

    if (!data)
    {
        if (hwnd == GetDesktopWindow()) return root_window;
        return (Window)GetPropA( hwnd, whole_window_prop );
    }
    return data->whole_window;
}


/***********************************************************************
 *		X11DRV_get_client_window
 *
 * Return the X window associated with the client area of a window
 */
static Window X11DRV_get_client_window( HWND hwnd )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );

    if (!data)
    {
        if (hwnd == GetDesktopWindow()) return root_window;
        return (Window)GetPropA( hwnd, client_window_prop );
    }
    return data->client_window;
}


/***********************************************************************
 *		X11DRV_get_ic
 *
 * Return the X input context associated with a window
 */
XIC X11DRV_get_ic( HWND hwnd )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );
    XIM xim;

    if (!data) return 0;

    x11drv_thread_data()->last_xic_hwnd = hwnd;
    if (data->xic) return data->xic;
    if (!(xim = x11drv_thread_data()->xim)) return 0;
    return X11DRV_CreateIC( xim, data );
}


/***********************************************************************
 *		X11DRV_GetDC   (X11DRV.@)
 */
void CDECL X11DRV_GetDC( HDC hdc, HWND hwnd, HWND top, const RECT *win_rect,
                         const RECT *top_rect, DWORD flags )
{
    struct x11drv_escape_set_drawable escape;
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );
    HWND parent;

    escape.code        = X11DRV_SET_DRAWABLE;
    escape.mode        = IncludeInferiors;
    escape.fbconfig_id = 0;
    escape.gl_drawable = 0;
    escape.pixmap      = 0;
    escape.gl_copy     = FALSE;

    escape.dc_rect.left         = win_rect->left - top_rect->left;
    escape.dc_rect.top          = win_rect->top - top_rect->top;
    escape.dc_rect.right        = win_rect->right - top_rect->left;
    escape.dc_rect.bottom       = win_rect->bottom - top_rect->top;
    escape.drawable_rect        = *top_rect;

    if (top == hwnd)
    {
        escape.fbconfig_id = data ? data->fbconfig_id : (XID)GetPropA( hwnd, fbconfig_id_prop );
        /* GL draws to the client area even for window DCs */
        escape.gl_drawable = data ? data->client_window : X11DRV_get_client_window( hwnd );
        if (data && IsIconic( hwnd ) && data->icon_window)
        {
            escape.drawable = data->icon_window;
        }
        else if (flags & DCX_WINDOW)
            escape.drawable = data ? data->whole_window : X11DRV_get_whole_window( hwnd );
        else
            escape.drawable = escape.gl_drawable;

        /* special case: when repainting the root window, clip out top-level windows */
        if (data && data->whole_window == root_window) escape.mode = ClipByChildren;
    }
    else
    {
        /* find the first ancestor that has a drawable */
        for (parent = hwnd; parent && parent != top; parent = GetAncestor( parent, GA_PARENT ))
            if ((escape.drawable = X11DRV_get_client_window( parent ))) break;

        if (escape.drawable)
        {
            POINT pt = { 0, 0 };
            MapWindowPoints( top, parent, &pt, 1 );
            OffsetRect( &escape.dc_rect, pt.x, pt.y );
            OffsetRect( &escape.drawable_rect, -pt.x, -pt.y );
        }
        else escape.drawable = X11DRV_get_client_window( top );

        escape.fbconfig_id = data ? data->fbconfig_id : (XID)GetPropA( hwnd, fbconfig_id_prop );
        escape.gl_drawable = data ? data->gl_drawable : (Drawable)GetPropA( hwnd, gl_drawable_prop );
        escape.pixmap      = data ? data->pixmap : (Pixmap)GetPropA( hwnd, pixmap_prop );
        escape.gl_copy     = (escape.gl_drawable != 0);
        if (flags & DCX_CLIPCHILDREN) escape.mode = ClipByChildren;
    }

    ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}


/***********************************************************************
 *		X11DRV_ReleaseDC  (X11DRV.@)
 */
void CDECL X11DRV_ReleaseDC( HWND hwnd, HDC hdc )
{
    struct x11drv_escape_set_drawable escape;

    escape.code = X11DRV_SET_DRAWABLE;
    escape.drawable = root_window;
    escape.mode = IncludeInferiors;
    escape.drawable_rect = virtual_screen_rect;
    SetRect( &escape.dc_rect, 0, 0, virtual_screen_rect.right - virtual_screen_rect.left,
             virtual_screen_rect.bottom - virtual_screen_rect.top );
    OffsetRect( &escape.dc_rect, -escape.drawable_rect.left, -escape.drawable_rect.top );
    escape.fbconfig_id = 0;
    escape.gl_drawable = 0;
    escape.pixmap = 0;
    ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}


/***********************************************************************
 *		SetCapture  (X11DRV.@)
 */
void CDECL X11DRV_SetCapture( HWND hwnd, UINT flags )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();

    if (!thread_data) return;
    if (!(flags & (GUI_INMOVESIZE | GUI_INMENUMODE))) return;

    if (hwnd)
    {
        Window grab_win = X11DRV_get_client_window( GetAncestor( hwnd, GA_ROOT ) );

        if (!grab_win) return;
        wine_tsx11_lock();
        XFlush( gdi_display );
        XGrabPointer( thread_data->display, grab_win, False,
                      PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                      GrabModeAsync, GrabModeAsync, None, None, CurrentTime );
        wine_tsx11_unlock();
        thread_data->grab_window = grab_win;
    }
    else  /* release capture */
    {
        wine_tsx11_lock();
        XFlush( gdi_display );
        XUngrabPointer( thread_data->display, CurrentTime );
        XFlush( thread_data->display );
        wine_tsx11_unlock();
        thread_data->grab_window = None;
    }
}


/*****************************************************************
 *		SetParent   (X11DRV.@)
 */
void CDECL X11DRV_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    Display *display = thread_display();
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );

    if (!data) return;
    if (parent == old_parent) return;
    if (data->embedded) return;

    if (parent != GetDesktopWindow()) /* a child window */
    {
        if (old_parent == GetDesktopWindow())
        {
            /* destroy the old X windows */
            destroy_whole_window( display, data, FALSE );
            destroy_icon_window( display, data );
            if (data->managed)
            {
                data->managed = FALSE;
                RemovePropA( data->hwnd, managed_prop );
            }
        }
    }
    else  /* new top level window */
    {
        /* FIXME: we ignore errors since we can't really recover anyway */
        create_whole_window( display, data );
    }
}


/*****************************************************************
 *		SetFocus   (X11DRV.@)
 *
 * Set the X focus.
 */
void CDECL X11DRV_SetFocus( HWND hwnd )
{
    Display *display = thread_display();
    struct x11drv_win_data *data;
    XWindowChanges changes;
    DWORD timestamp;

    if (!(hwnd = GetAncestor( hwnd, GA_ROOT ))) return;
    if (!(data = X11DRV_get_win_data( hwnd ))) return;
    if (data->managed || !data->whole_window) return;

    if (EVENT_x11_time_to_win32_time(0))
        /* ICCCM says don't use CurrentTime, so try to use last message time if possible */
        /* FIXME: this is not entirely correct */
        timestamp = GetMessageTime() - EVENT_x11_time_to_win32_time(0);
    else
        timestamp = CurrentTime;

    /* Set X focus and install colormap */
    wine_tsx11_lock();
    changes.stack_mode = Above;
    XConfigureWindow( display, data->whole_window, CWStackMode, &changes );
    XSetInputFocus( display, data->whole_window, RevertToParent, timestamp );
    wine_tsx11_unlock();
}


/***********************************************************************
 *		WindowPosChanging   (X11DRV.@)
 */
void CDECL X11DRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect, RECT *visible_rect )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );
    DWORD style = GetWindowLongW( hwnd, GWL_STYLE );

    if (!data)
    {
        /* create the win data if the window is being made visible */
        if (!(style & WS_VISIBLE) && !(swp_flags & SWP_SHOWWINDOW)) return;
        if (!(data = X11DRV_create_win_data( hwnd ))) return;
    }

    /* check if we need to switch the window to managed */
    if (!data->managed && data->whole_window && is_window_managed( hwnd, swp_flags, window_rect ))
    {
        TRACE( "making win %p/%lx managed\n", hwnd, data->whole_window );
        if (data->mapped) unmap_window( thread_display(), data );
        data->managed = TRUE;
        SetPropA( hwnd, managed_prop, (HANDLE)1 );
    }

    *visible_rect = *window_rect;
    X11DRV_window_to_X_rect( data, visible_rect );
}


/***********************************************************************
 *		WindowPosChanged   (X11DRV.@)
 */
void CDECL X11DRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *rectWindow, const RECT *rectClient,
                                    const RECT *visible_rect, const RECT *valid_rects )
{
    struct x11drv_thread_data *thread_data;
    Display *display;
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );
    DWORD new_style = GetWindowLongW( hwnd, GWL_STYLE );
    RECT old_window_rect, old_whole_rect, old_client_rect;
    int event_type;

    if (!data) return;

    thread_data = x11drv_thread_data();
    display = thread_data->display;

    old_window_rect = data->window_rect;
    old_whole_rect  = data->whole_rect;
    old_client_rect = data->client_rect;
    data->window_rect = *rectWindow;
    data->whole_rect  = *visible_rect;
    data->client_rect = *rectClient;

    TRACE( "win %p window %s client %s style %08x flags %08x\n",
           hwnd, wine_dbgstr_rect(rectWindow), wine_dbgstr_rect(rectClient), new_style, swp_flags );

    if (!IsRectEmpty( &valid_rects[0] ))
    {
        int x_offset = old_whole_rect.left - data->whole_rect.left;
        int y_offset = old_whole_rect.top - data->whole_rect.top;

        /* if all that happened is that the whole window moved, copy everything */
        if (!(swp_flags & SWP_FRAMECHANGED) &&
            old_whole_rect.right   - data->whole_rect.right   == x_offset &&
            old_whole_rect.bottom  - data->whole_rect.bottom  == y_offset &&
            old_client_rect.left   - data->client_rect.left   == x_offset &&
            old_client_rect.right  - data->client_rect.right  == x_offset &&
            old_client_rect.top    - data->client_rect.top    == y_offset &&
            old_client_rect.bottom - data->client_rect.bottom == y_offset &&
            !memcmp( &valid_rects[0], &data->client_rect, sizeof(RECT) ))
        {
            /* if we have an X window the bits will be moved by the X server */
            if (!data->whole_window && (x_offset != 0 || y_offset != 0))
                move_window_bits( data, &old_whole_rect, &data->whole_rect, &old_client_rect );
        }
        else
            move_window_bits( data, &valid_rects[1], &valid_rects[0], &old_client_rect );
    }

    wine_tsx11_lock();
    XFlush( gdi_display );  /* make sure painting is done before we move the window */
    wine_tsx11_unlock();

    sync_client_position( display, data, swp_flags, &old_client_rect, &old_whole_rect );

    if (!data->whole_window) return;

    /* check if we are currently processing an event relevant to this window */
    event_type = 0;
    if (thread_data->current_event && thread_data->current_event->xany.window == data->whole_window)
        event_type = thread_data->current_event->type;

    if (event_type != ConfigureNotify && event_type != PropertyNotify &&
        event_type != GravityNotify && event_type != ReparentNotify)
        event_type = 0;  /* ignore other events */

    if (data->mapped && event_type != ReparentNotify)
    {
        if (((swp_flags & SWP_HIDEWINDOW) && !(new_style & WS_VISIBLE)) ||
            (!event_type &&
             !is_window_rect_mapped( rectWindow ) && is_window_rect_mapped( &old_window_rect )))
        {
            unmap_window( display, data );
            if (is_window_rect_fullscreen( &old_window_rect )) reset_clipping_window();
        }
    }

    /* don't change position if we are about to minimize or maximize a managed window */
    if (!event_type &&
        !(data->managed && (swp_flags & SWP_STATECHANGED) && (new_style & (WS_MINIMIZE|WS_MAXIMIZE))))
        sync_window_position( display, data, swp_flags,
                              &old_window_rect, &old_whole_rect, &old_client_rect );

    if ((new_style & WS_VISIBLE) &&
        ((new_style & WS_MINIMIZE) || is_window_rect_mapped( rectWindow )))
    {
        if (!data->mapped || (swp_flags & (SWP_FRAMECHANGED|SWP_STATECHANGED)))
            set_wm_hints( display, data );

        if (!data->mapped)
        {
            map_window( display, data, new_style );
        }
        else if ((swp_flags & SWP_STATECHANGED) && (!data->iconic != !(new_style & WS_MINIMIZE)))
        {
            data->iconic = (new_style & WS_MINIMIZE) != 0;
            TRACE( "changing win %p iconic state to %u\n", data->hwnd, data->iconic );
            wine_tsx11_lock();
            if (data->iconic)
                XIconifyWindow( display, data->whole_window, DefaultScreen(display) );
            else if (is_window_rect_mapped( rectWindow ))
                XMapWindow( display, data->whole_window );
            wine_tsx11_unlock();
            update_net_wm_states( display, data );
        }
        else if (!event_type)
        {
            update_net_wm_states( display, data );
        }
    }

    wine_tsx11_lock();
    XFlush( display );  /* make sure changes are done before we start painting again */
    wine_tsx11_unlock();
}


/***********************************************************************
 *           ShowWindow   (X11DRV.@)
 */
UINT CDECL X11DRV_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
    int x, y;
    unsigned int width, height, border, depth;
    Window root, top;
    DWORD style = GetWindowLongW( hwnd, GWL_STYLE );
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );

    if (!data || !data->whole_window || !data->managed || !data->mapped || data->iconic) return swp;
    if (style & WS_MINIMIZE) return swp;
    if (IsRectEmpty( rect )) return swp;

    /* only fetch the new rectangle if the ShowWindow was a result of a window manager event */

    if (!thread_data->current_event || thread_data->current_event->xany.window != data->whole_window)
        return swp;

    if (thread_data->current_event->type != ConfigureNotify &&
        thread_data->current_event->type != PropertyNotify)
        return swp;

    TRACE( "win %p/%lx cmd %d at %s flags %08x\n",
           hwnd, data->whole_window, cmd, wine_dbgstr_rect(rect), swp );

    wine_tsx11_lock();
    XGetGeometry( thread_data->display, data->whole_window,
                  &root, &x, &y, &width, &height, &border, &depth );
    XTranslateCoordinates( thread_data->display, data->whole_window, root, 0, 0, &x, &y, &top );
    wine_tsx11_unlock();
    rect->left   = x;
    rect->top    = y;
    rect->right  = x + width;
    rect->bottom = y + height;
    OffsetRect( rect, virtual_screen_rect.left, virtual_screen_rect.top );
    X11DRV_X_to_window_rect( data, rect );
    return swp & ~(SWP_NOMOVE | SWP_NOCLIENTMOVE | SWP_NOSIZE | SWP_NOCLIENTSIZE);
}


/**********************************************************************
 *		SetWindowIcon (X11DRV.@)
 *
 * hIcon or hIconSm has changed (or is being initialised for the
 * first time). Complete the X11 driver-specific initialisation
 * and set the window hints.
 *
 * This is not entirely correct, may need to create
 * an icon window and set the pixmap as a background
 */
void CDECL X11DRV_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
    Display *display = thread_display();
    struct x11drv_win_data *data;


    if (!(data = X11DRV_get_win_data( hwnd ))) return;
    if (!data->whole_window) return;
    if (!data->managed) return;

    if (data->wm_hints)
    {
        if (type == ICON_BIG) set_icon_hints( display, data, icon, 0 );
        else set_icon_hints( display, data, 0, icon );
        wine_tsx11_lock();
        XSetWMHints( display, data->whole_window, data->wm_hints );
        wine_tsx11_unlock();
    }
}


/***********************************************************************
 *		SetWindowRgn  (X11DRV.@)
 *
 * Assign specified region to window (for non-rectangular windows)
 */
int CDECL X11DRV_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
    struct x11drv_win_data *data;

    if ((data = X11DRV_get_win_data( hwnd )))
    {
        sync_window_region( thread_display(), data, hrgn );
    }
    else if (X11DRV_get_whole_window( hwnd ))
    {
        SendMessageW( hwnd, WM_X11DRV_SET_WIN_REGION, 0, 0 );
    }
    return TRUE;
}


/***********************************************************************
 *		SetLayeredWindowAttributes  (X11DRV.@)
 *
 * Set transparency attributes for a layered window.
 */
void CDECL X11DRV_SetLayeredWindowAttributes( HWND hwnd, COLORREF key, BYTE alpha, DWORD flags )
{
    struct x11drv_win_data *data = X11DRV_get_win_data( hwnd );

    if (data)
    {
        if (data->whole_window)
            sync_window_opacity( thread_display(), data->whole_window, key, alpha, flags );
    }
    else
    {
        Window win = X11DRV_get_whole_window( hwnd );
        if (win) sync_window_opacity( gdi_display, win, key, alpha, flags );
    }
}


/**********************************************************************
 *           X11DRV_WindowMessage   (X11DRV.@)
 */
LRESULT CDECL X11DRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    struct x11drv_win_data *data;

    switch(msg)
    {
    case WM_X11DRV_ACQUIRE_SELECTION:
        return X11DRV_AcquireClipboard( hwnd );
    case WM_X11DRV_SET_WIN_FORMAT:
        return set_win_format( hwnd, (XID)wp );
    case WM_X11DRV_SET_WIN_REGION:
        if ((data = X11DRV_get_win_data( hwnd ))) sync_window_region( thread_display(), data, (HRGN)1 );
        return 0;
    case WM_X11DRV_RESIZE_DESKTOP:
        X11DRV_resize_desktop( LOWORD(lp), HIWORD(lp) );
        return 0;
    case WM_X11DRV_SET_CURSOR:
        if ((data = X11DRV_get_win_data( hwnd )) && data->whole_window)
            set_window_cursor( data->whole_window, (HCURSOR)lp );
        else if (hwnd == x11drv_thread_data()->clip_hwnd)
            set_window_cursor( x11drv_thread_data()->clip_window, (HCURSOR)lp );
        return 0;
    case WM_X11DRV_CLIP_CURSOR:
        return clip_cursor_notify( hwnd, (HWND)lp );
    default:
        FIXME( "got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, wp, lp );
        return 0;
    }
}


/***********************************************************************
 *              is_netwm_supported
 */
static BOOL is_netwm_supported( Display *display, Atom atom )
{
    static Atom *net_supported;
    static int net_supported_count = -1;
    int i;

    wine_tsx11_lock();
    if (net_supported_count == -1)
    {
        Atom type;
        int format;
        unsigned long count, remaining;

        if (!XGetWindowProperty( display, DefaultRootWindow(display), x11drv_atom(_NET_SUPPORTED), 0,
                                 ~0UL, False, XA_ATOM, &type, &format, &count,
                                 &remaining, (unsigned char **)&net_supported ))
            net_supported_count = get_property_size( format, count ) / sizeof(Atom);
        else
            net_supported_count = 0;
    }
    wine_tsx11_unlock();

    for (i = 0; i < net_supported_count; i++)
        if (net_supported[i] == atom) return TRUE;
    return FALSE;
}


/***********************************************************************
 *           SysCommand   (X11DRV.@)
 *
 * Perform WM_SYSCOMMAND handling.
 */
LRESULT CDECL X11DRV_SysCommand( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    WPARAM hittest = wparam & 0x0f;
    int dir;
    Display *display = thread_display();
    struct x11drv_win_data *data;

    if (!(data = X11DRV_get_win_data( hwnd ))) return -1;
    if (!data->whole_window || !data->managed || !data->mapped) return -1;

    switch (wparam & 0xfff0)
    {
    case SC_MOVE:
        if (!hittest) dir = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
        else dir = _NET_WM_MOVERESIZE_MOVE;
        break;
    case SC_SIZE:
        /* windows without WS_THICKFRAME are not resizable through the window manager */
        if (!(GetWindowLongW( hwnd, GWL_STYLE ) & WS_THICKFRAME)) return -1;

        switch (hittest)
        {
        case WMSZ_LEFT:        dir = _NET_WM_MOVERESIZE_SIZE_LEFT; break;
        case WMSZ_RIGHT:       dir = _NET_WM_MOVERESIZE_SIZE_RIGHT; break;
        case WMSZ_TOP:         dir = _NET_WM_MOVERESIZE_SIZE_TOP; break;
        case WMSZ_TOPLEFT:     dir = _NET_WM_MOVERESIZE_SIZE_TOPLEFT; break;
        case WMSZ_TOPRIGHT:    dir = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT; break;
        case WMSZ_BOTTOM:      dir = _NET_WM_MOVERESIZE_SIZE_BOTTOM; break;
        case WMSZ_BOTTOMLEFT:  dir = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT; break;
        case WMSZ_BOTTOMRIGHT: dir = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT; break;
        default:               dir = _NET_WM_MOVERESIZE_SIZE_KEYBOARD; break;
        }
        break;

    case SC_KEYMENU:
        /* prevent a simple ALT press+release from activating the system menu,
         * as that can get confusing on managed windows */
        if ((WCHAR)lparam) return -1;  /* got an explicit char */
        if (GetMenu( hwnd )) return -1;  /* window has a real menu */
        if (!(GetWindowLongW( hwnd, GWL_STYLE ) & WS_SYSMENU)) return -1;  /* no system menu */
        TRACE( "ignoring SC_KEYMENU wp %lx lp %lx\n", wparam, lparam );
        return 0;

    default:
        return -1;
    }

    if (IsZoomed(hwnd)) return -1;

    if (!is_netwm_supported( display, x11drv_atom(_NET_WM_MOVERESIZE) ))
    {
        TRACE( "_NET_WM_MOVERESIZE not supported\n" );
        return -1;
    }

    move_resize_window( display, data, dir );
    return 0;
}
