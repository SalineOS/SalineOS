/*
 * Copyright 2005-2009 Jacek Caban for CodeWeavers
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

#include "wingdi.h"
#include "docobj.h"
#include "docobjectservice.h"
#include "comcat.h"
#include "mshtml.h"
#include "mshtmhst.h"
#include "hlink.h"
#include "perhist.h"
#include "dispex.h"
#include "objsafe.h"
#include "htiframe.h"
#include "tlogstg.h"

#include "wine/list.h"
#include "wine/unicode.h"

#ifdef INIT_GUID
#include "initguid.h"
#endif

#include "nsiface.h"

#define NS_ERROR_GENERATE_FAILURE(module,code) \
    ((nsresult) (((PRUint32)(1<<31)) | ((PRUint32)(module+0x45)<<16) | ((PRUint32)(code))))

#define NS_OK                     ((nsresult)0x00000000L)
#define NS_ERROR_FAILURE          ((nsresult)0x80004005L)
#define NS_ERROR_OUT_OF_MEMORY    ((nsresult)0x8007000EL)
#define NS_ERROR_NOT_IMPLEMENTED  ((nsresult)0x80004001L)
#define NS_NOINTERFACE            ((nsresult)0x80004002L)
#define NS_ERROR_INVALID_POINTER  ((nsresult)0x80004003L)
#define NS_ERROR_NULL_POINTER     NS_ERROR_INVALID_POINTER
#define NS_ERROR_NOT_AVAILABLE    ((nsresult)0x80040111L)
#define NS_ERROR_INVALID_ARG      ((nsresult)0x80070057L) 
#define NS_ERROR_UNEXPECTED       ((nsresult)0x8000ffffL)

#define NS_ERROR_MODULE_NETWORK    6

#define NS_BINDING_ABORTED         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 2)
#define NS_ERROR_UNKNOWN_PROTOCOL  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 18)

#define NS_FAILED(res) ((res) & 0x80000000)
#define NS_SUCCEEDED(res) (!NS_FAILED(res))

#define NSAPI WINAPI

#define MSHTML_E_NODOC    0x800a025c

typedef struct HTMLDOMNode HTMLDOMNode;
typedef struct ConnectionPoint ConnectionPoint;
typedef struct BSCallback BSCallback;
typedef struct event_target_t event_target_t;

#define TID_LIST \
    XIID(NULL) \
    XDIID(DispCEventObj) \
    XDIID(DispCPlugins) \
    XDIID(DispDOMChildrenCollection) \
    XDIID(DispHTMLAnchorElement) \
    XDIID(DispHTMLAttributeCollection) \
    XDIID(DispHTMLBody) \
    XDIID(DispHTMLCommentElement) \
    XDIID(DispHTMLCurrentStyle) \
    XDIID(DispHTMLDocument) \
    XDIID(DispHTMLDOMAttribute) \
    XDIID(DispHTMLDOMTextNode) \
    XDIID(DispHTMLElementCollection) \
    XDIID(DispHTMLEmbed) \
    XDIID(DispHTMLFormElement) \
    XDIID(DispHTMLGenericElement) \
    XDIID(DispHTMLFrameElement) \
    XDIID(DispHTMLHeadElement) \
    XDIID(DispHTMLIFrame) \
    XDIID(DispHTMLImg) \
    XDIID(DispHTMLInputElement) \
    XDIID(DispHTMLLocation) \
    XDIID(DispHTMLNavigator) \
    XDIID(DispHTMLObjectElement) \
    XDIID(DispHTMLOptionElement) \
    XDIID(DispHTMLScreen) \
    XDIID(DispHTMLScriptElement) \
    XDIID(DispHTMLSelectElement) \
    XDIID(DispHTMLStyle) \
    XDIID(DispHTMLStyleElement) \
    XDIID(DispHTMLStyleSheetsCollection) \
    XDIID(DispHTMLTable) \
    XDIID(DispHTMLTableRow) \
    XDIID(DispHTMLTextAreaElement) \
    XDIID(DispHTMLTitleElement) \
    XDIID(DispHTMLUnknownElement) \
    XDIID(DispHTMLWindow2) \
    XDIID(HTMLDocumentEvents) \
    XIID(IHTMLAnchorElement) \
    XIID(IHTMLAttributeCollection) \
    XIID(IHTMLAttributeCollection2) \
    XIID(IHTMLAttributeCollection3) \
    XIID(IHTMLBodyElement) \
    XIID(IHTMLBodyElement2) \
    XIID(IHTMLCommentElement) \
    XIID(IHTMLCurrentStyle) \
    XIID(IHTMLCurrentStyle2) \
    XIID(IHTMLCurrentStyle3) \
    XIID(IHTMLCurrentStyle4) \
    XIID(IHTMLDocument2) \
    XIID(IHTMLDocument3) \
    XIID(IHTMLDocument4) \
    XIID(IHTMLDocument5) \
    XIID(IHTMLDOMAttribute) \
    XIID(IHTMLDOMChildrenCollection) \
    XIID(IHTMLDOMNode) \
    XIID(IHTMLDOMNode2) \
    XIID(IHTMLDOMTextNode) \
    XIID(IHTMLElement) \
    XIID(IHTMLElement2) \
    XIID(IHTMLElement3) \
    XIID(IHTMLElement4) \
    XIID(IHTMLElementCollection) \
    XIID(IHTMLEmbedElement) \
    XIID(IHTMLEventObj) \
    XIID(IHTMLFiltersCollection) \
    XIID(IHTMLFormElement) \
    XIID(IHTMLFrameBase) \
    XIID(IHTMLFrameBase2) \
    XIID(IHTMLFrameElement3) \
    XIID(IHTMLGenericElement) \
    XIID(IHTMLHeadElement) \
    XIID(IHTMLIFrameElement) \
    XIID(IHTMLImageElementFactory) \
    XIID(IHTMLImgElement) \
    XIID(IHTMLInputElement) \
    XIID(IHTMLLocation) \
    XIID(IHTMLMimeTypesCollection) \
    XIID(IHTMLObjectElement) \
    XIID(IHTMLOptionElement) \
    XIID(IHTMLPluginsCollection) \
    XIID(IHTMLRect) \
    XIID(IHTMLScreen) \
    XIID(IHTMLScriptElement) \
    XIID(IHTMLSelectElement) \
    XIID(IHTMLStyle) \
    XIID(IHTMLStyle2) \
    XIID(IHTMLStyle3) \
    XIID(IHTMLStyle4) \
    XIID(IHTMLStyle5) \
    XIID(IHTMLStyle6) \
    XIID(IHTMLStyleElement) \
    XIID(IHTMLStyleSheetsCollection) \
    XIID(IHTMLTable) \
    XIID(IHTMLTable2) \
    XIID(IHTMLTable3) \
    XIID(IHTMLTableRow) \
    XIID(IHTMLTextAreaElement) \
    XIID(IHTMLTextContainer) \
    XIID(IHTMLTitleElement) \
    XIID(IHTMLUniqueName) \
    XIID(IHTMLWindow2) \
    XIID(IHTMLWindow3) \
    XIID(IHTMLWindow4) \
    XIID(IHTMLWindow6) \
    XIID(IOmNavigator)

typedef enum {
#define XIID(iface) iface ## _tid,
#define XDIID(iface) iface ## _tid,
TID_LIST
#undef XIID
#undef XDIID
    LAST_tid
} tid_t;

typedef struct dispex_data_t dispex_data_t;
typedef struct dispex_dynamic_data_t dispex_dynamic_data_t;

#define MSHTML_DISPID_CUSTOM_MIN 0x60000000
#define MSHTML_DISPID_CUSTOM_MAX 0x6fffffff
#define MSHTML_CUSTOM_DISPID_CNT (MSHTML_DISPID_CUSTOM_MAX-MSHTML_DISPID_CUSTOM_MIN)

typedef struct DispatchEx DispatchEx;

typedef struct {
    HRESULT (*value)(DispatchEx*,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*get_dispid)(DispatchEx*,BSTR,DWORD,DISPID*);
    HRESULT (*invoke)(DispatchEx*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*populate_props)(DispatchEx*);
} dispex_static_data_vtbl_t;

typedef struct {
    const dispex_static_data_vtbl_t *vtbl;
    const tid_t disp_tid;
    dispex_data_t *data;
    const tid_t* const iface_tids;
} dispex_static_data_t;

struct DispatchEx {
    IDispatchEx IDispatchEx_iface;

    IUnknown *outer;

    dispex_static_data_t *data;
    dispex_dynamic_data_t *dynamic_data;
};

void init_dispex(DispatchEx*,IUnknown*,dispex_static_data_t*) DECLSPEC_HIDDEN;
void release_dispex(DispatchEx*) DECLSPEC_HIDDEN;
BOOL dispex_query_interface(DispatchEx*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT dispex_get_dprop_ref(DispatchEx*,const WCHAR*,BOOL,VARIANT**) DECLSPEC_HIDDEN;
HRESULT get_dispids(tid_t,DWORD*,DISPID**) DECLSPEC_HIDDEN;
HRESULT remove_prop(DispatchEx*,BSTR,VARIANT_BOOL*) DECLSPEC_HIDDEN;
void release_typelib(void) DECLSPEC_HIDDEN;
HRESULT get_htmldoc_classinfo(ITypeInfo **typeinfo) DECLSPEC_HIDDEN;

typedef struct HTMLWindow HTMLWindow;
typedef struct HTMLDocumentNode HTMLDocumentNode;
typedef struct HTMLDocumentObj HTMLDocumentObj;
typedef struct HTMLFrameBase HTMLFrameBase;
typedef struct NSContainer NSContainer;
typedef struct HTMLAttributeCollection HTMLAttributeCollection;

typedef enum {
    SCRIPTMODE_GECKO,
    SCRIPTMODE_ACTIVESCRIPT
} SCRIPTMODE;

typedef struct ScriptHost ScriptHost;

typedef enum {
    GLOBAL_SCRIPTVAR,
    GLOBAL_ELEMENTVAR
} global_prop_type_t;

typedef struct {
    global_prop_type_t type;
    WCHAR *name;
    ScriptHost *script_host;
    DISPID id;
} global_prop_t;

typedef struct {
    IHTMLOptionElementFactory IHTMLOptionElementFactory_iface;

    LONG ref;

    HTMLWindow *window;
} HTMLOptionElementFactory;

typedef struct {
    DispatchEx dispex;
    IHTMLImageElementFactory IHTMLImageElementFactory_iface;

    LONG ref;

    HTMLWindow *window;
} HTMLImageElementFactory;

struct HTMLLocation {
    DispatchEx dispex;
    IHTMLLocation IHTMLLocation_iface;

    LONG ref;

    HTMLWindow *window;
};

typedef struct {
    HTMLWindow *window;
    LONG ref;
}  windowref_t;

typedef struct nsChannelBSC nsChannelBSC;

struct HTMLWindow {
    DispatchEx dispex;
    IHTMLWindow2       IHTMLWindow2_iface;
    IHTMLWindow3       IHTMLWindow3_iface;
    IHTMLWindow4       IHTMLWindow4_iface;
    IHTMLWindow5       IHTMLWindow5_iface;
    IHTMLWindow6       IHTMLWindow6_iface;
    IHTMLPrivateWindow IHTMLPrivateWindow_iface;
    IDispatchEx        IDispatchEx_iface;
    IServiceProvider   IServiceProvider_iface;
    ITravelLogClient   ITravelLogClient_iface;

    LONG ref;

    windowref_t *window_ref;
    LONG task_magic;

    HTMLDocumentNode *doc;
    HTMLDocumentObj *doc_obj;
    nsIDOMWindow *nswindow;
    HTMLWindow *parent;
    HTMLFrameBase *frame_element;
    READYSTATE readystate;

    nsChannelBSC *bscallback;
    IMoniker *mon;
    IUri *uri;
    BSTR url;

    IHTMLEventObj *event;

    SCRIPTMODE scriptmode;
    struct list script_hosts;

    IInternetSecurityManager *secmgr;

    HTMLOptionElementFactory *option_factory;
    HTMLImageElementFactory *image_factory;
    HTMLLocation *location;
    IHTMLScreen *screen;

    global_prop_t *global_props;
    DWORD global_prop_cnt;
    DWORD global_prop_size;

    struct list children;
    struct list sibling_entry;
    struct list entry;
};

typedef enum {
    UNKNOWN_USERMODE,
    BROWSEMODE,
    EDITMODE        
} USERMODE;

typedef struct _cp_static_data_t {
    tid_t tid;
    void (*on_advise)(IUnknown*,struct _cp_static_data_t*);
    DWORD id_cnt;
    DISPID *ids;
} cp_static_data_t;

typedef struct ConnectionPointContainer {
    IConnectionPointContainer IConnectionPointContainer_iface;

    ConnectionPoint *cp_list;
    IUnknown *outer;
    struct ConnectionPointContainer *forward_container;
} ConnectionPointContainer;

struct ConnectionPoint {
    IConnectionPoint IConnectionPoint_iface;

    ConnectionPointContainer *container;

    union {
        IUnknown *unk;
        IDispatch *disp;
        IPropertyNotifySink *propnotif;
    } *sinks;
    DWORD sinks_size;

    const IID *iid;
    cp_static_data_t *data;

    ConnectionPoint *next;
};

struct HTMLDocument {
    IHTMLDocument2              IHTMLDocument2_iface;
    IHTMLDocument3              IHTMLDocument3_iface;
    IHTMLDocument4              IHTMLDocument4_iface;
    IHTMLDocument5              IHTMLDocument5_iface;
    IHTMLDocument6              IHTMLDocument6_iface;
    IPersistMoniker             IPersistMoniker_iface;
    IPersistFile                IPersistFile_iface;
    IPersistHistory             IPersistHistory_iface;
    IMonikerProp                IMonikerProp_iface;
    IOleObject                  IOleObject_iface;
    IOleDocument                IOleDocument_iface;
    IOleDocumentView            IOleDocumentView_iface;
    IOleInPlaceActiveObject     IOleInPlaceActiveObject_iface;
    IViewObjectEx               IViewObjectEx_iface;
    IOleInPlaceObjectWindowless IOleInPlaceObjectWindowless_iface;
    IServiceProvider            IServiceProvider_iface;
    IOleCommandTarget           IOleCommandTarget_iface;
    IOleControl                 IOleControl_iface;
    IHlinkTarget                IHlinkTarget_iface;
    IPersistStreamInit          IPersistStreamInit_iface;
    IDispatchEx                 IDispatchEx_iface;
    ISupportErrorInfo           ISupportErrorInfo_iface;
    IObjectWithSite             IObjectWithSite_iface;
    IOleContainer               IOleContainer_iface;
    IObjectSafety               IObjectSafety_iface;
    IProvideClassInfo           IProvideClassInfo_iface;

    IUnknown *unk_impl;
    IDispatchEx *dispex;

    HTMLDocumentObj *doc_obj;
    HTMLDocumentNode *doc_node;

    HTMLWindow *window;

    LONG task_magic;

    ConnectionPointContainer cp_container;
    ConnectionPoint cp_htmldocevents;
    ConnectionPoint cp_htmldocevents2;
    ConnectionPoint cp_propnotif;
    ConnectionPoint cp_dispatch;

    IOleAdviseHolder *advise_holder;
};

static inline HRESULT htmldoc_query_interface(HTMLDocument *This, REFIID riid, void **ppv)
{
    return IUnknown_QueryInterface(This->unk_impl, riid, ppv);
}

static inline ULONG htmldoc_addref(HTMLDocument *This)
{
    return IUnknown_AddRef(This->unk_impl);
}

static inline ULONG htmldoc_release(HTMLDocument *This)
{
    return IUnknown_Release(This->unk_impl);
}

struct HTMLDocumentObj {
    HTMLDocument basedoc;
    DispatchEx dispex;
    ICustomDoc ICustomDoc_iface;
    ITargetContainer ITargetContainer_iface;

    LONG ref;

    NSContainer *nscontainer;

    IOleClientSite *client;
    IDocHostUIHandler *hostui;
    BOOL custom_hostui;
    IOleInPlaceSite *ipsite;
    IOleInPlaceFrame *frame;
    IOleInPlaceUIWindow *ip_window;
    IAdviseSink *view_sink;
    IDocObjectService *doc_object_service;

    DOCHOSTUIINFO hostinfo;

    IOleUndoManager *undomgr;

    HWND hwnd;
    HWND tooltips_hwnd;

    BOOL request_uiactivate;
    BOOL in_place_active;
    BOOL ui_active;
    BOOL window_active;
    BOOL hostui_setup;
    BOOL is_webbrowser;
    BOOL container_locked;
    BOOL focus;
    BOOL has_popup;
    INT download_state;

    USERMODE usermode;
    LPWSTR mime;

    DWORD update;
};

typedef struct nsWeakReference nsWeakReference;

struct NSContainer {
    nsIWebBrowserChrome      nsIWebBrowserChrome_iface;
    nsIContextMenuListener   nsIContextMenuListener_iface;
    nsIURIContentListener    nsIURIContentListener_iface;
    nsIEmbeddingSiteWindow   nsIEmbeddingSiteWindow_iface;
    nsITooltipListener       nsITooltipListener_iface;
    nsIInterfaceRequestor    nsIInterfaceRequestor_iface;
    nsISupportsWeakReference nsISupportsWeakReference_iface;

    nsIWebBrowser *webbrowser;
    nsIWebNavigation *navigation;
    nsIBaseWindow *window;
    nsIWebBrowserFocus *focus;

    nsIEditor *editor;
    nsIController *editor_controller;

    LONG ref;

    nsWeakReference *weak_reference;

    NSContainer *parent;
    HTMLDocumentObj *doc;

    nsIURIContentListener *content_listener;

    HWND hwnd;
};

typedef struct {
    HRESULT (*qi)(HTMLDOMNode*,REFIID,void**);
    void (*destructor)(HTMLDOMNode*);
    HRESULT (*clone)(HTMLDOMNode*,nsIDOMNode*,HTMLDOMNode**);
    HRESULT (*get_attr_col)(HTMLDOMNode*,HTMLAttributeCollection**);
    event_target_t **(*get_event_target)(HTMLDOMNode*);
    HRESULT (*fire_event)(HTMLDOMNode*,DWORD,BOOL*);
    HRESULT (*handle_event)(HTMLDOMNode*,DWORD,nsIDOMEvent*,BOOL*);
    HRESULT (*put_disabled)(HTMLDOMNode*,VARIANT_BOOL);
    HRESULT (*get_disabled)(HTMLDOMNode*,VARIANT_BOOL*);
    HRESULT (*get_document)(HTMLDOMNode*,IDispatch**);
    HRESULT (*get_readystate)(HTMLDOMNode*,BSTR*);
    HRESULT (*get_dispid)(HTMLDOMNode*,BSTR,DWORD,DISPID*);
    HRESULT (*invoke)(HTMLDOMNode*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*bind_to_tree)(HTMLDOMNode*);
} NodeImplVtbl;

struct HTMLDOMNode {
    DispatchEx dispex;
    IHTMLDOMNode  IHTMLDOMNode_iface;
    IHTMLDOMNode2 IHTMLDOMNode2_iface;
    const NodeImplVtbl *vtbl;

    LONG ref;

    nsIDOMNode *nsnode;
    HTMLDocumentNode *doc;
    event_target_t *event_target;
    ConnectionPointContainer *cp_container;

    HTMLDOMNode *next;
};

typedef struct {
    HTMLDOMNode node;
    ConnectionPointContainer cp_container;

    IHTMLElement  IHTMLElement_iface;
    IHTMLElement2 IHTMLElement2_iface;
    IHTMLElement3 IHTMLElement3_iface;
    IHTMLElement4 IHTMLElement4_iface;

    nsIDOMHTMLElement *nselem;
    HTMLStyle *style;
    HTMLAttributeCollection *attrs;
} HTMLElement;

#define HTMLELEMENT_TIDS    \
    IHTMLDOMNode_tid,       \
    IHTMLDOMNode2_tid,      \
    IHTMLElement_tid,       \
    IHTMLElement2_tid,      \
    IHTMLElement3_tid,      \
    IHTMLElement4_tid

typedef struct {
    HTMLElement element;

    IHTMLTextContainer IHTMLTextContainer_iface;

    ConnectionPoint cp;
} HTMLTextContainer;

struct HTMLFrameBase {
    HTMLElement element;

    IHTMLFrameBase  IHTMLFrameBase_iface;
    IHTMLFrameBase2 IHTMLFrameBase2_iface;

    HTMLWindow *content_window;

    nsIDOMHTMLFrameElement *nsframe;
    nsIDOMHTMLIFrameElement *nsiframe;
};

typedef struct nsDocumentEventListener nsDocumentEventListener;

struct HTMLDocumentNode {
    HTMLDOMNode node;
    HTMLDocument basedoc;

    IInternetHostSecurityManager IInternetHostSecurityManager_iface;

    nsIDocumentObserver          nsIDocumentObserver_iface;

    LONG ref;

    nsIDOMHTMLDocument *nsdoc;
    HTMLDOMNode *nodes;
    BOOL content_ready;
    event_target_t *body_event_target;

    ICatInformation *catmgr;
    nsDocumentEventListener *nsevent_listener;
    BOOL *event_vector;

    WCHAR **elem_vars;
    unsigned elem_vars_size;
    unsigned elem_vars_cnt;

    BOOL skip_mutation_notif;

    struct list bindings;
    struct list selection_list;
    struct list range_list;
    struct list plugin_hosts;
};

HRESULT HTMLDocument_Create(IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT HTMLLoadOptions_Create(IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT create_doc_from_nsdoc(nsIDOMHTMLDocument*,HTMLDocumentObj*,HTMLWindow*,HTMLDocumentNode**) DECLSPEC_HIDDEN;
HRESULT create_document_fragment(nsIDOMNode*,HTMLDocumentNode*,HTMLDocumentNode**) DECLSPEC_HIDDEN;

HRESULT HTMLWindow_Create(HTMLDocumentObj*,nsIDOMWindow*,HTMLWindow*,HTMLWindow**) DECLSPEC_HIDDEN;
void update_window_doc(HTMLWindow*) DECLSPEC_HIDDEN;
HTMLWindow *nswindow_to_window(const nsIDOMWindow*) DECLSPEC_HIDDEN;
HTMLOptionElementFactory *HTMLOptionElementFactory_Create(HTMLWindow*) DECLSPEC_HIDDEN;
HTMLImageElementFactory *HTMLImageElementFactory_Create(HTMLWindow*) DECLSPEC_HIDDEN;
HRESULT HTMLLocation_Create(HTMLWindow*,HTMLLocation**) DECLSPEC_HIDDEN;
IOmNavigator *OmNavigator_Create(void) DECLSPEC_HIDDEN;
HRESULT HTMLScreen_Create(IHTMLScreen**) DECLSPEC_HIDDEN;

void HTMLDocument_HTMLDocument3_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_HTMLDocument5_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_Persist_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_OleCmd_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_OleObj_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_View_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_Window_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_Service_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_Hlink_Init(HTMLDocument*) DECLSPEC_HIDDEN;

void TargetContainer_Init(HTMLDocumentObj*) DECLSPEC_HIDDEN;

void HTMLDocumentNode_SecMgr_Init(HTMLDocumentNode*) DECLSPEC_HIDDEN;

HRESULT HTMLCurrentStyle_Create(HTMLElement*,IHTMLCurrentStyle**) DECLSPEC_HIDDEN;

void ConnectionPoint_Init(ConnectionPoint*,ConnectionPointContainer*,REFIID,cp_static_data_t*) DECLSPEC_HIDDEN;
void ConnectionPointContainer_Init(ConnectionPointContainer*,IUnknown*) DECLSPEC_HIDDEN;
void ConnectionPointContainer_Destroy(ConnectionPointContainer*) DECLSPEC_HIDDEN;

HRESULT create_nscontainer(HTMLDocumentObj*,NSContainer*,NSContainer**) DECLSPEC_HIDDEN;
void NSContainer_Release(NSContainer*) DECLSPEC_HIDDEN;
nsresult create_chrome_window(nsIWebBrowserChrome*,nsIWebBrowserChrome**) DECLSPEC_HIDDEN;

void init_mutation(nsIComponentManager*) DECLSPEC_HIDDEN;
void init_document_mutation(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void release_document_mutation(HTMLDocumentNode*) DECLSPEC_HIDDEN;

void HTMLDocument_LockContainer(HTMLDocumentObj*,BOOL) DECLSPEC_HIDDEN;
void show_context_menu(HTMLDocumentObj*,DWORD,POINT*,IDispatch*) DECLSPEC_HIDDEN;
void notif_focus(HTMLDocumentObj*) DECLSPEC_HIDDEN;

void show_tooltip(HTMLDocumentObj*,DWORD,DWORD,LPCWSTR) DECLSPEC_HIDDEN;
void hide_tooltip(HTMLDocumentObj*) DECLSPEC_HIDDEN;
HRESULT get_client_disp_property(IOleClientSite*,DISPID,VARIANT*) DECLSPEC_HIDDEN;

HRESULT ProtocolFactory_Create(REFCLSID,REFIID,void**) DECLSPEC_HIDDEN;

BOOL load_gecko(BOOL) DECLSPEC_HIDDEN;
void close_gecko(void) DECLSPEC_HIDDEN;
void register_nsservice(nsIComponentRegistrar*,nsIServiceManager*) DECLSPEC_HIDDEN;
void init_nsio(nsIComponentManager*,nsIComponentRegistrar*) DECLSPEC_HIDDEN;
void release_nsio(void) DECLSPEC_HIDDEN;
BOOL is_gecko_path(const char*) DECLSPEC_HIDDEN;

HRESULT nsuri_to_url(LPCWSTR,BOOL,BSTR*) DECLSPEC_HIDDEN;
BOOL compare_ignoring_frag(IUri*,IUri*) DECLSPEC_HIDDEN;

HRESULT navigate_url(HTMLWindow*,const WCHAR*,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT set_frame_doc(HTMLFrameBase*,nsIDOMDocument*) DECLSPEC_HIDDEN;

void call_property_onchanged(ConnectionPoint*,DISPID) DECLSPEC_HIDDEN;
HRESULT call_set_active_object(IOleInPlaceUIWindow*,IOleInPlaceActiveObject*) DECLSPEC_HIDDEN;

void *nsalloc(size_t) __WINE_ALLOC_SIZE(1) DECLSPEC_HIDDEN;
void nsfree(void*) DECLSPEC_HIDDEN;

void nsACString_InitDepend(nsACString*,const char*) DECLSPEC_HIDDEN;
void nsACString_SetData(nsACString*,const char*) DECLSPEC_HIDDEN;
PRUint32 nsACString_GetData(const nsACString*,const char**) DECLSPEC_HIDDEN;
void nsACString_Finish(nsACString*) DECLSPEC_HIDDEN;

BOOL nsAString_Init(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
void nsAString_InitDepend(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
void nsAString_SetData(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
PRUint32 nsAString_GetData(const nsAString*,const PRUnichar**) DECLSPEC_HIDDEN;
void nsAString_Finish(nsAString*) DECLSPEC_HIDDEN;
HRESULT return_nsstr(nsresult,nsAString*,BSTR*) DECLSPEC_HIDDEN;

nsICommandParams *create_nscommand_params(void) DECLSPEC_HIDDEN;
HRESULT nsnode_to_nsstring(nsIDOMNode*,nsAString*) DECLSPEC_HIDDEN;
void get_editor_controller(NSContainer*) DECLSPEC_HIDDEN;
nsresult get_nsinterface(nsISupports*,REFIID,void**) DECLSPEC_HIDDEN;
nsIWritableVariant *create_nsvariant(void) DECLSPEC_HIDDEN;

void set_window_bscallback(HTMLWindow*,nsChannelBSC*) DECLSPEC_HIDDEN;
void set_current_mon(HTMLWindow*,IMoniker*) DECLSPEC_HIDDEN;
void set_current_uri(HTMLWindow*,IUri*) DECLSPEC_HIDDEN;
HRESULT start_binding(HTMLWindow*,HTMLDocumentNode*,BSCallback*,IBindCtx*) DECLSPEC_HIDDEN;
HRESULT async_start_doc_binding(HTMLWindow*,nsChannelBSC*) DECLSPEC_HIDDEN;
void abort_document_bindings(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void set_download_state(HTMLDocumentObj*,int) DECLSPEC_HIDDEN;

HRESULT bind_mon_to_buffer(HTMLDocumentNode*,IMoniker*,void**,DWORD*) DECLSPEC_HIDDEN;

void set_ready_state(HTMLWindow*,READYSTATE) DECLSPEC_HIDDEN;

HRESULT HTMLSelectionObject_Create(HTMLDocumentNode*,nsISelection*,IHTMLSelectionObject**) DECLSPEC_HIDDEN;
HRESULT HTMLTxtRange_Create(HTMLDocumentNode*,nsIDOMRange*,IHTMLTxtRange**) DECLSPEC_HIDDEN;
HRESULT HTMLStyle_Create(nsIDOMCSSStyleDeclaration*,HTMLStyle**) DECLSPEC_HIDDEN;
IHTMLStyleSheet *HTMLStyleSheet_Create(nsIDOMStyleSheet*) DECLSPEC_HIDDEN;
IHTMLStyleSheetsCollection *HTMLStyleSheetsCollection_Create(nsIDOMStyleSheetList*) DECLSPEC_HIDDEN;

void detach_selection(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void detach_ranges(HTMLDocumentNode*) DECLSPEC_HIDDEN;
HRESULT get_node_text(HTMLDOMNode*,BSTR*) DECLSPEC_HIDDEN;
HRESULT replace_node_by_html(nsIDOMHTMLDocument*,nsIDOMNode*,const WCHAR*) DECLSPEC_HIDDEN;

HRESULT create_nselem(HTMLDocumentNode*,const WCHAR*,nsIDOMHTMLElement**) DECLSPEC_HIDDEN;

HRESULT HTMLDOMTextNode_Create(HTMLDocumentNode*,nsIDOMNode*,HTMLDOMNode**) DECLSPEC_HIDDEN;

struct HTMLAttributeCollection {
    DispatchEx dispex;
    IHTMLAttributeCollection IHTMLAttributeCollection_iface;
    IHTMLAttributeCollection2 IHTMLAttributeCollection2_iface;
    IHTMLAttributeCollection3 IHTMLAttributeCollection3_iface;

    LONG ref;

    HTMLElement *elem;
    struct list attrs;
};

typedef struct {
    DispatchEx dispex;
    IHTMLDOMAttribute IHTMLDOMAttribute_iface;

    LONG ref;

    DISPID dispid;
    HTMLElement *elem;
    struct list entry;
} HTMLDOMAttribute;

HRESULT HTMLDOMAttribute_Create(HTMLElement*,DISPID,HTMLDOMAttribute**) DECLSPEC_HIDDEN;

HRESULT HTMLElement_Create(HTMLDocumentNode*,nsIDOMNode*,BOOL,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLCommentElement_Create(HTMLDocumentNode*,nsIDOMNode*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLAnchorElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLBodyElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLEmbedElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLFormElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLFrameElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLHeadElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLIFrame_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLStyleElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLImgElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLInputElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLObjectElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLOptionElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLScriptElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLSelectElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTable_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTableRow_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTextAreaElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTitleElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLGenericElement_Create(HTMLDocumentNode*,nsIDOMHTMLElement*,HTMLElement**) DECLSPEC_HIDDEN;

void HTMLDOMNode_Init(HTMLDocumentNode*,HTMLDOMNode*,nsIDOMNode*) DECLSPEC_HIDDEN;
void HTMLElement_Init(HTMLElement*,HTMLDocumentNode*,nsIDOMHTMLElement*,dispex_static_data_t*) DECLSPEC_HIDDEN;
void HTMLElement2_Init(HTMLElement*) DECLSPEC_HIDDEN;
void HTMLElement3_Init(HTMLElement*) DECLSPEC_HIDDEN;
void HTMLTextContainer_Init(HTMLTextContainer*,HTMLDocumentNode*,nsIDOMHTMLElement*,dispex_static_data_t*) DECLSPEC_HIDDEN;
void HTMLFrameBase_Init(HTMLFrameBase*,HTMLDocumentNode*,nsIDOMHTMLElement*,dispex_static_data_t*) DECLSPEC_HIDDEN;

HRESULT HTMLDOMNode_QI(HTMLDOMNode*,REFIID,void**) DECLSPEC_HIDDEN;
void HTMLDOMNode_destructor(HTMLDOMNode*) DECLSPEC_HIDDEN;

HRESULT HTMLElement_QI(HTMLDOMNode*,REFIID,void**) DECLSPEC_HIDDEN;
void HTMLElement_destructor(HTMLDOMNode*) DECLSPEC_HIDDEN;
HRESULT HTMLElement_clone(HTMLDOMNode*,nsIDOMNode*,HTMLDOMNode**) DECLSPEC_HIDDEN;
HRESULT HTMLElement_get_attr_col(HTMLDOMNode*,HTMLAttributeCollection**) DECLSPEC_HIDDEN;

HRESULT HTMLFrameBase_QI(HTMLFrameBase*,REFIID,void**) DECLSPEC_HIDDEN;
void HTMLFrameBase_destructor(HTMLFrameBase*) DECLSPEC_HIDDEN;

HRESULT get_node(HTMLDocumentNode*,nsIDOMNode*,BOOL,HTMLDOMNode**) DECLSPEC_HIDDEN;
void release_nodes(HTMLDocumentNode*) DECLSPEC_HIDDEN;

HTMLElement *unsafe_impl_from_IHTMLElement(IHTMLElement*) DECLSPEC_HIDDEN;

void release_script_hosts(HTMLWindow*) DECLSPEC_HIDDEN;
void connect_scripts(HTMLWindow*) DECLSPEC_HIDDEN;
void doc_insert_script(HTMLWindow*,nsIDOMHTMLScriptElement*) DECLSPEC_HIDDEN;
IDispatch *script_parse_event(HTMLWindow*,LPCWSTR) DECLSPEC_HIDDEN;
HRESULT exec_script(HTMLWindow*,const WCHAR*,const WCHAR*,VARIANT*) DECLSPEC_HIDDEN;
void set_script_mode(HTMLWindow*,SCRIPTMODE) DECLSPEC_HIDDEN;
BOOL find_global_prop(HTMLWindow*,BSTR,DWORD,ScriptHost**,DISPID*) DECLSPEC_HIDDEN;
IDispatch *get_script_disp(ScriptHost*) DECLSPEC_HIDDEN;
HRESULT search_window_props(HTMLWindow*,BSTR,DWORD,DISPID*) DECLSPEC_HIDDEN;

HRESULT wrap_iface(IUnknown*,IUnknown*,IUnknown**) DECLSPEC_HIDDEN;

IHTMLElementCollection *create_all_collection(HTMLDOMNode*,BOOL) DECLSPEC_HIDDEN;
IHTMLElementCollection *create_collection_from_nodelist(HTMLDocumentNode*,IUnknown*,nsIDOMNodeList*) DECLSPEC_HIDDEN;
IHTMLElementCollection *create_collection_from_htmlcol(HTMLDocumentNode*,IUnknown*,nsIDOMHTMLCollection*) DECLSPEC_HIDDEN;

/* commands */
typedef struct {
    DWORD id;
    HRESULT (*query)(HTMLDocument*,OLECMD*);
    HRESULT (*exec)(HTMLDocument*,DWORD,VARIANT*,VARIANT*);
} cmdtable_t;

extern const cmdtable_t editmode_cmds[] DECLSPEC_HIDDEN;

void do_ns_command(HTMLDocument*,const char*,nsICommandParams*) DECLSPEC_HIDDEN;

/* timer */
#define UPDATE_UI       0x0001
#define UPDATE_TITLE    0x0002

void update_doc(HTMLDocument*,DWORD) DECLSPEC_HIDDEN;
void update_title(HTMLDocumentObj*) DECLSPEC_HIDDEN;

HRESULT do_query_service(IUnknown*,REFGUID,REFIID,void**) DECLSPEC_HIDDEN;

/* editor */
void init_editor(HTMLDocument*) DECLSPEC_HIDDEN;
void handle_edit_event(HTMLDocument*,nsIDOMEvent*) DECLSPEC_HIDDEN;
HRESULT editor_exec_copy(HTMLDocument*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT editor_exec_cut(HTMLDocument*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT editor_exec_paste(HTMLDocument*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
void handle_edit_load(HTMLDocument*) DECLSPEC_HIDDEN;
HRESULT editor_is_dirty(HTMLDocument*) DECLSPEC_HIDDEN;
void set_dirty(HTMLDocument*,VARIANT_BOOL) DECLSPEC_HIDDEN;

extern DWORD mshtml_tls DECLSPEC_HIDDEN;

typedef struct task_t task_t;
typedef void (*task_proc_t)(task_t*);

struct task_t {
    LONG target_magic;
    task_proc_t proc;
    task_proc_t destr;
    struct task_t *next;
};

typedef struct {
    task_t header;
    HTMLDocumentObj *doc;
} docobj_task_t;

typedef struct {
    HWND thread_hwnd;
    task_t *task_queue_head;
    task_t *task_queue_tail;
    struct list timer_list;
} thread_data_t;

thread_data_t *get_thread_data(BOOL) DECLSPEC_HIDDEN;
HWND get_thread_hwnd(void) DECLSPEC_HIDDEN;

LONG get_task_target_magic(void) DECLSPEC_HIDDEN;
void push_task(task_t*,task_proc_t,task_proc_t,LONG) DECLSPEC_HIDDEN;
void remove_target_tasks(LONG) DECLSPEC_HIDDEN;

DWORD set_task_timer(HTMLDocument*,DWORD,BOOL,IDispatch*) DECLSPEC_HIDDEN;
HRESULT clear_task_timer(HTMLDocument*,BOOL,DWORD) DECLSPEC_HIDDEN;

const char *debugstr_variant(const VARIANT*) DECLSPEC_HIDDEN;

DEFINE_GUID(CLSID_AboutProtocol, 0x3050F406, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_JSProtocol, 0x3050F3B2, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_MailtoProtocol, 0x3050F3DA, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_ResProtocol, 0x3050F3BC, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_SysimageProtocol, 0x76E67A63, 0x06E9, 0x11D2, 0xA8,0x40, 0x00,0x60,0x08,0x05,0x93,0x82);

DEFINE_GUID(CLSID_CMarkup,0x3050f4fb,0x98b5,0x11cf,0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b);

DEFINE_OLEGUID(CGID_DocHostCmdPriv, 0x000214D4L, 0, 0);

/* memory allocation functions */

static inline void * __WINE_ALLOC_SIZE(1) heap_alloc(size_t len)
{
    return HeapAlloc(GetProcessHeap(), 0, len);
}

static inline void * __WINE_ALLOC_SIZE(1) heap_alloc_zero(size_t len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}

static inline void * __WINE_ALLOC_SIZE(2) heap_realloc(void *mem, size_t len)
{
    return HeapReAlloc(GetProcessHeap(), 0, mem, len);
}

static inline void * __WINE_ALLOC_SIZE(2) heap_realloc_zero(void *mem, size_t len)
{
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, mem, len);
}

static inline BOOL heap_free(void *mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}

static inline LPWSTR heap_strdupW(LPCWSTR str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD size;

        size = (strlenW(str)+1)*sizeof(WCHAR);
        ret = heap_alloc(size);
        memcpy(ret, str, size);
    }

    return ret;
}

static inline LPWSTR heap_strndupW(LPCWSTR str, unsigned len)
{
    LPWSTR ret = NULL;

    if(str) {
        ret = heap_alloc((len+1)*sizeof(WCHAR));
        memcpy(ret, str, len*sizeof(WCHAR));
        ret[len] = 0;
    }

    return ret;
}

static inline char *heap_strdupA(const char *str)
{
    char *ret = NULL;

    if(str) {
        DWORD size;

        size = strlen(str)+1;
        ret = heap_alloc(size);
        memcpy(ret, str, size);
    }

    return ret;
}

static inline WCHAR *heap_strdupAtoW(const char *str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD len;

        len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        ret = heap_alloc(len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    }

    return ret;
}

static inline char *heap_strdupWtoA(LPCWSTR str)
{
    char *ret = NULL;

    if(str) {
        DWORD size = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
        ret = heap_alloc(size);
        WideCharToMultiByte(CP_ACP, 0, str, -1, ret, size, NULL, NULL);
    }

    return ret;
}

static inline void windowref_addref(windowref_t *ref)
{
    InterlockedIncrement(&ref->ref);
}

static inline void windowref_release(windowref_t *ref)
{
    if(!InterlockedDecrement(&ref->ref))
        heap_free(ref);
}

HDC get_display_dc(void) DECLSPEC_HIDDEN;
HINSTANCE get_shdoclc(void) DECLSPEC_HIDDEN;
void set_statustext(HTMLDocumentObj*,INT,LPCWSTR) DECLSPEC_HIDDEN;

extern HINSTANCE hInst DECLSPEC_HIDDEN;
