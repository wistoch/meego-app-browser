// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ChromeActiveDocument
#include "chrome_frame/chrome_active_document.h"

#include <hlink.h>
#include <htiface.h>
#include <initguid.h>
#include <mshtmcid.h>
#include <shdeprecated.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <tlogstg.h>
#include <urlmon.h>
#include <wininet.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/scoped_variant_win.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/thread_local.h"

#include "grit/generated_resources.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/navigation_types.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/utils.h"

const wchar_t kChromeAttachExternalTabPrefix[] = L"attach_external_tab";

static const wchar_t kUseChromeNetworking[] = L"UseChromeNetworking";
static const wchar_t kHandleTopLevelRequests[] = L"HandleTopLevelRequests";

DEFINE_GUID(CGID_DocHostCmdPriv, 0x000214D4L, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0,
            0x46);


base::ThreadLocalPointer<ChromeActiveDocument> g_active_doc_cache;

bool g_first_launch_by_process_ = true;

ChromeActiveDocument::ChromeActiveDocument()
    : first_navigation_(true),
      is_automation_client_reused_(false),
      accelerator_table_(NULL) {
  url_fetcher_.set_frame_busting(false);
  memset(&navigation_info_, 0, sizeof(navigation_info_));
}

HRESULT ChromeActiveDocument::FinalConstruct() {
  // If we have a cached ChromeActiveDocument instance in TLS, then grab
  // ownership of the cached document's automation client. This is an
  // optimization to get Chrome active documents to load faster.
  ChromeActiveDocument* cached_document = g_active_doc_cache.Get();
  if (cached_document) {
    DCHECK(automation_client_.get() == NULL);
    automation_client_ = cached_document->automation_client_.release();
    DLOG(INFO) << "Reusing automation client instance from "
               << cached_document;
    DCHECK(automation_client_.get() != NULL);
    automation_client_->Reinitialize(this, &url_fetcher_);
    is_automation_client_reused_ = true;
  } else {
    // The FinalConstruct implementation in the ChromeFrameActivexBase class
    // i.e. Base creates an instance of the ChromeFrameAutomationClient class
    // and initializes it, which would spawn a new Chrome process, etc.
    // We don't want to be doing this if we have a cached document, whose
    // automation client instance can be reused.
    HRESULT hr = Base::FinalConstruct();
    if (FAILED(hr))
      return hr;
  }

  bool chrome_network = GetConfigBool(false, kUseChromeNetworking);
  bool top_level_requests = GetConfigBool(true, kHandleTopLevelRequests);
  automation_client_->set_use_chrome_network(chrome_network);
  automation_client_->set_handle_top_level_requests(top_level_requests);

  find_dialog_.Init(automation_client_.get());

  enabled_commands_map_[OLECMDID_PRINT] = true;
  enabled_commands_map_[OLECMDID_FIND] = true;
  enabled_commands_map_[OLECMDID_CUT] = true;
  enabled_commands_map_[OLECMDID_COPY] = true;
  enabled_commands_map_[OLECMDID_PASTE] = true;
  enabled_commands_map_[OLECMDID_SELECTALL] = true;
  enabled_commands_map_[OLECMDID_SAVEAS] = true;

  accelerator_table_ =
    LoadAccelerators(GetModuleHandle(L"npchrome_frame.dll"),
                     MAKEINTRESOURCE(IDR_CHROME_FRAME_IE_FULL_TAB));
  DCHECK(accelerator_table_ != NULL);
  return S_OK;
}

ChromeActiveDocument::~ChromeActiveDocument() {
  DLOG(INFO) << __FUNCTION__;
  if (find_dialog_.IsWindow()) {
    find_dialog_.DestroyWindow();
  }
  // ChromeFramePlugin
  Base::Uninitialize();
}

// Override DoVerb
STDMETHODIMP ChromeActiveDocument::DoVerb(LONG verb,
                                          LPMSG msg,
                                          IOleClientSite* active_site,
                                          LONG index,
                                          HWND parent_window,
                                          LPCRECT pos) {
  // IE will try and in-place activate us in some cases. This happens when
  // the user opens a new IE window with a URL that has us as the DocObject.
  // Here we refuse to be activated in-place and we will force IE to UIActivate
  // us.
  if (OLEIVERB_INPLACEACTIVATE == verb) {
    return E_NOTIMPL;
  }
  // Check if we should activate as a docobject or not
  // (client supports IOleDocumentSite)
  if (doc_site_) {
    switch (verb) {
    case OLEIVERB_SHOW: {
      ScopedComPtr<IDocHostUIHandler> doc_host_handler;
      doc_host_handler.QueryFrom(doc_site_);
      if (doc_host_handler.get()) {
        doc_host_handler->ShowUI(DOCHOSTUITYPE_BROWSE, this, this, NULL, NULL);
      }
    }
    case OLEIVERB_OPEN:
    case OLEIVERB_UIACTIVATE:
      if (!m_bUIActive) {
        return doc_site_->ActivateMe(NULL);
      }
      break;
    }
  }
  return IOleObjectImpl<ChromeActiveDocument>::DoVerb(verb,
                                                      msg,
                                                      active_site,
                                                      index,
                                                      parent_window,
                                                      pos);
}

// Override IOleInPlaceActiveObjectImpl::OnDocWindowActivate
STDMETHODIMP ChromeActiveDocument::OnDocWindowActivate(BOOL activate) {
  DLOG(INFO) << __FUNCTION__;
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::TranslateAccelerator(MSG* msg) {
  DLOG(INFO) << __FUNCTION__;
  if (msg == NULL)
    return E_POINTER;

  if (msg->message == WM_KEYDOWN && msg->wParam == VK_TAB) {
    HWND focus = ::GetFocus();
    if (focus != m_hWnd && !::IsChild(m_hWnd, focus)) {
      // The call to SetFocus triggers a WM_SETFOCUS that makes the base class
      // set focus to the correct element in Chrome.
      ::SetFocus(m_hWnd);
      return S_OK;
    }
  }

  return S_FALSE;
}
// Override IPersistStorageImpl::IsDirty
STDMETHODIMP ChromeActiveDocument::IsDirty() {
  DLOG(INFO) << __FUNCTION__;
  return S_FALSE;
}

void ChromeActiveDocument::OnAutomationServerReady() {
  Base::OnAutomationServerReady();
  Base::GiveFocusToChrome();
}

STDMETHODIMP ChromeActiveDocument::Load(BOOL fully_avalable,
                                        IMoniker* moniker_name,
                                        LPBC bind_context,
                                        DWORD mode) {
  if (NULL == moniker_name) {
    return E_INVALIDARG;
  }

  ScopedComPtr<IOleClientSite> client_site;
  if (bind_context) {
    ScopedComPtr<IUnknown> site;
    bind_context->GetObjectParam(SZ_HTML_CLIENTSITE_OBJECTPARAM,
                                 site.Receive());
    if (site)
      client_site.QueryFrom(site);
  }

  if (client_site) {
    SetClientSite(client_site);
  }

  Bho* chrome_frame_bho = Bho::GetCurrentThreadBhoInstance();

  // If the original URL contains an anchor, then the URL queried
  // from the moniker does not contain the anchor. To workaround
  // this we retrieve the URL from our BHO.
  std::wstring url = GetActualUrlFromMoniker(
      moniker_name, bind_context,
      chrome_frame_bho ? chrome_frame_bho->url() : std::wstring());

  // The is_new_navigation variable indicates if this a navigation initiated
  // by typing in a URL for e.g. in the IE address bar, or from Chrome by
  // a window.open call from javascript, in which case the current IE tab
  // will attach to an existing ExternalTabContainer instance.
  bool is_new_navigation = true;
  bool is_chrome_protocol = false;

  if (!ParseUrl(url, &is_new_navigation, &is_chrome_protocol, &url)) {
    DLOG(WARNING) << __FUNCTION__ << " Failed to parse url:" << url;
    return E_INVALIDARG;
  }

  if (!LaunchUrl(url, is_new_navigation)) {
    NOTREACHED() << __FUNCTION__ << " Failed to launch url:" << url;
    return E_INVALIDARG;
  }

  if (!is_chrome_protocol) {
    url_fetcher_.UseMonikerForUrl(moniker_name, bind_context, url);
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.FullTabLaunchType",
                              is_chrome_protocol, 0, 1, 2);
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Save(IMoniker* moniker_name,
                                        LPBC bind_context,
                                        BOOL remember) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::SaveCompleted(IMoniker* moniker_name,
                                                 LPBC bind_context) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::GetCurMoniker(IMoniker** moniker_name) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::GetClassID(CLSID* class_id) {
  if (NULL == class_id) {
    return E_POINTER;
  }
  *class_id = GetObjectCLSID();
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::QueryStatus(const GUID* cmd_group_guid,
                                               ULONG number_of_commands,
                                               OLECMD commands[],
                                               OLECMDTEXT* command_text) {
  DLOG(INFO) << __FUNCTION__;
  for (ULONG command_index = 0; command_index < number_of_commands;
       command_index++) {
    DLOG(INFO) << "Command id = " << commands[command_index].cmdID;
    if (enabled_commands_map_.find(commands[command_index].cmdID) !=
        enabled_commands_map_.end()) {
      commands[command_index].cmdf = OLECMDF_ENABLED;
    }
  }
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Exec(const GUID* cmd_group_guid,
                                        DWORD command_id,
                                        DWORD cmd_exec_opt,
                                        VARIANT* in_args,
                                        VARIANT* out_args) {
  DLOG(INFO) << __FUNCTION__ << " Cmd id =" << command_id;
  // Bail out if we have been uninitialized.
  if (automation_client_.get() && automation_client_->tab()) {
    return ProcessExecCommand(cmd_group_guid, command_id, cmd_exec_opt,
                              in_args, out_args);
  }
  return OLECMDERR_E_NOTSUPPORTED;
}

STDMETHODIMP ChromeActiveDocument::LoadHistory(IStream* stream,
                                               IBindCtx* bind_context) {
  // Read notes in ChromeActiveDocument::SaveHistory
  DCHECK(stream);
  LARGE_INTEGER offset = {0};
  ULARGE_INTEGER cur_pos = {0};
  STATSTG statstg = {0};

  stream->Seek(offset, STREAM_SEEK_CUR, &cur_pos);
  stream->Stat(&statstg, STATFLAG_NONAME);

  DWORD url_size = statstg.cbSize.LowPart - cur_pos.LowPart;
  ScopedBstr url_bstr;
  DWORD bytes_read = 0;
  stream->Read(url_bstr.AllocateBytes(url_size), url_size, &bytes_read);
  std::wstring url(url_bstr);

  bool is_new_navigation = true;
  bool is_chrome_protocol = false;

  if (!ParseUrl(url, &is_new_navigation, &is_chrome_protocol, &url)) {
    DLOG(WARNING) << __FUNCTION__ << " Failed to parse url:" << url;
    return E_INVALIDARG;
  }

  if (!LaunchUrl(url, is_new_navigation)) {
    NOTREACHED() << __FUNCTION__ << " Failed to launch url:" << url;
    return E_INVALIDARG;
  }
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::SaveHistory(IStream* stream) {
  // TODO(sanjeevr): We need to fetch the entire list of navigation entries
  // from Chrome and persist it in the stream. And in LoadHistory we need to
  // pass this list back to Chrome which will recreate the list. This will allow
  // Back-Forward navigation to anchors to work correctly when we navigate to a
  // page outside of ChromeFrame and then come back.
  if (!stream) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  LARGE_INTEGER offset = {0};
  ULARGE_INTEGER new_pos = {0};
  DWORD written = 0;
  std::wstring url = UTF8ToWide(navigation_info_.url.spec());
  return stream->Write(url.c_str(), (url.length() + 1) * sizeof(wchar_t),
                       &written);
}

STDMETHODIMP ChromeActiveDocument::SetPositionCookie(DWORD position_cookie) {
  int index = static_cast<int>(position_cookie);
  navigation_info_.navigation_index = index;
  automation_client_->NavigateToIndex(index);
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetPositionCookie(DWORD* position_cookie) {
  if (!position_cookie)
    return E_INVALIDARG;

  *position_cookie = navigation_info_.navigation_index;
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetUrlForEvents(BSTR* url) {
  if (NULL == url) {
    return E_POINTER;
  }
  *url = ::SysAllocString(url_);
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetAddressBarUrl(BSTR* url) {
  return GetUrlForEvents(url);
}

HRESULT ChromeActiveDocument::IOleObject_SetClientSite(
    IOleClientSite* client_site) {
  if (client_site == NULL) {
    ChromeActiveDocument* cached_document = g_active_doc_cache.Get();
    if (cached_document) {
      DCHECK(this == cached_document);
      g_active_doc_cache.Set(NULL);
      cached_document->Release();
    }

    ScopedComPtr<IDocHostUIHandler> doc_host_handler;
    if (doc_site_) {
      doc_host_handler.QueryFrom(doc_site_);
    }

    if (doc_host_handler.get()) {
      doc_host_handler->HideUI();
    }

    doc_site_.Release();
    in_place_frame_.Release();
  }

  if (client_site != m_spClientSite)
    return Base::IOleObject_SetClientSite(client_site);

  return S_OK;
}


HRESULT ChromeActiveDocument::ActiveXDocActivate(LONG verb) {
  HRESULT hr = S_OK;
  m_bNegotiatedWnd = TRUE;
  if (!m_bInPlaceActive) {
    hr = m_spInPlaceSite->CanInPlaceActivate();
    if (FAILED(hr)) {
      return hr;
    }
    m_spInPlaceSite->OnInPlaceActivate();
  }
  m_bInPlaceActive = TRUE;
  // get location in the parent window,
  // as well as some information about the parent
  ScopedComPtr<IOleInPlaceUIWindow> in_place_ui_window;
  frame_info_.cb = sizeof(OLEINPLACEFRAMEINFO);
  HWND parent_window = NULL;
  if (m_spInPlaceSite->GetWindow(&parent_window) == S_OK) {
    in_place_frame_.Release();
    RECT position_rect = {0};
    RECT clip_rect = {0};
    m_spInPlaceSite->GetWindowContext(in_place_frame_.Receive(),
                                      in_place_ui_window.Receive(),
                                      &position_rect,
                                      &clip_rect,
                                      &frame_info_);
    if (!m_bWndLess) {
      if (IsWindow()) {
        ::ShowWindow(m_hWnd, SW_SHOW);
        SetFocus();
      } else {
        m_hWnd = Create(parent_window, position_rect);
      }
    }
    SetObjectRects(&position_rect, &clip_rect);
  }

  ScopedComPtr<IOleInPlaceActiveObject> in_place_active_object(this);

  // Gone active by now, take care of UIACTIVATE
  if (DoesVerbUIActivate(verb)) {
    if (!m_bUIActive) {
      m_bUIActive = TRUE;
      hr = m_spInPlaceSite->OnUIActivate();
      if (FAILED(hr)) {
        return hr;
      }
      // set ourselves up in the host
      if (in_place_active_object) {
        if (in_place_frame_) {
          in_place_frame_->SetActiveObject(in_place_active_object, NULL);
        }
        if (in_place_ui_window) {
          in_place_ui_window->SetActiveObject(in_place_active_object, NULL);
        }
      }
    }
  }
  m_spClientSite->ShowObject();
  return S_OK;
}

void ChromeActiveDocument::OnNavigationStateChanged(int tab_handle, int flags,
    const IPC::NavigationInfo& nav_info) {
  // TODO(joshia): handle INVALIDATE_TAB,INVALIDATE_LOAD etc.
  DLOG(INFO) << __FUNCTION__ << std::endl << " Flags: " << flags
      << "Url: " << nav_info.url <<
      ", Title: " << nav_info.title <<
      ", Type: " << nav_info.navigation_type << ", Relative Offset: " <<
      nav_info.relative_offset << ", Index: " << nav_info.navigation_index;

  UpdateNavigationState(nav_info);
}

void ChromeActiveDocument::OnUpdateTargetUrl(int tab_handle,
    const std::wstring& new_target_url) {
  if (in_place_frame_) {
    in_place_frame_->SetStatusText(new_target_url.c_str());
  }
}

bool IsFindAccelerator(const MSG& msg) {
  // TODO(robertshield): This may not stand up to localization. Fix if this
  // is the case.
  return msg.message == WM_KEYDOWN && msg.wParam == 'F' &&
         win_util::IsCtrlPressed() &&
         !(win_util::IsAltPressed() || win_util::IsShiftPressed());
}

void ChromeActiveDocument::OnAcceleratorPressed(int tab_handle,
                                                const MSG& accel_message) {
  if (::TranslateAccelerator(m_hWnd, accelerator_table_,
                             const_cast<MSG*>(&accel_message)))
    return;

  bool handled_accel = false;
  if (in_place_frame_ != NULL) {
    handled_accel = (S_OK == in_place_frame_->TranslateAcceleratorW(
        const_cast<MSG*>(&accel_message), 0));
  }

  if (!handled_accel) {
    if (IsFindAccelerator(accel_message)) {
      // Handle the showing of the find dialog explicitly.
      OnFindInPage();
    } else {
      Base::OnAcceleratorPressed(tab_handle, accel_message);
    }
  } else {
    DLOG(INFO) << "IE handled accel key " << accel_message.wParam;
  }
}

void ChromeActiveDocument::OnTabbedOut(int tab_handle, bool reverse) {
  DLOG(INFO) << __FUNCTION__;
  if (in_place_frame_) {
    MSG msg = { NULL, WM_KEYDOWN, VK_TAB };
    in_place_frame_->TranslateAcceleratorW(&msg, 0);
  }
}

void ChromeActiveDocument::OnDidNavigate(int tab_handle,
                                         const IPC::NavigationInfo& nav_info) {
  DLOG(INFO) << __FUNCTION__ << std::endl << "Url: " << nav_info.url <<
      ", Title: " << nav_info.title <<
      ", Type: " << nav_info.navigation_type << ", Relative Offset: " <<
      nav_info.relative_offset << ", Index: " << nav_info.navigation_index;

  // This could be NULL if the active document instance is being destroyed.
  if (!m_spInPlaceSite) {
    DLOG(INFO) << __FUNCTION__ << "m_spInPlaceSite is NULL. Returning";
    return;
  }

  UpdateNavigationState(nav_info);
}

void ChromeActiveDocument::UpdateNavigationState(
    const IPC::NavigationInfo& new_navigation_info) {
  HRESULT hr = S_OK;
  bool is_title_changed = (navigation_info_.title != new_navigation_info.title);
  bool is_ssl_state_changed =
      (navigation_info_.security_style != new_navigation_info.security_style) ||
      (navigation_info_.has_mixed_content !=
          new_navigation_info.has_mixed_content);

  if (is_ssl_state_changed) {
    int lock_status = SECURELOCK_SET_UNSECURE;
    switch (new_navigation_info.security_style) {
      case SECURITY_STYLE_AUTHENTICATION_BROKEN:
        lock_status = SECURELOCK_SET_SECUREUNKNOWNBIT;
        break;
      case SECURITY_STYLE_AUTHENTICATED:
        lock_status = new_navigation_info.has_mixed_content ?
            SECURELOCK_SET_MIXED : SECURELOCK_SET_SECUREUNKNOWNBIT;
        break;
      default:
        break;
    }

    ScopedVariant secure_lock_status(lock_status);
    IEExec(&CGID_ShellDocView, INTERNAL_CMDID_SET_SSL_LOCK,
           OLECMDEXECOPT_DODEFAULT, secure_lock_status.AsInput(), NULL);
  }

  // Ideally all navigations should come to Chrome Frame so that we can call
  // BeforeNavigate2 on installed BHOs and give them a chance to cancel the
  // navigation. However, in practice what happens is as below:
  // The very first navigation that happens in CF happens via a Load or a
  // LoadHistory call. In this case, IE already has the correct information for
  // its travel log as well address bar. For other internal navigations (navs
  // that only happen within Chrome such as anchor navigations) we need to
  // update IE's internal state after the fact. In the case of internal
  // navigations, we notify the BHOs but ignore the should_cancel flag.

  // Another case where we need to issue BeforeNavigate2 calls is as below:-
  // We get notified after the fact, when navigations are initiated within
  // Chrome via window.open calls. These navigations are handled by creating
  // an external tab container within chrome and then connecting to it from IE.
  // We still want to update the address bar/history, etc, to ensure that
  // the special URL used by Chrome to indicate this is updated correctly.
  bool is_internal_navigation = ((new_navigation_info.navigation_index > 0) &&
      (new_navigation_info.navigation_index !=
       navigation_info_.navigation_index)) ||
       StartsWith(static_cast<BSTR>(url_), kChromeAttachExternalTabPrefix,
                  false);

  if (new_navigation_info.url.is_valid()) {
    url_.Allocate(UTF8ToWide(new_navigation_info.url.spec()).c_str());
  }

  if (is_internal_navigation) {
    ScopedComPtr<IDocObjectService> doc_object_svc;
    ScopedComPtr<IWebBrowserEventsService> web_browser_events_svc;
    DoQueryService(__uuidof(web_browser_events_svc), m_spClientSite,
                   web_browser_events_svc.Receive());
    // web_browser_events_svc can be NULL on IE6.
    if (web_browser_events_svc) {
      VARIANT_BOOL should_cancel = VARIANT_FALSE;
      web_browser_events_svc->FireBeforeNavigate2Event(&should_cancel);
    }

    // We need to tell IE that we support navigation so that IE will query us
    // for IPersistHistory and call GetPositionCookie to save our navigation
    // index.
    ScopedVariant html_window(static_cast<IUnknown*>(
        static_cast<IHTMLWindow2*>(this)));
    IEExec(&CGID_DocHostCmdPriv, DOCHOST_DOCCANNAVIGATE, 0,
           html_window.AsInput(), NULL);

    // We pass the HLNF_INTERNALJUMP flag to INTERNAL_CMDID_FINALIZE_TRAVEL_LOG
    // since we want to make IE treat all internal navigations within this page
    // (including anchor navigations and subframe navigations) as anchor
    // navigations. This will ensure that IE calls GetPositionCookie
    // to save the current position cookie in the travel log and then call
    // SetPositionCookie when the user hits Back/Forward to come back here.
    ScopedVariant internal_navigation(HLNF_INTERNALJUMP);
    IEExec(&CGID_Explorer, INTERNAL_CMDID_FINALIZE_TRAVEL_LOG, 0,
           internal_navigation.AsInput(), NULL);

    // We no longer need to lie to IE. If we lie persistently to IE, then
    // IE reuses us for new navigations.
    IEExec(&CGID_DocHostCmdPriv, DOCHOST_DOCCANNAVIGATE, 0, NULL, NULL);

    if (doc_object_svc) {
      // Now call the FireNavigateCompleteEvent which makes IE update the text
      // in the address-bar.
      doc_object_svc->FireNavigateComplete2(this, 0);
      doc_object_svc->FireDocumentComplete(this, 0);
    } else if (web_browser_events_svc) {
      web_browser_events_svc->FireNavigateComplete2Event();
      web_browser_events_svc->FireDocumentCompleteEvent();
    }
  }

  if (is_title_changed) {
    ScopedVariant title(new_navigation_info.title.c_str());
    IEExec(NULL, OLECMDID_SETTITLE, OLECMDEXECOPT_DONTPROMPTUSER,
           title.AsInput(), NULL);
  }

  // It is important that we only update the navigation_info_ after we have
  // finalized the travel log. This is because IE will ask for information
  // such as navigation index when the travel log is finalized and we need
  // supply the old index and not the new one.
  navigation_info_ = new_navigation_info;
  // Update the IE zone here. Ideally we would like to do it when the active
  // document is activated. However that does not work at times as the frame we
  // get there is not the actual frame which handles the command.
  IEExec(&CGID_Explorer, SBCMDID_MIXEDZONE, 0, NULL, NULL);
}

void ChromeActiveDocument::OnFindInPage() {
  TabProxy* tab = GetTabProxy();
  if (tab) {
    if (!find_dialog_.IsWindow()) {
      find_dialog_.Create(m_hWnd);
    }

    find_dialog_.ShowWindow(SW_SHOW);
  }
}

void ChromeActiveDocument::OnViewSource() {
  DCHECK(navigation_info_.url.is_valid());
  std::string url_to_open = "view-source:";
  url_to_open += navigation_info_.url.spec();
  OnOpenURL(0, GURL(url_to_open), GURL(), NEW_WINDOW);
}

void ChromeActiveDocument::OnDetermineSecurityZone(const GUID* cmd_group_guid,
                                                   DWORD command_id,
                                                   DWORD cmd_exec_opt,
                                                   VARIANT* in_args,
                                                   VARIANT* out_args) {
  if (out_args != NULL) {
    out_args->vt = VT_UI4;
    out_args->ulVal = URLZONE_INTERNET;
  }
}

void ChromeActiveDocument::OnOpenURL(int tab_handle,
                                     const GURL& url_to_open,
                                     const GURL& referrer,
                                     int open_disposition) {
  // If the disposition indicates that we should be opening the URL in the
  // current tab, then we can reuse the ChromeFrameAutomationClient instance
  // maintained by the current ChromeActiveDocument instance. We cache this
  // instance so that it can be used by the new ChromeActiveDocument instance
  // which may be instantiated for handling the new URL.
  if (open_disposition == CURRENT_TAB) {
    // Grab a reference to ensure that the document remains valid.
    AddRef();
    g_active_doc_cache.Set(this);
  }

  Base::OnOpenURL(tab_handle, url_to_open, referrer, open_disposition);
}

bool ChromeActiveDocument::PreProcessContextMenu(HMENU menu) {
  ScopedComPtr<IBrowserService> browser_service;
  ScopedComPtr<ITravelLog> travel_log;
  GetBrowserServiceAndTravelLog(browser_service.Receive(),
                                travel_log.Receive());
  if (!browser_service || !travel_log)
    return true;

  if (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_BACK, NULL))) {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_BACK, MF_BYCOMMAND | MF_ENABLED);
  } else {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_BACK, MF_BYCOMMAND | MFS_DISABLED);
  }

  if (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_FORE, NULL))) {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_FORWARD,
                   MF_BYCOMMAND | MF_ENABLED);
  } else {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_FORWARD,
                   MF_BYCOMMAND | MFS_DISABLED);
  }

  // Call base class (adds 'About' item)
  return Base::PreProcessContextMenu(menu);
}

bool ChromeActiveDocument::HandleContextMenuCommand(UINT cmd,
    const IPC::ContextMenuParams& params) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());

  switch (cmd) {
    case IDS_CONTENT_CONTEXT_BACK:
      web_browser2->GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      web_browser2->GoForward();
      break;

    case IDS_CONTENT_CONTEXT_RELOAD:
      web_browser2->Refresh();
      break;

    default:
      return Base::HandleContextMenuCommand(cmd, params);
  }

  return true;
}

HRESULT ChromeActiveDocument::IEExec(const GUID* cmd_group_guid,
                                     DWORD command_id, DWORD cmd_exec_opt,
                                     VARIANT* in_args, VARIANT* out_args) {
  HRESULT hr = E_FAIL;

  ScopedComPtr<IOleCommandTarget> frame_cmd_target;

  ScopedComPtr<IOleInPlaceSite> in_place_site(m_spInPlaceSite);
  if (!in_place_site.get() && m_spClientSite != NULL) {
    in_place_site.QueryFrom(m_spClientSite);
  }

  if (in_place_site)
    hr = frame_cmd_target.QueryFrom(in_place_site);

  if (frame_cmd_target)
    hr = frame_cmd_target->Exec(cmd_group_guid, command_id, cmd_exec_opt,
                                in_args, out_args);

  return hr;
}

bool ChromeActiveDocument::IsUrlZoneRestricted(const std::wstring& url) {
  if (security_manager_.get() == NULL) {
    HRESULT hr = CoCreateInstance(
        CLSID_InternetSecurityManager,
        NULL,
        CLSCTX_ALL,
        IID_IInternetSecurityManager,
        reinterpret_cast<void**>(security_manager_.Receive()));

    if (FAILED(hr)) {
      NOTREACHED() << __FUNCTION__
                   << " Failed to create InternetSecurityManager. Error: 0x%x"
                   << hr;
      return true;
    }
  }

  DWORD zone = URLZONE_UNTRUSTED;
  security_manager_->MapUrlToZone(url.c_str(), &zone, 0);
  return zone == URLZONE_UNTRUSTED;
}

bool ChromeActiveDocument::ParseUrl(const std::wstring& url,
                                    bool* is_new_navigation,
                                    bool* is_chrome_protocol,
                                    std::wstring* parsed_url) {
  if (!is_new_navigation || !is_chrome_protocol|| !parsed_url) {
    NOTREACHED() << __FUNCTION__ << " Invalid arguments";
    return false;
  }

  std::wstring initial_url = url;

  *is_chrome_protocol = StartsWith(initial_url, kChromeProtocolPrefix,
                                   false);

  *is_new_navigation = true;

  if (*is_chrome_protocol) {
    initial_url.erase(0, lstrlen(kChromeProtocolPrefix));
    *is_new_navigation =
        !StartsWith(initial_url, kChromeAttachExternalTabPrefix, false);
  }

  if (!IsValidUrlScheme(initial_url, is_privileged_)) {
    DLOG(WARNING) << __FUNCTION__ << " Disallowing navigation to url: "
                  << url;
    return false;
  }

  if (IsUrlZoneRestricted(initial_url)) {
    DLOG(WARNING) << __FUNCTION__
                  << " Disallowing navigation to restricted url: "
                  << initial_url;
    return false;
  }

  if (*is_chrome_protocol) {
    // Allow chrome protocol (gcf:) if -
    // - explicitly enabled using registry
    // - for gcf:attach_external_tab
    // - for gcf:about and gcf:view-source
    GURL crack_url(initial_url);
    bool allow_gcf_protocol = !*is_new_navigation ||
        GetConfigBool(false, kEnableGCFProtocol) ||
        crack_url.SchemeIs(chrome::kAboutScheme) ||
        crack_url.SchemeIs(chrome::kViewSourceScheme);
    if (!allow_gcf_protocol)
      return false;
  }

  *parsed_url = initial_url;
  return true;
}

bool ChromeActiveDocument::LaunchUrl(const std::wstring& url,
                                     bool is_new_navigation) {
  url_.Reset(::SysAllocString(url.c_str()));

  if (!is_new_navigation) {
    WStringTokenizer tokenizer(url, L"&");
    // Skip over kChromeAttachExternalTabPrefix
    tokenizer.GetNext();

    intptr_t external_tab_cookie = 0;

    if (tokenizer.GetNext())
      StringToInt(tokenizer.token(),
                  reinterpret_cast<int*>(&external_tab_cookie));

    if (external_tab_cookie == 0) {
      NOTREACHED() << "invalid url for attach tab: " << url;
      return false;
    }

    automation_client_->AttachExternalTab(external_tab_cookie);
  } else {
    // Initiate navigation before launching chrome so that the url will be
    // cached and sent with launch settings.
    if (url_.Length()) {
      std::string utf8_url;
      WideToUTF8(url_, url_.Length(), &utf8_url);

      std::string referrer;
      Bho* chrome_frame_bho = Bho::GetCurrentThreadBhoInstance();
      if (chrome_frame_bho)
        referrer = chrome_frame_bho->referrer();

      if (!automation_client_->InitiateNavigation(utf8_url,
                                                  referrer,
                                                  is_privileged_)) {
        DLOG(ERROR) << "Invalid URL: " << url;
        Error(L"Invalid URL");
        url_.Reset();
        return false;
      }

      DLOG(INFO) << "Url is " << url_;
    }
  }

  if (is_automation_client_reused_)
    return true;

  automation_client_->SetUrlFetcher(&url_fetcher_);

  if (InitializeAutomation(GetHostProcessName(false), L"", IsIEInPrivate()))
    return true;

  return false;
}

HRESULT ChromeActiveDocument::SetPageFontSize(const GUID* cmd_group_guid,
                                              DWORD command_id,
                                              DWORD cmd_exec_opt,
                                              VARIANT* in_args,
                                              VARIANT* out_args) {
  if (!automation_client_.get()) {
    NOTREACHED() << "Invalid automtion client";
    return E_FAIL;
  }

  switch (command_id) {
    case IDM_BASELINEFONT1:
      automation_client_->SetPageFontSize(SMALLEST_FONT);
      break;

    case IDM_BASELINEFONT2:
      automation_client_->SetPageFontSize(SMALL_FONT);
      break;

    case IDM_BASELINEFONT3:
      automation_client_->SetPageFontSize(MEDIUM_FONT);
      break;

    case IDM_BASELINEFONT4:
      automation_client_->SetPageFontSize(LARGE_FONT);
      break;

    case IDM_BASELINEFONT5:
      automation_client_->SetPageFontSize(LARGEST_FONT);
      break;

    default:
      NOTREACHED() << "Invalid font size command: "
                  << command_id;
      return E_FAIL;
  }

  // Forward the command back to IEFrame with group set to
  // CGID_ExplorerBarDoc. This is probably needed to update the menu state to
  // indicate that the font size was set. This currently fails with error
  // 0x80040104.
  // TODO(iyengar)
  // Do some investigation into why this Exec call fails.
  IEExec(&CGID_ExplorerBarDoc, command_id, cmd_exec_opt, NULL, NULL);
  return S_OK;
}

void ChromeActiveDocument::OnGoToHistoryEntryOffset(int tab_handle,
                                                    int offset) {
  DLOG(INFO) <<  __FUNCTION__ << " - offset:" << offset;

  ScopedComPtr<IBrowserService> browser_service;
  ScopedComPtr<ITravelLog> travel_log;
  GetBrowserServiceAndTravelLog(browser_service.Receive(),
                                travel_log.Receive());

  if (browser_service && travel_log)
    travel_log->Travel(browser_service, offset);
}

HRESULT ChromeActiveDocument::GetBrowserServiceAndTravelLog(
    IBrowserService** browser_service, ITravelLog** travel_log) {
  DCHECK(browser_service || travel_log);
  ScopedComPtr<IBrowserService> browser_service_local;
  HRESULT hr = DoQueryService(SID_SShellBrowser, m_spClientSite,
                              browser_service_local.Receive());
  if (!browser_service_local) {
    NOTREACHED() << "DoQueryService for IBrowserService failed: " << hr;
    return hr;
  }

  if (travel_log) {
    hr = browser_service_local->GetTravelLog(travel_log);
    DLOG_IF(INFO, !travel_log) << "browser_service->GetTravelLog failed: "
        << hr;
  }

  if (browser_service)
    *browser_service = browser_service_local.Detach();

  return hr;
}

LRESULT ChromeActiveDocument::OnForward(WORD notify_code, WORD id,
                                        HWND control_window,
                                        BOOL& bHandled) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
  DCHECK(web_browser2);

  if (web_browser2) {
    web_browser2->GoForward();
  }
  return 0;
}

LRESULT ChromeActiveDocument::OnBack(WORD notify_code, WORD id,
                                     HWND control_window,
                                     BOOL& bHandled) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
  DCHECK(web_browser2);

  if (web_browser2) {
    web_browser2->GoBack();
  }
  return 0;
}

