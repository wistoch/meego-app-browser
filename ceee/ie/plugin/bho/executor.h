// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// CeeeExecutor & CeeeExecutorCreator implementation, interfaces to
// execute code in other threads which can be running in other another process.

#ifndef CEEE_IE_PLUGIN_BHO_EXECUTOR_H_
#define CEEE_IE_PLUGIN_BHO_EXECUTOR_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string.h>

#include "base/scoped_ptr.h"
#include "ceee/ie/plugin/bho/infobar_manager.h"
#include "ceee/ie/plugin/toolband/resource.h"

#include "toolband.h"  // NOLINT

struct IWebBrowser2;
namespace infobar_api {
  class InfobarManager;
};

// The executor creator hooks itself in the destination thread where
// the executor will then be created and register in the CeeeBroker.

// The creator of CeeeExecutors.
class ATL_NO_VTABLE CeeeExecutorCreator
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<CeeeExecutorCreator,
                         &CLSID_CeeeExecutorCreator>,
      public ICeeeExecutorCreator {
 public:
  CeeeExecutorCreator();
  void FinalRelease();

  DECLARE_REGISTRY_RESOURCEID(IDR_EXECUTOR_CREATOR)

  DECLARE_NOT_AGGREGATABLE(CeeeExecutorCreator)
  BEGIN_COM_MAP(CeeeExecutorCreator)
    COM_INTERFACE_ENTRY(ICeeeExecutorCreator)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // @name ICeeeExecutorCreator implementation.
  // @{
  STDMETHOD(CreateWindowExecutor)(long thread_id, CeeeWindowHandle window);
  STDMETHOD(Teardown)(long thread_id);
  // @}

 protected:
  // The registered message we use to communicate with the destination thread.
  static const UINT kCreateWindowExecutorMessage;

  // The function that will be hooked in the destination thread.
  // See http://msdn.microsoft.com/en-us/library/ms644981(VS.85).aspx
  // for more details.
  static LRESULT CALLBACK GetMsgProc(int code, WPARAM wparam, LPARAM lparam);

  // We must remember the hook so that we can unhook when we are done.
  HHOOK hook_;

  // We can only work for one thread at a time. Used to validate that the
  // call to ICeeeExecutorCreator::Teardown are balanced to a previous call
  // to ICeeeExecutorCreator::CreateExecutor.
  long current_thread_id_;
};

// The executor object that is instantiated in the destination thread and
// then called to... execute stuff...
class ATL_NO_VTABLE CeeeExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<CeeeExecutor, &CLSID_CeeeExecutor>,
      public IObjectWithSiteImpl<CeeeExecutor>,
      public ICeeeWindowExecutor,
      public ICeeeTabExecutor,
      public ICeeeCookieExecutor,
      public ICeeeInfobarExecutor {
 public:
  DECLARE_REGISTRY_RESOURCEID(IDR_EXECUTOR)

  DECLARE_NOT_AGGREGATABLE(CeeeExecutor)
  BEGIN_COM_MAP(CeeeExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(ICeeeWindowExecutor)
    COM_INTERFACE_ENTRY(ICeeeTabExecutor)
    COM_INTERFACE_ENTRY(ICeeeCookieExecutor)
    COM_INTERFACE_ENTRY(ICeeeInfobarExecutor)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()
  DECLARE_CLASSFACTORY()

  // @name ICeeeWindowExecutor implementation.
  // @{
  STDMETHOD(Initialize)(CeeeWindowHandle hwnd);
  STDMETHOD(GetWindow)(BOOL populate_tabs, CeeeWindowInfo* window_info);
  STDMETHOD(GetTabs)(BSTR* tab_list);
  STDMETHOD(UpdateWindow)(long left, long top, long width, long height,
      CeeeWindowInfo* window_info);
  STDMETHOD(RemoveWindow)();
  STDMETHOD(GetTabIndex)(CeeeWindowHandle tab, long* index);
  STDMETHOD(MoveTab)(CeeeWindowHandle tab, long index);
  STDMETHOD(RemoveTab)(CeeeWindowHandle tab);
  STDMETHOD(SelectTab)(CeeeWindowHandle tab);
  // @}

  // @name ICeeeTabExecutor implementation.
  // @{
  // Initialize was already declared in ICeeeWindowExecutor, so we don't
  // add it here, even if it's part of the interface.
  STDMETHOD(GetTabInfo)(CeeeTabInfo* tab_info);
  STDMETHOD(Navigate)(BSTR url, long flags, BSTR target);
  STDMETHOD(InsertCode)(BSTR code, BSTR file, BOOL all_frames,
                        CeeeTabCodeType type);
  // @}

  // @name ICeeeCookieExecutor implementation.
  // @{
  STDMETHOD(GetCookie)(BSTR url, BSTR name, CeeeCookieInfo* cookie_info);
  STDMETHOD(RegisterCookieStore)();
  STDMETHOD(CookieStoreIsRegistered)();
  // @}

  // @name ICeeeInfobarExecutor implementation.
  // @{
  STDMETHOD(SetExtensionId)(BSTR extension_id);
  STDMETHOD(ShowInfobar)(BSTR url, CeeeWindowHandle* window_handle);
  STDMETHOD(OnTopFrameBeforeNavigate)(BSTR url);
  // @}

  CeeeExecutor() : hwnd_(NULL) {}

 protected:
  // Get the IWebBrowser2 interface of the
  // frame event host that was set as our site.
  virtual HRESULT GetWebBrowser(IWebBrowser2** browser);

  // Used via EnumChildWindows to get all tabs.
  static BOOL CALLBACK GetTabsEnumProc(HWND window, LPARAM param);

  // Ensure we're running inside the right thread.
  HRESULT EnsureWindowThread();

  // The HWND of the tab/window we are associated to.
  HWND hwnd_;

  // Extension id.
  std::string extension_id_;

  // Get the value of the cookie with the given name, associated with the given
  // URL. Returns S_FALSE if the cookie does not exist, and returns an error
  // code if something unexpected occurs.
  virtual HRESULT GetCookieValue(BSTR url, BSTR name, BSTR* value);

  // Mainly for unit testing purposes.
  void set_cookie_store_is_registered(bool is_registered);

  // Instance of InfobarManager for the tab associated with the thread to which
  // the executor is attached.
  scoped_ptr<infobar_api::InfobarManager> infobar_manager_;
};

#endif  // CEEE_IE_PLUGIN_BHO_EXECUTOR_H_
