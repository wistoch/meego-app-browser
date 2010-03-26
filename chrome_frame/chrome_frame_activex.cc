// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_activex.h"

#include <wininet.h>

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_bstr_win.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/utils.h"

namespace {

// Class used to maintain a mapping from top-level windows to ChromeFrameActivex
// instances.
class TopLevelWindowMapping {
 public:
  typedef std::vector<HWND> WindowList;

  static TopLevelWindowMapping* instance() {
    return Singleton<TopLevelWindowMapping>::get();
  }

  // Add |cf_window| to the set of windows registered under |top_window|.
  void AddMapping(HWND top_window, HWND cf_window) {
    top_window_map_lock_.Lock();
    top_window_map_[top_window].push_back(cf_window);
    top_window_map_lock_.Unlock();
  }

  // Return the set of Chrome-Frame instances under |window|.
  WindowList GetInstances(HWND window) {
    top_window_map_lock_.Lock();
    WindowList list = top_window_map_[window];
    top_window_map_lock_.Unlock();
    return list;
  }

 private:
  // Constructor is private as this class it to be used as a singleton.
  // See static method instance().
  TopLevelWindowMapping() {}

  friend struct DefaultSingletonTraits<TopLevelWindowMapping>;

  typedef std::map<HWND, WindowList> TopWindowMap;
  TopWindowMap top_window_map_;

  CComAutoCriticalSection top_window_map_lock_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelWindowMapping);
};

// Message pump hook function that monitors for WM_MOVE and WM_MOVING
// messages on a top-level window, and passes notification to the appropriate
// Chrome-Frame instances.
LRESULT CALLBACK TopWindowProc(int code, WPARAM wparam, LPARAM lparam) {
  CWPSTRUCT *info = reinterpret_cast<CWPSTRUCT*>(lparam);
  const UINT &message = info->message;
  const HWND &message_hwnd = info->hwnd;

  switch (message) {
    case WM_MOVE:
    case WM_MOVING: {
      TopLevelWindowMapping::WindowList cf_instances =
          TopLevelWindowMapping::instance()->GetInstances(message_hwnd);
      TopLevelWindowMapping::WindowList::iterator
          iter(cf_instances.begin()), end(cf_instances.end());
      for (;iter != end; ++iter) {
        PostMessage(*iter, WM_HOST_MOVED_NOTIFICATION, NULL, NULL);
      }
      break;
    }
    default:
      break;
  }

  return CallNextHookEx(0, code, wparam, lparam);
}

HHOOK InstallLocalWindowHook(HWND window) {
  if (!window)
    return NULL;

  DWORD proc_thread = ::GetWindowThreadProcessId(window, NULL);
  if (!proc_thread)
    return NULL;

  // Note that this hook is installed as a LOCAL hook.
  return  ::SetWindowsHookEx(WH_CALLWNDPROC,
                             TopWindowProc,
                             NULL,
                             proc_thread);
}

}  // unnamed namespace

ChromeFrameActivex::ChromeFrameActivex()
    : chrome_wndproc_hook_(NULL) {
}

HRESULT ChromeFrameActivex::FinalConstruct() {
  HRESULT hr = Base::FinalConstruct();
  if (FAILED(hr))
    return hr;

  // No need to call FireOnChanged at this point since nobody will be listening.
  ready_state_ = READYSTATE_LOADING;
  return S_OK;
}

ChromeFrameActivex::~ChromeFrameActivex() {
  // We expect these to be released during a call to SetClientSite(NULL).
  DCHECK_EQ(0, onmessage_.size());
  DCHECK_EQ(0, onloaderror_.size());
  DCHECK_EQ(0, onload_.size());
  DCHECK_EQ(0, onreadystatechanged_.size());
  DCHECK_EQ(0, onextensionready_.size());

  if (chrome_wndproc_hook_) {
    BOOL unhook_success = ::UnhookWindowsHookEx(chrome_wndproc_hook_);
    DCHECK(unhook_success);
  }

  // ChromeFramePlugin::Uninitialize()
  Base::Uninitialize();
}

LRESULT ChromeFrameActivex::OnCreate(UINT message, WPARAM wparam, LPARAM lparam,
                                     BOOL& handled) {
  Base::OnCreate(message, wparam, lparam, handled);
  // Install the notification hook on the top-level window, so that we can
  // be notified on move events.  Note that the return value is not checked.
  // This hook is installed here, as opposed to during IOleObject_SetClientSite
  // because m_hWnd has not yet been assigned during the SetSite call.
  InstallTopLevelHook(m_spClientSite);
  return 0;
}

LRESULT ChromeFrameActivex::OnHostMoved(UINT message, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  Base::OnHostMoved();
  return 0;
}

HRESULT ChromeFrameActivex::GetContainingDocument(IHTMLDocument2** doc) {
  ScopedComPtr<IOleContainer> container;
  HRESULT hr = m_spClientSite->GetContainer(container.Receive());
  if (container)
    hr = container.QueryInterface(doc);
  return hr;
}

HRESULT ChromeFrameActivex::GetDocumentWindow(IHTMLWindow2** window) {
  ScopedComPtr<IHTMLDocument2> document;
  HRESULT hr = GetContainingDocument(document.Receive());
  if (document)
    hr = document->get_parentWindow(window);
  return hr;
}

void ChromeFrameActivex::OnLoad(int tab_handle, const GURL& gurl) {
  ScopedComPtr<IDispatch> event;
  std::string url = gurl.spec();
  if (SUCCEEDED(CreateDomEvent("event", url, "", event.Receive())))
    Fire_onload(event);

  FireEvent(onload_, url);
  Base::OnLoad(tab_handle, gurl);
}

void ChromeFrameActivex::OnLoadFailed(int error_code, const std::string& url) {
  ScopedComPtr<IDispatch> event;
  if (SUCCEEDED(CreateDomEvent("event", url, "", event.Receive())))
    Fire_onloaderror(event);

  FireEvent(onloaderror_, url);
  Base::OnLoadFailed(error_code, url);
}

void ChromeFrameActivex::OnMessageFromChromeFrame(int tab_handle,
                                                  const std::string& message,
                                                  const std::string& origin,
                                                  const std::string& target) {
  DLOG(INFO) << __FUNCTION__;

  if (target.compare("*") != 0) {
    bool drop = true;

    if (is_privileged_) {
      // Forward messages if the control is in privileged mode.
      ScopedComPtr<IDispatch> message_event;
      if (SUCCEEDED(CreateDomEvent("message", message, origin,
                                   message_event.Receive()))) {
        ScopedBstr target_bstr(UTF8ToWide(target).c_str());
        Fire_onprivatemessage(message_event, target_bstr);

        FireEvent(onprivatemessage_, message_event, target_bstr);
      }
    } else {
      if (HaveSameOrigin(target, document_url_)) {
        drop = false;
      } else {
        DLOG(WARNING) << "Dropping posted message since target doesn't match "
            "the current document's origin. target=" << target;
      }
    }

    if (drop)
      return;
  }

  ScopedComPtr<IDispatch> message_event;
  if (SUCCEEDED(CreateDomEvent("message", message, origin,
                               message_event.Receive()))) {
    Fire_onmessage(message_event);

    FireEvent(onmessage_, message_event);

    ScopedVariant event_var;
    event_var.Set(static_cast<IDispatch*>(message_event));
    InvokeScriptFunction(onmessage_handler_, event_var.AsInput());
  }
}

void ChromeFrameActivex::OnAutomationServerLaunchFailed(
    AutomationLaunchResult reason, const std::string& server_version) {
  Base::OnAutomationServerLaunchFailed(reason, server_version);

  if (reason == AUTOMATION_VERSION_MISMATCH) {
    DisplayVersionMismatchWarning(m_hWnd, server_version);
  }
}

void ChromeFrameActivex::OnExtensionInstalled(
    const FilePath& path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues response) {
  ScopedBstr path_str(path.value().c_str());
  Fire_onextensionready(path_str, response);
}

void ChromeFrameActivex::OnGetEnabledExtensionsComplete(
    void* user_data,
    const std::vector<FilePath>& extension_directories) {
  SAFEARRAY* sa = ::SafeArrayCreateVector(VT_BSTR, 0,
                                          extension_directories.size());
  sa->fFeatures = sa->fFeatures | FADF_BSTR;
  ::SafeArrayLock(sa);

  for (size_t i = 0; i < extension_directories.size(); ++i) {
    LONG index = static_cast<LONG>(i);
    ::SafeArrayPutElement(sa, &index, reinterpret_cast<void*>(
        CComBSTR(extension_directories[i].ToWStringHack().c_str()).Detach()));
  }

  Fire_ongetenabledextensionscomplete(sa);
  ::SafeArrayUnlock(sa);
  ::SafeArrayDestroy(sa);
}

void ChromeFrameActivex::OnChannelError() {
  Fire_onchannelerror();
}

HRESULT ChromeFrameActivex::OnDraw(ATL_DRAWINFO& draw_info) {  // NOLINT
  HRESULT hr = S_OK;
  int dc_type = ::GetObjectType(draw_info.hicTargetDev);
  if (dc_type == OBJ_ENHMETADC) {
    RECT print_bounds = {0};
    print_bounds.left = draw_info.prcBounds->left;
    print_bounds.right = draw_info.prcBounds->right;
    print_bounds.top = draw_info.prcBounds->top;
    print_bounds.bottom = draw_info.prcBounds->bottom;

    automation_client_->Print(draw_info.hdcDraw, print_bounds);
  } else {
    hr = Base::OnDraw(draw_info);
  }

  return hr;
}

STDMETHODIMP ChromeFrameActivex::Load(IPropertyBag* bag, IErrorLog* error_log) {
  DCHECK(bag);

  const wchar_t* event_props[] = {
    (L"onload"),
    (L"onloaderror"),
    (L"onmessage"),
    (L"onreadystatechanged"),
  };

  ScopedComPtr<IHTMLObjectElement> obj_element;
  GetObjectElement(obj_element.Receive());

  ScopedBstr object_id;
  GetObjectScriptId(obj_element, object_id.Receive());

  ScopedComPtr<IHTMLElement2> element;
  element.QueryFrom(obj_element);
  HRESULT hr = S_OK;

  for (int i = 0; SUCCEEDED(hr) && i < arraysize(event_props); ++i) {
    ScopedBstr prop(event_props[i]);
    ScopedVariant value;
    if (SUCCEEDED(bag->Read(prop, value.Receive(), error_log))) {
      if (value.type() != VT_BSTR ||
          FAILED(hr = CreateScriptBlockForEvent(element, object_id,
                                                V_BSTR(&value), prop))) {
        DLOG(ERROR) << "Failed to create script block for " << prop
                    << StringPrintf(L"hr=0x%08X, vt=%i", hr, value.type());
      } else {
        DLOG(INFO) << "script block created for event " << prop <<
            StringPrintf(" (0x%08X)", hr) << " connections: " <<
            ProxyDIChromeFrameEvents<ChromeFrameActivex>::m_vec.GetSize();
      }
    } else {
      DLOG(INFO) << "event property " << prop << " not in property bag";
    }
  }

  ScopedVariant src;
  if (SUCCEEDED(bag->Read(StackBstr(L"src"), src.Receive(), error_log))) {
    if (src.type() == VT_BSTR) {
      hr = put_src(V_BSTR(&src));
      DCHECK(hr != E_UNEXPECTED);
    }
  }

  ScopedVariant use_chrome_network;
  if (SUCCEEDED(bag->Read(StackBstr(L"useChromeNetwork"),
                          use_chrome_network.Receive(), error_log))) {
    VariantChangeType(use_chrome_network.AsInput(),
                      use_chrome_network.AsInput(),
                      0, VT_BOOL);
    if (use_chrome_network.type() == VT_BOOL) {
      hr = put_useChromeNetwork(V_BOOL(&use_chrome_network));
      DCHECK(hr != E_UNEXPECTED);
    }
  }

  DLOG_IF(ERROR, FAILED(hr))
      << StringPrintf("Failed to load property bag: 0x%08X", hr);

  return hr;
}

const wchar_t g_activex_mixed_content_error[] = {
    L"data:text/html,<html><body><b>ChromeFrame Security Error<br><br>"
    L"Cannot navigate to HTTP url when document URL is HTTPS</body></html>"};

STDMETHODIMP ChromeFrameActivex::put_src(BSTR src) {
  GURL document_url(GetDocumentUrl());
  if (document_url.SchemeIsSecure()) {
    GURL source_url(src);
    if (!source_url.SchemeIsSecure()) {
      Base::put_src(ScopedBstr(g_activex_mixed_content_error));
      return E_ACCESSDENIED;
    }
  }
  return Base::put_src(src);
}

HRESULT ChromeFrameActivex::IOleObject_SetClientSite(
    IOleClientSite* client_site) {
  HRESULT hr = Base::IOleObject_SetClientSite(client_site);
  if (FAILED(hr) || !client_site) {
    EventHandlers* handlers[] = {
      &onmessage_,
      &onloaderror_,
      &onload_,
      &onreadystatechanged_,
      &onextensionready_,
    };

    for (int i = 0; i < arraysize(handlers); ++i)
      handlers[i]->clear();

    // Drop privileged mode on uninitialization.
    is_privileged_ = false;
  } else {
    ScopedComPtr<IHTMLDocument2> document;
    GetContainingDocument(document.Receive());
    if (document) {
      ScopedBstr url;
      if (SUCCEEDED(document->get_URL(url.Receive())))
        WideToUTF8(url, url.Length(), &document_url_);
    }

    // Probe to see whether the host implements the privileged service.
    ScopedComPtr<IChromeFramePrivileged> service;
    HRESULT service_hr = DoQueryService(SID_ChromeFramePrivileged, client_site,
                                service.Receive());
    if (SUCCEEDED(service_hr) && service) {
      // Does the host want privileged mode?
      boolean wants_privileged = false;
      service_hr = service->GetWantsPrivileged(&wants_privileged);

      if (SUCCEEDED(service_hr) && wants_privileged)
        is_privileged_ = true;

      url_fetcher_.set_privileged_mode(is_privileged_);
    }

    std::wstring chrome_extra_arguments;
    std::wstring profile_name(GetHostProcessName(false));
    if (is_privileged_) {
      // Does the host want to provide extra arguments?
      ScopedBstr extra_arguments_arg;
      service_hr = service->GetChromeExtraArguments(
          extra_arguments_arg.Receive());
      if (S_OK == service_hr && extra_arguments_arg)
        chrome_extra_arguments.assign(extra_arguments_arg,
                                      extra_arguments_arg.Length());

      ScopedBstr automated_functions_arg;
      service_hr = service->GetExtensionApisToAutomate(
          automated_functions_arg.Receive());
      if (S_OK == service_hr && automated_functions_arg) {
        std::string automated_functions(
            WideToASCII(static_cast<BSTR>(automated_functions_arg)));
        functions_enabled_.clear();
        // SplitString writes one empty entry for blank strings, so we need this
        // to allow specifying zero automation of API functions.
        if (!automated_functions.empty())
          SplitString(automated_functions, ',', &functions_enabled_);
      }

      ScopedBstr profile_name_arg;
      service_hr = service->GetChromeProfileName(profile_name_arg.Receive());
      if (S_OK == service_hr && profile_name_arg)
        profile_name.assign(profile_name_arg, profile_name_arg.Length());
    }

    url_fetcher_.set_frame_busting(!is_privileged_);
    automation_client_->SetUrlFetcher(&url_fetcher_);
    if (!InitializeAutomation(profile_name, chrome_extra_arguments,
                              IsIEInPrivate(), true)) {
      return E_FAIL;
    }
  }

  return hr;
}

HRESULT ChromeFrameActivex::GetObjectScriptId(IHTMLObjectElement* object_elem,
                                              BSTR* id) {
  DCHECK(object_elem != NULL);
  DCHECK(id != NULL);

  HRESULT hr = E_FAIL;
  if (object_elem) {
    ScopedComPtr<IHTMLElement> elem;
    hr = elem.QueryFrom(object_elem);
    if (elem) {
      hr = elem->get_id(id);
    }
  }

  return hr;
}

HRESULT ChromeFrameActivex::GetObjectElement(IHTMLObjectElement** element) {
  DCHECK(m_spClientSite);
  if (!m_spClientSite)
    return E_UNEXPECTED;

  ScopedComPtr<IOleControlSite> site;
  HRESULT hr = site.QueryFrom(m_spClientSite);
  if (site) {
    ScopedComPtr<IDispatch> disp;
    hr = site->GetExtendedControl(disp.Receive());
    if (disp) {
      hr = disp.QueryInterface(element);
    } else {
      DCHECK(FAILED(hr));
    }
  }

  return hr;
}

HRESULT ChromeFrameActivex::CreateScriptBlockForEvent(
    IHTMLElement2* insert_after, BSTR instance_id, BSTR script,
    BSTR event_name) {
  DCHECK(insert_after);
  DCHECK_GT(::SysStringLen(event_name), 0UL);  // should always have this

  // This might be 0 if not specified in the HTML document.
  if (!::SysStringLen(instance_id)) {
    // TODO(tommi): Should we give ourselves an ID if this happens?
    NOTREACHED() << "Need to handle this";
    return E_INVALIDARG;
  }

  ScopedComPtr<IHTMLDocument2> document;
  HRESULT hr = GetContainingDocument(document.Receive());
  if (SUCCEEDED(hr)) {
    ScopedComPtr<IHTMLElement> element, new_element;
    document->createElement(StackBstr(L"script"), element.Receive());
    if (element) {
      ScopedComPtr<IHTMLScriptElement> script_element;
      if (SUCCEEDED(hr = script_element.QueryFrom(element))) {
        script_element->put_htmlFor(instance_id);
        script_element->put_event(event_name);
        script_element->put_text(script);

        hr = insert_after->insertAdjacentElement(StackBstr(L"afterEnd"),
                                                 element,
                                                 new_element.Receive());
      }
    }
  }

  return hr;
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   const std::string& arg) {
  if (handlers.size()) {
    ScopedComPtr<IDispatch> event;
    if (SUCCEEDED(CreateDomEvent("event", arg, "", event.Receive()))) {
      FireEvent(handlers, event);
    }
  }
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   IDispatch* event) {
  DCHECK(event != NULL);
  VARIANT arg = { VT_DISPATCH };
  arg.pdispVal = event;
  DISPPARAMS params = { &arg, NULL, 1, 0 };
  for (EventHandlers::const_iterator it = handlers.begin();
       it != handlers.end();
       ++it) {
    HRESULT hr = (*it)->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_METHOD, &params, NULL, NULL, NULL);
    // 0x80020101 == SCRIPT_E_REPORTED.
    // When the script we're invoking has an error, we get this error back.
    DLOG_IF(ERROR, FAILED(hr) && hr != 0x80020101)
        << StringPrintf(L"Failed to invoke script: 0x%08X", hr);
  }
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   IDispatch* event, BSTR target) {
  DCHECK(event != NULL);
  // Arguments in reverse order to event handler function declaration,
  // because that's what DISPPARAMS requires.
  VARIANT args[2] = { { VT_BSTR }, { VT_DISPATCH }, };
  args[0].bstrVal = target;
  args[1].pdispVal = event;
  DISPPARAMS params = { args, NULL, arraysize(args), 0 };
  for (EventHandlers::const_iterator it = handlers.begin();
       it != handlers.end();
       ++it) {
    HRESULT hr = (*it)->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_METHOD, &params, NULL, NULL, NULL);
    // 0x80020101 == SCRIPT_E_REPORTED.
    // When the script we're invoking has an error, we get this error back.
    DLOG_IF(ERROR, FAILED(hr) && hr != 0x80020101)
        << StringPrintf(L"Failed to invoke script: 0x%08X", hr);
  }
}

HRESULT ChromeFrameActivex::InstallTopLevelHook(IOleClientSite* client_site) {
  // Get the parent window of the site, and install our hook on the topmost
  // window of the parent.
  ScopedComPtr<IOleWindow> ole_window;
  HRESULT hr = ole_window.QueryFrom(client_site);
  if (FAILED(hr))
    return hr;

  HWND parent_wnd;
  hr = ole_window->GetWindow(&parent_wnd);
  if (FAILED(hr))
    return hr;

  HWND top_window = ::GetAncestor(parent_wnd, GA_ROOT);
  chrome_wndproc_hook_ = InstallLocalWindowHook(top_window);
  if (chrome_wndproc_hook_)
    TopLevelWindowMapping::instance()->AddMapping(top_window, m_hWnd);

  return chrome_wndproc_hook_ ? S_OK : E_FAIL;
}
