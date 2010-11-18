// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE browser helper object implementation.
#ifndef CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_
#define CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>  // Needed for exdisp.h
#include <exdisp.h>
#include <exdispid.h>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "ceee/ie/plugin/bho/tab_events_funnel.h"
#include "ceee/ie/common/chrome_frame_host.h"
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include "ceee/ie/plugin/bho/extension_port_manager.h"
#include "ceee/ie/plugin/bho/tool_band_visibility.h"
#include "ceee/ie/plugin/bho/web_browser_events_source.h"
#include "ceee/ie/plugin/bho/web_progress_notifier.h"
#include "ceee/ie/plugin/scripting/userscripts_librarian.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/ie/plugin/toolband/resource.h"
#include "broker_lib.h"  // NOLINT
#include "toolband.h"  // NOLINT

// Implementation of an IE browser helper object.
class ATL_NO_VTABLE BrowserHelperObject
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<BrowserHelperObject, &CLSID_BrowserHelperObject>,
      public IObjectWithSiteImpl<BrowserHelperObject>,
      public IDispEventSimpleImpl<0,
                                  BrowserHelperObject,
                                  &DIID_DWebBrowserEvents2>,
      public IPersistImpl<BrowserHelperObject>,
      public IFrameEventHandlerHost,
      public IExtensionPortMessagingProvider,
      public IChromeFrameHostEvents,
      public ToolBandVisibility,
      public WebBrowserEventsSource {
 public:
  DECLARE_REGISTRY_RESOURCEID(IDR_BROWSERHELPEROBJECT)
  DECLARE_NOT_AGGREGATABLE(BrowserHelperObject)

  BEGIN_COM_MAP(BrowserHelperObject)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IPersist)
    COM_INTERFACE_ENTRY_IID(IID_IFrameEventHandlerHost, IFrameEventHandlerHost)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  BEGIN_SINK_MAP(BrowserHelperObject)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                    OnBeforeNavigate2,
                    &handler_type_idispatch_5variantptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                    OnDocumentComplete, &handler_type_idispatch_variantptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                    OnNavigateComplete2, &handler_type_idispatch_variantptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                    OnNavigateError,
                    &handler_type_idispatch_3variantptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2,
                    OnNewWindow2, &handler_type_idispatchptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                    OnNewWindow3,
                    &handler_type_idispatchptr_boolptr_dword_2bstr_)
  END_SINK_MAP()

  BrowserHelperObject();
  ~BrowserHelperObject();

  HRESULT FinalConstruct();
  void FinalRelease();

  // @name IObjectWithSite override.
  STDMETHOD(SetSite)(IUnknown* site);

  // @name IExtensionPortMessagingProvider implementation
  // @{
  virtual void CloseAll(IContentScriptNativeApi* instance);
  virtual HRESULT OpenChannelToExtension(IContentScriptNativeApi* instance,
                                         const std::string& extension,
                                         const std::string& channel_name,
                                         int cookie);
  virtual HRESULT PostMessage(int port_id, const std::string& message);
  // @}

  // @name IChromeFrameHostEvents implementation
  virtual HRESULT OnCfReadyStateChanged(LONG state);
  virtual HRESULT OnCfPrivateMessage(BSTR msg, BSTR origin, BSTR target);
  virtual HRESULT OnCfExtensionReady(BSTR path, int response);
  virtual HRESULT OnCfGetEnabledExtensionsComplete(
      SAFEARRAY* tab_delimited_paths);
  virtual HRESULT OnCfGetExtensionApisToAutomate(BSTR* functions_enabled);
  virtual HRESULT OnCfChannelError();

  // @name WebBrowser event handlers
  // @{
  STDMETHOD_(void, OnBeforeNavigate2)(IDispatch* webbrowser_disp, VARIANT* url,
                                      VARIANT* flags,
                                      VARIANT* target_frame_name,
                                      VARIANT* post_data, VARIANT* headers,
                                      VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* webbrowser_disp,
                                       VARIANT* url);
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* webbrowser_disp,
                                        VARIANT* url);
  STDMETHOD_(void, OnNavigateError)(IDispatch* webbrowser_disp, VARIANT* url,
                                    VARIANT* target_frame_name,
                                    VARIANT* status_code, VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNewWindow2)(IDispatch** webbrowser_disp,
                                 VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNewWindow3)(IDispatch** webbrowser_disp,
                                 VARIANT_BOOL* cancel, DWORD flags,
                                 BSTR url_context, BSTR url);
  // @}

  // @name IFrameEventHandlerHost
  // @{
  virtual HRESULT AttachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler);
  virtual HRESULT DetachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler);
  virtual HRESULT GetTopLevelBrowser(IWebBrowser2** browser);
  virtual HRESULT GetMatchingUserScriptsCssContent(
      const GURL& url, bool require_all_frames, std::string* css_content);
  virtual HRESULT GetMatchingUserScriptsJsContent(
      const GURL& url, UserScript::RunLocation location,
      bool require_all_frames,
      UserScriptsLibrarian::JsFileList* js_file_list);
  virtual HRESULT OnReadyStateChanged(READYSTATE ready_state);
  virtual HRESULT GetReadyState(READYSTATE* ready_state);
  virtual HRESULT GetExtensionId(std::wstring* extension_id);
  virtual HRESULT GetExtensionPath(std::wstring* extension_path);
  virtual HRESULT GetExtensionPortMessagingProvider(
      IExtensionPortMessagingProvider** messaging_provider);
  virtual HRESULT InsertCode(BSTR code, BSTR file, BOOL all_frames,
                             CeeeTabCodeType type);
  // @}

  // @name WebBrowserEventsSource
  // @{
  // Both RegisterSink and UnregisterSink are supposed to be called from the
  // main browser thread of the tab to which this BHO is attached. Sinks will
  // receive notifications on the same thread.
  virtual void RegisterSink(Sink* sink);
  virtual void UnregisterSink(Sink* sink);
  // @}

 protected:
  // Finds the handler attached to webbrowser.
  // @returns S_OK if handler is found.
  HRESULT GetBrowserHandler(IWebBrowser2* webbrowser,
                            IFrameEventHandler** handler);

  virtual void HandleNavigateComplete(IWebBrowser2* webbrowser, BSTR url);
  virtual HRESULT HandleReadyStateChanged(READYSTATE old_state,
                                          READYSTATE new_state);

  // Unit testing seems to create the frame event handler.
  virtual HRESULT CreateFrameEventHandler(IWebBrowser2* browser,
                                          IWebBrowser2* parent_browser,
                                          IFrameEventHandler** handler);

  // Unit testing seems to get the parent of a browser.
  virtual HRESULT GetParentBrowser(IWebBrowser2* browser,
                                   IWebBrowser2** parent_browser);

  // Unit testing seems to create the broker registrar.
  virtual HRESULT GetBrokerRegistrar(ICeeeBrokerRegistrar** broker);

  // Unit testing seems to create an executor.
  virtual HRESULT CreateExecutor(IUnknown** executor);

  // Unit testing seems to create a WebProgressNotifier instance.
  virtual WebProgressNotifier* CreateWebProgressNotifier();

  // Initializes the BHO to the given site.
  // Called from SetSite.
  HRESULT Initialize(IUnknown* site);

  // Tears down an initialized bho.
  // Called from SetSite.
  HRESULT TearDown();

  // Creates and initializes the chrome frame host.
  HRESULT InitializeChromeFrameHost();

  // Fetch and remembers the tab window we are attached to.
  // Virtual for testing purposes.
  virtual HRESULT GetTabWindow(IServiceProvider* service_provider);

  // Connect for notifications.
  HRESULT ConnectSinks(IServiceProvider* service_provider);

  // Isolate the creation of the host so we can overload it to mock
  // the Chrome Frame Host in our tests.
  virtual HRESULT CreateChromeFrameHost();

  // Accessor so that we can mock it in unit tests.
  virtual TabEventsFunnel& tab_events_funnel() { return tab_events_funnel_; }

  // Fires the tab.onCreated event via the tab event funnel.
  virtual HRESULT FireOnCreatedEvent(BSTR url);

  // Fires the tab.onRemoved event via the tab event funnel.
  virtual HRESULT FireOnRemovedEvent();

  // Fires the private message to unmap a tab to its BHO.
  virtual HRESULT FireOnUnmappedEvent();

  // Loads our manifest and initialize our librarian.
  virtual void LoadManifestFile(const std::wstring& base_dir);

  // Called when we know the base directory of our extension.
  void StartExtension(const wchar_t* base_dir);

  // Our ToolBandVisibility window maintains a refcount on us for the duration
  // of its lifetime. The self-reference is managed with these two methods.
  virtual void OnFinalMessage(HWND window);
  virtual LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);

  // Compares two URLs and returns whether they represent a hash change.
  virtual bool IsHashChange(BSTR url1, BSTR url2);

  // Ensure that the tab ID is correct. On the first time it's set, it will
  // call all deferred methods added to deferred_tab_id_call_.
  // This method should be called by every method that send a message or use
  // the tab event funnel, as they need the tab_id to be mapped.
  // If this method returns false, the caller should defer itself using the
  // deferred_tab_id_call_ list.
  virtual bool EnsureTabId();

  // Returns true if the browser interface passed in contains a full tab
  // chrome frame.
  virtual bool BrowserContainsChromeFrame(IWebBrowser2* browser);

  // Attach ourselves and the event handler to the browser, and launches the
  // right events when going to and from a Full Tab Chrome Frame.
  virtual HRESULT AttachBrowserHandler(IWebBrowser2* webbrowser,
                                       IFrameEventHandler** handler);

  // Function info objects describing our message handlers.
  // Effectively const but can't make const because of silly ATL macro problem.
  static _ATL_FUNC_INFO handler_type_idispatch_5variantptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatch_variantptr_;
  static _ATL_FUNC_INFO handler_type_idispatch_3variantptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatchptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatchptr_boolptr_dword_2bstr_;

  // The top-level web browser (window) we're attached to. NULL before SetSite.
  CComPtr<IWebBrowser2> web_browser_;

  // The Chrome Frame host handling a Chrome Frame instance for us.
  CComPtr<IChromeFrameHost> chrome_frame_host_;

  // The Broker Registrar we use to un/register executors for our thread.
  CComPtr<ICeeeBrokerRegistrar> broker_registrar_;

  // We keep a reference to the executor we registered so that we can
  // manually disconnect it, so it doesn't get called while we unregister it.
  CComPtr<IUnknown> executor_;

  // Maintains a map from browser (top-level and sub-browsers) to the
  // attached FrameEventHandlers.
  typedef std::map<CAdapt<CComPtr<IUnknown> >,
                   CAdapt<CComPtr<IFrameEventHandler> > > BrowserHandlerMap;
  BrowserHandlerMap browsers_;

  // Initialized by LoadManifestFile() at
  // OnCfGetEnabledExtensionsComplete-time. Valid from that point forward.
  UserScriptsLibrarian librarian_;

  // Filesystem path to the .crx we will install (or have installed), or the
  // empty string, or (if not ending in .crx) the path to an exploded extension
  // directory to load (or which we have loaded).
  std::wstring extension_path_;

  // The extension we're associated with.  Set at
  // OnCfGetEnabledExtensionsComplete-time.
  // TODO(siggi@chromium.org): Generalize this to multiple extensions.
  std::wstring extension_id_;

  // The base directory of the extension we're associated with.
  // Set at OnCfGetEnabledExtensionsComplete time.
  std::wstring extension_base_dir_;

  // Extension port messaging and management is delegated to this.
  ExtensionPortManager extension_port_manager_;

  // Used to dispatch tab events back to Chrome.
  TabEventsFunnel tab_events_funnel_;

  // Remember the tab window handle so that we can use it.
  HWND tab_window_;

  // Remember the tab id so we can pass it to the underlying Chrome.
  int tab_id_;

  // Makes sure we fire the onCreated event only once.
  bool fired_on_created_event_;

  // True if we found no enabled extensions and tried to install one.
  bool already_tried_installing_;

  // The last known ready state lower bound, so that we decide when to fire a
  // tabs.onUpdated event, which only when we go from all frames completed to
  // at least one of them not completed, and vice versa (from incomplete to
  // fully completely completed :-)...
  READYSTATE lower_bound_ready_state_;

  // Consumers of WebBrowser events.
  std::vector<Sink*> sinks_;

  // Used to generate and fire Web progress notifications.
  scoped_ptr<WebProgressNotifier> web_progress_notifier_;

  // True if the user is running IE7 or later.
  bool ie7_or_later_;

  // The thread we are running into.
  DWORD thread_id_;

  // Indicates if the current shown page is a full-tab chrome frame.
  bool full_tab_chrome_frame_;

 private:
  // Used during initialization to get the tab information from Chrome and
  // register ourselves with the broker.
  HRESULT RegisterTabInfo();

  typedef std::deque<Task*> DeferredCallListType;
  DeferredCallListType deferred_tab_id_call_;
};

#endif  // CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_
