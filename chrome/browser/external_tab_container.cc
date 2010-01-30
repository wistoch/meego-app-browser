// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_tab_container.h"

#include <string>

#include "app/l10n_util.h"
#include "app/win_util.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_extension_function.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/tab_contents/provisional_load_details.h"
#include "chrome/browser/views/tab_contents/render_view_context_menu_external_win.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/automation/automation_messages.h"
#include "grit/generated_resources.h"
#include "views/window/window.h"

static const wchar_t kWindowObjectKey[] = L"ChromeWindowObject";

ExternalTabContainer::PendingTabs ExternalTabContainer::pending_tabs_;

ExternalTabContainer::ExternalTabContainer(
    AutomationProvider* automation, AutomationResourceMessageFilter* filter)
    : automation_(automation),
      tab_contents_(NULL),
      tab_contents_container_(NULL),
      tab_handle_(0),
      ignore_next_load_notification_(false),
      automation_resource_message_filter_(filter),
      load_requests_via_automation_(false),
      handle_top_level_requests_(false),
      external_method_factory_(this),
      enabled_extension_automation_(false),
      waiting_for_unload_event_(false),
      pending_(false) {
}

ExternalTabContainer::~ExternalTabContainer() {
  Uninitialize();
}

bool ExternalTabContainer::Init(Profile* profile,
                                HWND parent,
                                const gfx::Rect& bounds,
                                DWORD style,
                                bool load_requests_via_automation,
                                bool handle_top_level_requests,
                                TabContents* existing_contents,
                                const GURL& initial_url,
                                const GURL& referrer) {
  if (IsWindow()) {
    NOTREACHED();
    return false;
  }

  load_requests_via_automation_ = load_requests_via_automation;
  handle_top_level_requests_ = handle_top_level_requests;

  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  views::WidgetWin::Init(NULL, bounds);
  if (!IsWindow()) {
    NOTREACHED();
    return false;
  }

  // TODO(jcampan): limit focus traversal to contents.

  // We don't ever remove the prop because the lifetime of this object
  // is the same as the lifetime of the window
  SetProp(GetNativeView(), kWindowObjectKey, this);

  if (existing_contents) {
    tab_contents_ = existing_contents;
    tab_contents_->controller().set_profile(profile);
  } else {
    tab_contents_ = new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL);
  }

  tab_contents_->set_delegate(this);

  tab_contents_->GetMutableRendererPrefs()->browser_handles_top_level_requests =
      handle_top_level_requests;

  if (!existing_contents) {
    tab_contents_->render_view_host()->AllowBindings(
        BindingsPolicy::EXTERNAL_HOST);
  }

  // Create a TabContentsContainer to handle focus cycling using Tab and
  // Shift-Tab.
  tab_contents_container_ = new TabContentsContainer;
  SetContentsView(tab_contents_container_);

  // Note that SetTabContents must be called after AddChildView is called
  tab_contents_container_->ChangeTabContents(tab_contents_);

  NavigationController* controller = &tab_contents_->controller();
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 Source<TabContents>(tab_contents_));
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_DELETED,
                 Source<TabContents>(tab_contents_));

  NotificationService::current()->Notify(
      NotificationType::EXTERNAL_TAB_CREATED,
      Source<NavigationController>(controller),
      NotificationService::NoDetails());

  // Start loading initial URL
  if (!initial_url.is_empty()) {
    // Navigate out of context since we don't have a 'tab_handle_' yet.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        external_method_factory_.NewRunnableMethod(
            &ExternalTabContainer::Navigate, initial_url, referrer));
  }

  // We need WS_POPUP to be on the window during initialization, but
  // once initialized we apply the requested style which may or may not
  // include the popup bit.
  // Note that it's important to do this before we call SetParent since
  // during the SetParent call we will otherwise get a WA_ACTIVATE call
  // that causes us to steal the current focus.
  SetWindowLong(GWL_STYLE, (GetWindowLong(GWL_STYLE) & ~WS_POPUP) | style);

  // Now apply the parenting and style
  if (parent)
    SetParent(GetNativeView(), parent);

  ::ShowWindow(tab_contents_->GetNativeView(), SW_SHOWNA);

  disabled_context_menu_ids_.push_back(
      IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  LoadAccelerators();
  return true;
}

void ExternalTabContainer::Uninitialize() {
  if (enabled_extension_automation_) {
    AutomationExtensionFunction::Disable();
  }

  registrar_.RemoveAll();
  if (tab_contents_) {
    if (Browser::RunUnloadEventsHelper(tab_contents_)) {
      waiting_for_unload_event_ = true;
      MessageLoop::current()->Run();
      waiting_for_unload_event_ = false;
    }

    RenderViewHost* rvh = tab_contents_->render_view_host();
    if (rvh && DevToolsManager::GetInstance()) {
      DevToolsManager::GetInstance()->UnregisterDevToolsClientHostFor(rvh);
    }

    NotificationService::current()->Notify(
        NotificationType::EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(&tab_contents_->controller()),
        Details<ExternalTabContainer>(this));

    delete tab_contents_;
    tab_contents_ = NULL;
  }

  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->UnregisterAccelerators(this);
  }

  request_context_ = NULL;
}

bool ExternalTabContainer::Reinitialize(
    AutomationProvider* automation_provider,
    AutomationResourceMessageFilter* filter) {
  if (!automation_provider || !filter) {
    NOTREACHED();
    return false;
  }

  automation_ = automation_provider;
  automation_resource_message_filter_ = filter;

  if (load_requests_via_automation_) {
    InitializeAutomationRequestContext(tab_handle_);

    RenderViewHost* rvh = tab_contents_->render_view_host();
    if (rvh) {
      AutomationResourceMessageFilter::ResumePendingRenderView(
          rvh->process()->id(), rvh->routing_id(),
          tab_handle_, automation_resource_message_filter_);
    }
  }

  // We cannot send the navigation state right away as the automation channel
  // may not have been fully setup yet.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      external_method_factory_.NewRunnableMethod(
          &ExternalTabContainer::OnReinitialize));
  return true;
}

void ExternalTabContainer::SetTabHandle(int handle) {
  tab_handle_ = handle;
  if (automation_resource_message_filter_.get() &&
      load_requests_via_automation_) {
    InitializeAutomationRequestContext(tab_handle_);
  }
}

void ExternalTabContainer::ProcessUnhandledAccelerator(const MSG& msg) {
  NativeWebKeyboardEvent keyboard_event(msg.hwnd, msg.message, msg.wParam,
                                        msg.lParam);
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(keyboard_event,
                                                        GetFocusManager());
}

void ExternalTabContainer::FocusThroughTabTraversal(bool reverse) {
  DCHECK(tab_contents_);
  if (tab_contents_) {
    static_cast<TabContents*>(tab_contents_)->Focus();
    static_cast<TabContents*>(tab_contents_)->FocusThroughTabTraversal(reverse);
  }
}

// static
bool ExternalTabContainer::IsExternalTabContainer(HWND window) {
  if (GetProp(window, kWindowObjectKey) != NULL)
    return true;

  return false;
}

// static
ExternalTabContainer* ExternalTabContainer::GetContainerForTab(
    HWND tab_window) {
  HWND parent_window = ::GetParent(tab_window);
  if (!::IsWindow(parent_window)) {
    return NULL;
  }
  if (!IsExternalTabContainer(parent_window)) {
    return NULL;
  }
  ExternalTabContainer* container = reinterpret_cast<ExternalTabContainer*>(
      GetProp(parent_window, kWindowObjectKey));
  return container;
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, TabContentsDelegate implementation:

void ExternalTabContainer::OpenURLFromTab(TabContents* source,
                           const GURL& url,
                           const GURL& referrer,
                           WindowOpenDisposition disposition,
                           PageTransition::Type transition) {
  if (pending()) {
    PendingTopLevelNavigation url_request;
    url_request.disposition = disposition;
    url_request.transition = transition;
    url_request.url = url;
    url_request.referrer = referrer;

    pending_open_url_requests_.push_back(url_request);
    return;
  }

  switch (disposition) {
    case CURRENT_TAB:
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_POPUP:
    case NEW_WINDOW:
    case SAVE_TO_DISK:
      if (automation_) {
        automation_->Send(new AutomationMsg_OpenURL(0, tab_handle_,
                                                    url, referrer,
                                                    disposition));
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExternalTabContainer::NavigationStateChanged(const TabContents* source,
                                                  unsigned changed_flags) {
  if (automation_) {
    IPC::NavigationInfo nav_info;
    if (InitNavigationInfo(&nav_info, NavigationType::NAV_IGNORE, 0))
      automation_->Send(new AutomationMsg_NavigationStateChanged(
          0, tab_handle_, changed_flags, nav_info));
  }
}

void ExternalTabContainer::AddNewContents(TabContents* source,
                            TabContents* new_contents,
                            WindowOpenDisposition disposition,
                            const gfx::Rect& initial_pos,
                            bool user_gesture) {
  DCHECK(automation_ != NULL);

  scoped_refptr<ExternalTabContainer> new_container =
      new ExternalTabContainer(NULL, NULL);

  // Make sure that ExternalTabContainer instance is initialized with
  // an unwrapped Profile.
  bool result = new_container->Init(
      new_contents->profile()->GetOriginalProfile(),
      NULL,
      initial_pos,
      WS_CHILD,
      load_requests_via_automation_,
      handle_top_level_requests_,
      new_contents,
      GURL(),
      GURL());

  if (result) {
    pending_tabs_[reinterpret_cast<intptr_t>(new_container.get())] =
        new_container;

    new_container->set_pending(true);

    RenderViewHost* rvh = new_contents->render_view_host();
    DCHECK(rvh != NULL);
    if (rvh) {
      // Register this render view as a pending render view, i.e. any network
      // requests initiated by this render view would be serviced when the
      // external host connects to the new external tab instance.
      AutomationResourceMessageFilter::RegisterRenderView(
          rvh->process()->id(), rvh->routing_id(),
          tab_handle_, automation_resource_message_filter_,
          true);
    }
    automation_->Send(new AutomationMsg_AttachExternalTab(
        0,
        tab_handle_,
        reinterpret_cast<intptr_t>(new_container.get()),
        disposition));
  } else {
    NOTREACHED();
  }
}

void ExternalTabContainer::ActivateContents(TabContents* contents) {
}

void ExternalTabContainer::LoadingStateChanged(TabContents* source) {
}

void ExternalTabContainer::CloseContents(TabContents* source) {
  if (waiting_for_unload_event_) {
    MessageLoop::current()->Quit();
  }
}

void ExternalTabContainer::MoveContents(TabContents* source,
                                        const gfx::Rect& pos) {
}

bool ExternalTabContainer::IsPopup(TabContents* source) {
  return false;
}

void ExternalTabContainer::URLStarredChanged(TabContents* source,
                                             bool starred) {
}

void ExternalTabContainer::UpdateTargetURL(TabContents* source,
                                           const GURL& url) {
  if (automation_) {
    std::wstring url_string = CA2W(url.spec().c_str());
    automation_->Send(
        new AutomationMsg_UpdateTargetUrl(0, tab_handle_, url_string));
  }
}

void ExternalTabContainer::ContentsZoomChange(bool zoom_in) {
}

void ExternalTabContainer::ToolbarSizeChanged(TabContents* source,
                                              bool finished) {
}

void ExternalTabContainer::ForwardMessageToExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  if (automation_) {
    automation_->Send(
        new AutomationMsg_ForwardMessageToExternalHost(0, tab_handle_,
            message, origin, target));
  }
}

gfx::NativeWindow ExternalTabContainer::GetFrameNativeWindow() {
  return hwnd();
}

bool ExternalTabContainer::TakeFocus(bool reverse) {
  if (automation_) {
    automation_->Send(new AutomationMsg_TabbedOut(0, tab_handle_,
        win_util::IsShiftPressed()));
  }

  return true;
}

bool ExternalTabContainer::CanDownload(int request_id) {
  if (load_requests_via_automation_) {
    if (automation_) {
      // In case the host needs to show UI that needs to take the focus.
      ::AllowSetForegroundWindow(ASFW_ANY);

      ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(automation_resource_message_filter_.get(),
              &AutomationResourceMessageFilter::SendDownloadRequestToHost,
              0, tab_handle_, request_id));
    }
  } else {
    DLOG(WARNING) << "Downloads are only supported with host browser network "
                     "stack enabled.";
  }

  // Never allow downloads.
  return false;
}

void ExternalTabContainer::ShowPageInfo(Profile* profile,
                                        const GURL& url,
                                        const NavigationEntry::SSLStatus& ssl,
                                        bool show_history) {
  browser::ShowPageInfo(GetNativeView(), profile, url, ssl, show_history);
}

bool ExternalTabContainer::HandleContextMenu(const ContextMenuParams& params) {
  if (!automation_) {
    NOTREACHED();
    return false;
  }

  external_context_menu_.reset(
      new RenderViewContextMenuExternalWin(tab_contents(),
                                           params,
                                           disabled_context_menu_ids_));
  external_context_menu_->Init();

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_pt, 1);

  IPC::ContextMenuParams ipc_params;
  ipc_params.screen_x = screen_pt.x;
  ipc_params.screen_y = screen_pt.y;
  ipc_params.link_url = params.link_url;
  ipc_params.unfiltered_link_url = params.unfiltered_link_url;
  ipc_params.src_url = params.src_url;
  ipc_params.page_url = params.page_url;
  ipc_params.frame_url = params.frame_url;

  bool rtl = l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
  automation_->Send(
      new AutomationMsg_ForwardContextMenuToExternalHost(0, tab_handle_,
          external_context_menu_->GetMenuHandle(),
          rtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN, ipc_params));

  return true;
}

bool ExternalTabContainer::ExecuteContextMenuCommand(int command) {
  if (!external_context_menu_.get()) {
    NOTREACHED();
    return false;
  }

  switch (command) {
    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      NOTREACHED();  // Should be handled in host.
      break;
    }
  }

  external_context_menu_->ExecuteCommand(command);
  return true;
}

bool ExternalTabContainer::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  return false;
}

void ExternalTabContainer::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  ProcessUnhandledKeyStroke(event.os_event.hwnd, event.os_event.message,
                            event.os_event.wParam, event.os_event.lParam);
}

void ExternalTabContainer::ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                                          gfx::NativeWindow parent_window) {
  if (!browser_.get()) {
    browser_.reset(Browser::CreateForPopup(tab_contents_->profile()));
  }

  gfx::NativeWindow parent = parent_window ? parent_window
                                           : GetParent();
  browser_->window()->ShowHTMLDialog(delegate, parent);
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, NotificationObserver implementation:

void ExternalTabContainer::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (!automation_)
    return;

  static const int kHttpClientErrorStart = 400;
  static const int kHttpServerErrorEnd = 510;

  switch (type.value) {
    case NotificationType::LOAD_STOP: {
        const LoadNotificationDetails* load =
            Details<LoadNotificationDetails>(details).ptr();
        if (load != NULL && PageTransition::IsMainFrame(load->origin())) {
          automation_->Send(new AutomationMsg_TabLoaded(0, tab_handle_,
                                                        load->url()));
        }
        break;
      }
    case NotificationType::NAV_ENTRY_COMMITTED: {
        if (ignore_next_load_notification_) {
          ignore_next_load_notification_ = false;
          return;
        }

        const NavigationController::LoadCommittedDetails* commit =
            Details<NavigationController::LoadCommittedDetails>(details).ptr();

        if (commit->http_status_code >= kHttpClientErrorStart &&
            commit->http_status_code <= kHttpServerErrorEnd) {
          automation_->Send(new AutomationMsg_NavigationFailed(
              0, tab_handle_, commit->http_status_code, commit->entry->url()));

          ignore_next_load_notification_ = true;
        } else {
          IPC::NavigationInfo navigation_info;
          // When the previous entry index is invalid, it will be -1, which
          // will still make the computation come out right (navigating to the
          // 0th entry will be +1).
          if (InitNavigationInfo(&navigation_info, commit->type,
                  commit->previous_entry_index -
                  tab_contents_->controller().last_committed_entry_index()))
            automation_->Send(new AutomationMsg_DidNavigate(0, tab_handle_,
                                                            navigation_info));
        }
        break;
      }
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR: {
      const ProvisionalLoadDetails* load_details =
          Details<ProvisionalLoadDetails>(details).ptr();
      automation_->Send(new AutomationMsg_NavigationFailed(
          0, tab_handle_, load_details->error_code(), load_details->url()));

      ignore_next_load_notification_ = true;
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = Details<RenderViewHost>(details).ptr();
        if (rvh) {
          AutomationResourceMessageFilter::RegisterRenderView(
              rvh->process()->id(), rvh->routing_id(),
              tab_handle_, automation_resource_message_filter_, false);
        }
      }
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_DELETED: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = Details<RenderViewHost>(details).ptr();
        if (rvh) {
          AutomationResourceMessageFilter::UnRegisterRenderView(
              rvh->process()->id(), rvh->routing_id());
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, views::WidgetWin overrides:

LRESULT ExternalTabContainer::OnCreate(LPCREATESTRUCT create_struct) {
  LRESULT result = views::WidgetWin::OnCreate(create_struct);
  if (result == 0) {
    // Grab a reference here which will be released in OnFinalMessage
    AddRef();
  }
  return result;
}

void ExternalTabContainer::OnDestroy() {
  Uninitialize();
  WidgetWin::OnDestroy();
  if (browser_.get()) {
    ::DestroyWindow(browser_->window()->GetNativeHandle());
  }
}

void ExternalTabContainer::OnFinalMessage(HWND window) {
  // Release the reference which we grabbed in WM_CREATE.
  Release();
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, private:
bool ExternalTabContainer::ProcessUnhandledKeyStroke(HWND window,
                                                     UINT message,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  if (!automation_) {
    return false;
  }
  if ((wparam == VK_TAB) && !win_util::IsCtrlPressed()) {
    // Tabs are handled separately (except if this is Ctrl-Tab or
    // Ctrl-Shift-Tab)
    return false;
  }

  unsigned int flags = HIWORD(lparam);
  bool alt = (flags & KF_ALTDOWN) != 0;
  if (!alt && (message == WM_SYSKEYUP || message == WM_KEYUP)) {
    // In case the Alt key is being released.
    alt = (wparam == VK_MENU);
  }

  if ((flags & KF_EXTENDED) || alt || (wparam >= VK_F1 && wparam <= VK_F24) ||
      wparam == VK_ESCAPE || wparam == VK_RETURN ||
      win_util::IsShiftPressed() || win_util::IsCtrlPressed()) {
    // If this is an extended key or if one or more of Alt, Shift and Control
    // are pressed, this might be an accelerator that the external host wants
    // to handle. If the host does not handle this accelerator, it will reflect
    // the accelerator back to us via the ProcessUnhandledAccelerator method.
    MSG msg = {0};
    msg.hwnd = window;
    msg.message = message;
    msg.wParam = wparam;
    msg.lParam = lparam;
    automation_->Send(new AutomationMsg_HandleAccelerator(0, tab_handle_, msg));
    return true;
  }

  return false;
}

bool ExternalTabContainer::InitNavigationInfo(IPC::NavigationInfo* nav_info,
                                              NavigationType::Type nav_type,
                                              int relative_offset) {
  DCHECK(nav_info);
  NavigationEntry* entry = tab_contents_->controller().GetActiveEntry();
  // If this is very early in the game then we may not have an entry.
  if (!entry)
    return false;

  nav_info->navigation_type = nav_type;
  nav_info->relative_offset = relative_offset;
  nav_info->navigation_index =
      tab_contents_->controller().GetCurrentEntryIndex();
  nav_info->url = entry->url();
  nav_info->title =  UTF16ToWideHack(entry->title());
  if (nav_info->title.empty())
    nav_info->title = UTF8ToWide(nav_info->url.spec());

  nav_info->security_style = entry->ssl().security_style();
  nav_info->has_mixed_content = entry->ssl().has_mixed_content();
  return true;
}

ExternalTabContainer* ExternalTabContainer::RemovePendingTab(intptr_t cookie) {
  PendingTabs::iterator index = pending_tabs_.find(cookie);
  if (index != pending_tabs_.end()) {
    scoped_refptr<ExternalTabContainer> container = (*index).second;
    pending_tabs_.erase(index);
    return container.release();
  }

  NOTREACHED() << "Failed to find ExternalTabContainer for cookie: "
               << cookie;
  return NULL;
}

void ExternalTabContainer::SetEnableExtensionAutomation(
    const std::vector<std::string>& functions_enabled) {
  if (functions_enabled.size() > 0) {
    if (!tab_contents_) {
      NOTREACHED() << "Being invoked via tab so should have TabContents";
      return;
    }

    AutomationExtensionFunction::Enable(tab_contents_, functions_enabled);
    enabled_extension_automation_ = true;
  } else {
    AutomationExtensionFunction::Disable();
    enabled_extension_automation_ = false;
  }
}

// ExternalTabContainer instances do not have a window.
views::Window* ExternalTabContainer::GetWindow() {
  return NULL;
}

bool ExternalTabContainer::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  std::map<views::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());

  if (!tab_contents_ || !tab_contents_->render_view_host()) {
    NOTREACHED();
    return false;
  }

  int command_id = iter->second;
  switch (command_id) {
    case IDC_ZOOM_PLUS:
      tab_contents_->render_view_host()->Zoom(PageZoom::ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      tab_contents_->render_view_host()->Zoom(PageZoom::RESET);
      break;
    case IDC_ZOOM_MINUS:
      tab_contents_->render_view_host()->Zoom(PageZoom::ZOOM_OUT);
      break;
    case IDC_DEV_TOOLS:
      DevToolsManager::GetInstance()->ToggleDevToolsWindow(
          tab_contents_->render_view_host(), false);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      DevToolsManager::GetInstance()->ToggleDevToolsWindow(
          tab_contents_->render_view_host(), true);
      break;
    default:
      NOTREACHED() << "Unsupported accelerator: " << command_id;
      return false;
  }
  return true;
}

void ExternalTabContainer::Navigate(const GURL& url, const GURL& referrer) {
  if (!tab_contents_) {
    NOTREACHED();
    return;
  }

  tab_contents_->controller().LoadURL(url, referrer,
                                      PageTransition::START_PAGE);
}

bool ExternalTabContainer::OnGoToEntryOffset(int offset) {
  if (load_requests_via_automation_) {
    automation_->Send(new AutomationMsg_RequestGoToHistoryEntryOffset(
        0, tab_handle_, offset));
    return false;
  }

  return true;
}

void ExternalTabContainer::InitializeAutomationRequestContext(
    int tab_handle) {
  request_context_ =
      AutomationRequestContext::CreateAutomationURLRequestContextForTab(
          tab_handle, tab_contents_->profile(),
          automation_resource_message_filter_);

  DCHECK(request_context_.get() != NULL);
  tab_contents_->set_request_context(request_context_.get());
}

void ExternalTabContainer::LoadAccelerators() {
  HACCEL accelerator_table = AtlLoadAccelerators(IDR_CHROMEFRAME);
  DCHECK(accelerator_table);

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return;
  }

  scoped_ptr<ACCEL> scoped_accelerators(new ACCEL[count]);
  ACCEL* accelerators = scoped_accelerators.get();
  DCHECK(accelerators != NULL);

  CopyAcceleratorTable(accelerator_table, accelerators, count);

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's fill our own accelerator table.
  for (int i = 0; i < count; ++i) {
    bool alt_down = (accelerators[i].fVirt & FALT) == FALT;
    bool ctrl_down = (accelerators[i].fVirt & FCONTROL) == FCONTROL;
    bool shift_down = (accelerators[i].fVirt & FSHIFT) == FSHIFT;
    views::Accelerator accelerator(
        static_cast<base::KeyboardCode>(accelerators[i].key),
        shift_down, ctrl_down, alt_down);
    accelerator_table_[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }
}

void ExternalTabContainer::OnReinitialize() {
  NavigationStateChanged(tab_contents_, 0);
  ServicePendingOpenURLRequests();
}

void ExternalTabContainer::ServicePendingOpenURLRequests() {
  DCHECK(pending());

  set_pending(false);

  for (size_t index = 0; index < pending_open_url_requests_.size();
       index ++) {
    const PendingTopLevelNavigation& url_request =
        pending_open_url_requests_[index];
    OpenURLFromTab(tab_contents_, url_request.url, url_request.referrer,
                   url_request.disposition, url_request.transition);
  }
  pending_open_url_requests_.clear();
}

