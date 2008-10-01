// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/gfx/point.h"
#include "base/stats_counters.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"

static StatsCounter windowless_queue(L"Plugin.ThrottleQueue");

static const wchar_t kNativeWindowClassName[] = L"NativeWindowClass";
static const wchar_t kWebPluginDelegateProperty[] =
    L"WebPluginDelegateProperty";
static const wchar_t kPluginNameAtomProperty[] = L"PluginNameAtom";
static const wchar_t kDummyActivationWindowName[] = L"DummyWindowForActivation";
static const wchar_t kPluginOrigProc[] = L"OriginalPtr";
static const wchar_t kPluginFlashThrottle[] = L"FlashThrottle";

// The fastest we are willing to process WM_USER+1 events for Flash.
// Flash can easily exceed the limits of our CPU if we don't throttle it.
// The throttle has been chosen by testing various delays and compromising
// on acceptable Flash performance and reasonable CPU consumption.
//
// I'd like to make the throttle delay variable, based on the amount of
// time currently required to paint Flash plugins.  There isn't a good
// way to count the time spent in aggregate plugin painting, however, so
// this seems to work well enough.
static const int kFlashWMUSERMessageThrottleDelayMs = 5;

std::list<MSG> WebPluginDelegateImpl::throttle_queue_;

WebPluginDelegateImpl* WebPluginDelegateImpl::current_plugin_instance_ = NULL;

WebPluginDelegateImpl* WebPluginDelegateImpl::Create(
    const std::wstring& filename,
    const std::string& mime_type,
    HWND containing_window) {
  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin.get() == NULL)
    return NULL;

  NPError err = plugin->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin->CreateInstance(mime_type);
  return new WebPluginDelegateImpl(containing_window, instance.get());
}

bool WebPluginDelegateImpl::IsPluginDelegateWindow(HWND window) {
  // We use a buffer that is one char longer than we need to detect cases where
  // kNativeWindowClassName is a prefix of the given window's class name.  It
  // happens that GetClassNameW will just silently truncate the class name to
  // fit into the given buffer.
  wchar_t class_name[arraysize(kNativeWindowClassName) + 1];
  if (!GetClassNameW(window, class_name, arraysize(class_name)))
    return false;
  return wcscmp(class_name, kNativeWindowClassName) == 0;
}

bool WebPluginDelegateImpl::GetPluginNameFromWindow(
    HWND window, std::wstring *plugin_name) {
  if (NULL == plugin_name) {
    return false;
  }
  if (!IsPluginDelegateWindow(window)) {
    return false;
  }
  ATOM plugin_name_atom = reinterpret_cast<ATOM>(
      GetPropW(window, kPluginNameAtomProperty));
  if (plugin_name_atom != 0) {
    WCHAR plugin_name_local[MAX_PATH] = {0};
    GlobalGetAtomNameW(plugin_name_atom,
                       plugin_name_local,
                       ARRAYSIZE(plugin_name_local));
    *plugin_name = plugin_name_local;
    return true;
  }
  return false;
}

bool WebPluginDelegateImpl::IsDummyActivationWindow(HWND window) {
  if (!IsWindow(window))
    return false;

  wchar_t window_title[MAX_PATH + 1] = {0};
  if (GetWindowText(window, window_title, arraysize(window_title))) {
    return (0 == lstrcmpiW(window_title, kDummyActivationWindowName));
  }
  return false;
}

LRESULT CALLBACK WebPluginDelegateImpl::HandleEventMessageFilterHook(
    int code, WPARAM wParam, LPARAM lParam) {

  DCHECK(current_plugin_instance_);
  current_plugin_instance_->OnModalLoopEntered();
  return CallNextHookEx(NULL, code, wParam, lParam);
}

WebPluginDelegateImpl::WebPluginDelegateImpl(
    HWND containing_window,
    NPAPI::PluginInstance *instance)
    : parent_(containing_window),
      instance_(instance),
      quirks_(0),
      plugin_(NULL),
      windowless_(false),
      windowed_handle_(NULL),
      windowed_did_set_window_(false),
      windowless_needs_set_window_(true),
      plugin_wnd_proc_(NULL),
      last_message_(0),
      is_calling_wndproc(false),
      initial_plugin_resize_done_(false),
      dummy_window_for_activation_(NULL),
      handle_event_message_filter_hook_(NULL),
      handle_event_pump_messages_event_(NULL),
      handle_event_depth_(0),
      user_gesture_message_posted_(false),
#pragma warning(suppress: 4355)  // can use this
      user_gesture_msg_factory_(this),
      load_manually_(false),
      first_geometry_update_(true) {
  memset(&window_, 0, sizeof(window_));

  const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
  std::wstring filename = file_util::GetFilenameFromPath(plugin_info.file);

  if (instance_->mime_type() == "application/x-shockwave-flash" ||
      filename == L"npswf32.dll") {
    // Flash only requests windowless plugins if we return a Mozilla user
    // agent.
    instance_->set_use_mozilla_user_agent();
    quirks_ |= PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE;
  } else if (plugin_info.name.find(L"Windows Media Player") !=
             std::wstring::npos) {
    // Windows Media Player needs two NPP_SetWindow calls.
    quirks_ |= PLUGIN_QUIRK_SETWINDOW_TWICE;
  } else if (instance_->mime_type() == "audio/x-pn-realaudio-plugin" ||
             filename == L"nppl3260.dll") {
    quirks_ |= PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY;
  } else if (plugin_info.name.find(L"VLC Multimedia Plugin") !=
             std::wstring::npos) {
    // VLC hangs on NPP_Destroy if we call NPP_SetWindow with a null window
    // handle
    quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;
    // VLC 0.8.6d and 0.8.6e crash if multiple instances are created.
    quirks_ |= PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES;
  }
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  if (::IsWindow(dummy_window_for_activation_)) {
    ::DestroyWindow(dummy_window_for_activation_);
  }

  DestroyInstance();

  if (!windowless_)
    WindowedDestroyWindow();

  if (handle_event_pump_messages_event_) {
    CloseHandle(handle_event_pump_messages_event_);
  }
}

void WebPluginDelegateImpl::PluginDestroyed() {
  delete this;
}

bool WebPluginDelegateImpl::Initialize(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebPlugin* plugin,
                                       bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin);
  NPAPI::PluginInstance* old_instance =
      NPAPI::PluginInstance::SetInitializingInstance(instance_);

  if (quirks_ & PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES) {
    NPAPI::PluginLib* plugin_lib = instance()->plugin_lib();
    if (plugin_lib->instance_count() > 1) {
      return false;
    }
  }

  bool start_result = instance_->Start(url, argn, argv, argc, load_manually);

  NPAPI::PluginInstance::SetInitializingInstance(old_instance);

  if (!start_result)
    return false;

  windowless_ = instance_->windowless();
  if (windowless_) {
    // For windowless plugins we should set the containing window handle
    // as the instance window handle. This is what Safari does. Not having
    // a valid window handle causes subtle bugs with plugins which retreive
    // the window handle and validate the same. The window handle can be
    // retreived via NPN_GetValue of NPNVnetscapeWindow.
    instance_->set_window_handle(parent_);
    CreateDummyWindowForActivation();
    handle_event_pump_messages_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  } else {
    if (!WindowedCreatePlugin())
      return false;
  }

  plugin->SetWindow(windowed_handle_, handle_event_pump_messages_event_);

  load_manually_ = load_manually;
  plugin_url_ = url.spec();
  return true;
}

void WebPluginDelegateImpl::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    window_.window = NULL;
    if (!(quirks_ & PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY)) {
      instance_->NPP_SetWindow(&window_);
    }

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }
}

void WebPluginDelegateImpl::UpdateGeometry(const gfx::Rect& window_rect,
                                           const gfx::Rect& clip_rect,
                                           bool visible) {
  if (windowless_) {
    WindowlessUpdateGeometry(window_rect, clip_rect);
  } else {
    WindowedUpdateGeometry(window_rect, clip_rect, visible);
  }

  // Initiate a download on the plugin url. This should be done for the
  // first update geometry sequence.
  if (first_geometry_update_) {
    first_geometry_update_ = false;
    // An empty url corresponds to an EMBED tag with no src attribute.
    if (!load_manually_ && !plugin_url_.empty()) {
      instance_->SendStream(plugin_url_.c_str(), false, NULL);
    }
  }
}

void WebPluginDelegateImpl::Paint(HDC hdc, const gfx::Rect& rect) {
  if (windowless_)
    WindowlessPaint(hdc, rect);
}

void WebPluginDelegateImpl::Print(HDC hdc) {
  // Disabling the call to NPP_Print as it causes a crash in
  // flash in some cases. In any case this does not work as expected
  // as the EMF meta file dc passed in needs to be created with the
  // the plugin window dc as its sibling dc and the window rect
  // in .01 mm units.
#if 0
  NPPrint npprint;
  npprint.mode = NP_EMBED;
  npprint.print.embedPrint.platformPrint = reinterpret_cast<void*>(hdc);
  npprint.print.embedPrint.window = window_;
  instance()->NPP_Print(&npprint);
#endif
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(NPReason reason) {
  instance()->DidFinishLoadWithReason(reason);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return ::GetCurrentProcessId();
}

void WebPluginDelegateImpl::SendJavaScriptStream(const std::string& url,
                                                 const std::wstring& result,
                                                 bool success,
                                                 bool notify_needed,
                                                 int notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegateImpl::DidReceiveManualResponse(
    const std::string& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegateImpl::DidReceiveManualData(const char* buffer,
                                                 int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegateImpl::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegateImpl::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

std::wstring WebPluginDelegateImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().file;
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NPEvent evt;
  evt.event = PluginInstallerImpl::kInstallMissingPluginMessage;
  evt.lParam = 0;
  evt.wParam = 0;
  instance()->NPP_HandleEvent(&evt);
}

void WebPluginDelegateImpl::WindowedUpdateGeometry(const gfx::Rect& window_rect,
                                                   const gfx::Rect& clip_rect,
                                                   bool visible) {
  if (WindowedReposition(window_rect, clip_rect, visible) ||
      !windowed_did_set_window_) {
    // Let the plugin know that it has been moved
    WindowedSetWindow();
  }
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);

  RegisterNativeWindowClass();

  // The window will be sized and shown later.
  windowed_handle_ = CreateWindowEx(
    WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
    kNativeWindowClassName,
    0,
    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
    0,
    0,
    0,
    0,
    parent_,
    0,
    GetModuleHandle(NULL),
    0);
  if (windowed_handle_ == 0)
    return false;

  BOOL result = SetProp(windowed_handle_, kWebPluginDelegateProperty, this);
  DCHECK(result == TRUE) << "SetProp failed, last error = " << GetLastError();
  // Get the name of the plugin, create an atom and set that in a window
  // property. Use an atom so that other processes can access the name of
  // the plugin that this window is hosting
  if (instance_ != NULL) {
    NPAPI::PluginLib* plugin_lib = instance()->plugin_lib();
    if (plugin_lib != NULL) {
      std::wstring plugin_name = plugin_lib->plugin_info().name;
      if (!plugin_name.empty()) {
        ATOM plugin_name_atom = GlobalAddAtomW(plugin_name.c_str());
        DCHECK(0 != plugin_name_atom);
        result = SetProp(windowed_handle_,
            kPluginNameAtomProperty,
            reinterpret_cast<HANDLE>(plugin_name_atom));
        DCHECK(result == TRUE) << "SetProp failed, last error = " <<
            GetLastError();
      }
    }
  }

  // Calling SetWindowLongPtrA here makes the window proc ASCII, which is
  // required by at least the Shockwave Director plug-in.
  SetWindowLongPtrA(
      windowed_handle_, GWL_WNDPROC, reinterpret_cast<LONG>(DefWindowProcA));

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (windowed_handle_ != NULL) {
    // Unsubclass the window.
    WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(windowed_handle_, GWLP_WNDPROC));
    if (current_wnd_proc == NativeWndProc) {
      SetWindowLongPtr(windowed_handle_,
                       GWLP_WNDPROC,
                       reinterpret_cast<LONG>(plugin_wnd_proc_));
    }

    DestroyWindow(windowed_handle_);
    windowed_handle_ = 0;
  }
}

// Erase all messages in the queue destined for a particular window.
// When windows are closing, callers should use this function to clear
// the queue.
// static
void WebPluginDelegateImpl::ClearThrottleQueueForWindow(HWND window) {
  std::list<MSG>::iterator it;
  for (it = throttle_queue_.begin(); it != throttle_queue_.end(); ) {
    if (it->hwnd == window) {
      it = throttle_queue_.erase(it);
      windowless_queue.Decrement();
    } else {
      it++;
    }
  }
}

// Delayed callback for processing throttled messages.
// Throttled messages are aggregated globally across all plugins.
// static
void WebPluginDelegateImpl::OnThrottleMessage() {
  // The current algorithm walks the list and processes the first
  // message it finds for each plugin.  It is important to service
  // all active plugins with each pass through the throttle, otherwise
  // we see video jankiness.
  std::map<HWND, int> processed;

  std::list<MSG>::iterator it = throttle_queue_.begin();
  while (it != throttle_queue_.end()) {
    const MSG& msg = *it;
    if (processed.find(msg.hwnd) == processed.end()) {
      WNDPROC proc = reinterpret_cast<WNDPROC>(msg.time);
	  // It is possible that the window was closed after we queued
	  // this message.  This is a rare event; just verify the window
	  // is alive.  (see also bug 1259488)
	  if (IsWindow(msg.hwnd))
          CallWindowProc(proc, msg.hwnd, msg.message, msg.wParam, msg.lParam);
      processed[msg.hwnd] = 1;
      it = throttle_queue_.erase(it);
      windowless_queue.Decrement();
    } else {
      it++;
    }
  }

  if (throttle_queue_.size() > 0)
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableFunction(&WebPluginDelegateImpl::OnThrottleMessage),
        kFlashWMUSERMessageThrottleDelayMs);
}

// Schedule a windows message for delivery later.
// static
void WebPluginDelegateImpl::ThrottleMessage(WNDPROC proc, HWND hwnd,
                                            UINT message, WPARAM wParam,
                                            LPARAM lParam) {
  MSG msg;
  msg.time = reinterpret_cast<DWORD>(proc);
  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;
  throttle_queue_.push_back(msg);
  windowless_queue.Increment();

  if (throttle_queue_.size() == 1) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableFunction(&WebPluginDelegateImpl::OnThrottleMessage),
        kFlashWMUSERMessageThrottleDelayMs);
  }
}

// We go out of our way to find the hidden windows created by Flash for
// windowless plugins.  We throttle the rate at which they deliver messages
// so that they will not consume outrageous amounts of CPU.
// static
LRESULT CALLBACK WebPluginDelegateImpl::FlashWindowlessWndProc(HWND hwnd,
    UINT message, WPARAM wparam, LPARAM lparam) {
  WNDPROC old_proc = reinterpret_cast<WNDPROC>(GetProp(hwnd, kPluginOrigProc));
  DCHECK(old_proc);

  switch (message) {
    case WM_NCDESTROY: {
      WebPluginDelegateImpl::ClearThrottleQueueForWindow(hwnd);
      break;
    }
    // Flash may flood the message queue with WM_USER+1 message causing 100% CPU
    // usage.  See https://bugzilla.mozilla.org/show_bug.cgi?id=132759.  We
    // prevent this by throttling the messages.
    case WM_USER + 1: {
      WebPluginDelegateImpl::ThrottleMessage(old_proc, hwnd, message, wparam,
                                             lparam);
      return TRUE;
    }
    default: {
      break;
    }
  }
  return CallWindowProc(old_proc, hwnd, message, wparam, lparam);
}

// Callback for enumerating the Flash windows.
BOOL CALLBACK EnumFlashWindows(HWND window, LPARAM arg) {
  WNDPROC wnd_proc = reinterpret_cast<WNDPROC>(arg);
  TCHAR class_name[1024];
  if (!RealGetWindowClass(window, class_name,
                          sizeof(class_name)/sizeof(TCHAR))) {
    LOG(ERROR) << "RealGetWindowClass failure: " << GetLastError();
    return FALSE;
  }

  if (wcscmp(class_name, L"SWFlash_PlaceholderX"))
    return TRUE;

  WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(window, GWLP_WNDPROC));
  if (current_wnd_proc != wnd_proc) {
    WNDPROC old_flash_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        window, GWLP_WNDPROC,
        reinterpret_cast<LONG>(wnd_proc)));
    DCHECK(old_flash_proc);
    BOOL result = SetProp(window, kPluginOrigProc, old_flash_proc);
    if (!result) {
      LOG(ERROR) << "SetProp failed, last error = " << GetLastError();
      return FALSE;
    }
  }

  return TRUE;
}

bool WebPluginDelegateImpl::CreateDummyWindowForActivation() {
  DCHECK(!dummy_window_for_activation_);
  dummy_window_for_activation_ = CreateWindowEx(
    0,
    L"Static",
    kDummyActivationWindowName,
    WS_CHILD,
    0,
    0,
    0,
    0,
    parent_,
    0,
    GetModuleHandle(NULL),
    0);

  if (dummy_window_for_activation_ == 0)
    return false;

  // Flash creates background windows which use excessive CPU in our
  // environment; we wrap these windows and throttle them so that they don't
  // get out of hand.
  if (!EnumThreadWindows(::GetCurrentThreadId(), EnumFlashWindows,
      reinterpret_cast<LPARAM>(
      &WebPluginDelegateImpl::FlashWindowlessWndProc))) {
    // Log that this happened.  Flash will still work; it just means the
    // throttle isn't installed (and Flash will use more CPU).
    NOTREACHED();
    LOG(ERROR) << "Failed to wrap all windowless Flash windows";
  }
  return true;
}

void WebPluginDelegateImpl::MoveWindow(HWND window,
                                       const gfx::Rect& window_rect,
                                       const gfx::Rect& clip_rect,
                                       bool visible) {
  HRGN hrgn = ::CreateRectRgn(clip_rect.x(),
                              clip_rect.y(),
                              clip_rect.right(),
                              clip_rect.bottom());

  // Note: System will own the hrgn after we call SetWindowRgn,
  // so we don't need to call DeleteObject(hrgn)
  ::SetWindowRgn(window, hrgn, FALSE);

  unsigned long flags = 0;
  if (visible)
    flags |= SWP_SHOWWINDOW;
  else
    flags |= SWP_HIDEWINDOW;

  ::SetWindowPos(window,
                 NULL,
                 window_rect.x(),
                 window_rect.y(),
                 window_rect.width(),
                 window_rect.height(),
                 flags);
}

bool WebPluginDelegateImpl::WindowedReposition(const gfx::Rect& window_rect,
                                               const gfx::Rect& clip_rect,
                                               bool visible) {
  if (!windowed_handle_) {
    NOTREACHED();
    return false;
  }

  if (window_rect_ == window_rect && clip_rect_ == clip_rect &&
      initial_plugin_resize_done_)
    return false;

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  if (!initial_plugin_resize_done_) {
    // We need to ensure that the plugin process continues to reposition
    // the plugin window until we receive an indication that it is now visible.
    // Subsequent repositions will be done by the browser.
    if (visible)
      initial_plugin_resize_done_ = true;
    // We created the window with 0 width and height since we didn't know it
    // at the time.  Now that we know the geometry, we we can update its size
    // since the browser only calls SetWindowPos when scrolling occurs.
    MoveWindow(windowed_handle_, window_rect, clip_rect, visible);
    // Ensure that the entire window gets repainted.
    ::InvalidateRect(windowed_handle_, NULL, FALSE);
  }

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();

  window_.window = windowed_handle_;
  window_.type = NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
  if (quirks_ & PLUGIN_QUIRK_SETWINDOW_TWICE)
    instance()->NPP_SetWindow(&window_);

  WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(windowed_handle_, GWLP_WNDPROC));
  if (current_wnd_proc != NativeWndProc) {
    plugin_wnd_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        windowed_handle_, GWLP_WNDPROC, reinterpret_cast<LONG>(NativeWndProc)));
  }
}

ATOM WebPluginDelegateImpl::RegisterNativeWindowClass() {
  static bool have_registered_window_class = false;
  if (have_registered_window_class == true)
      return true;

  have_registered_window_class = true;

  WNDCLASSEX wcex;
  wcex.cbSize         = sizeof(WNDCLASSEX);
  wcex.style          = CS_DBLCLKS;
  wcex.lpfnWndProc    = DefWindowProc;
  wcex.cbClsExtra     = 0;
  wcex.cbWndExtra     = 0;
  wcex.hInstance      = GetModuleHandle(NULL);
  wcex.hIcon          = 0;
  wcex.hCursor        = 0;
  // Some plugins like windows media player 11 create child windows parented
  // by our plugin window, where the media content is rendered. These plugins
  // dont implement WM_ERASEBKGND, which causes painting issues, when the
  // window where the media is rendered is moved around. DefWindowProc does
  // implement WM_ERASEBKGND correctly if we have a valid background brush.
  wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
  wcex.lpszMenuName   = 0;
  wcex.lpszClassName  = kNativeWindowClassName;
  wcex.hIconSm        = 0;

  return RegisterClassEx(&wcex);
}

LRESULT CALLBACK WebPluginDelegateImpl::NativeWndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  WebPluginDelegateImpl* delegate = reinterpret_cast<WebPluginDelegateImpl*>(
      GetProp(hwnd, kWebPluginDelegateProperty));
  if (!delegate) {
    NOTREACHED();
    return 0;
  }

  if (message == delegate->last_message_ &&
      delegate->quirks() & PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY &&
      delegate->is_calling_wndproc) {
    // Real may go into a state where it recursively dispatches the same event
    // when subclassed.  See https://bugzilla.mozilla.org/show_bug.cgi?id=192914
    // We only do the recursive check for Real because it's possible and valid
    // for a plugin to synchronously dispatch a message to itself such that it
    // looks like it's in recursion.
    return TRUE;
  }

  switch (message) {
    case WM_NCDESTROY: {
      RemoveProp(hwnd, kWebPluginDelegateProperty);
      ATOM plugin_name_atom = reinterpret_cast  <ATOM>(
          RemoveProp(hwnd, kPluginNameAtomProperty));
      if (plugin_name_atom != 0)
        GlobalDeleteAtom(plugin_name_atom);
      ClearThrottleQueueForWindow(hwnd);
      break;
    }
    // Flash may flood the message queue with WM_USER+1 message causing 100% CPU
    // usage.  See https://bugzilla.mozilla.org/show_bug.cgi?id=132759.  We
    // prevent this by throttling the messages.
    case WM_USER + 1: {
      if (delegate->quirks() & PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE) {
        WebPluginDelegateImpl::ThrottleMessage(delegate->plugin_wnd_proc_, hwnd,
                                               message, wparam, lparam);
        return FALSE;
      }
      break;
    }
    default: {
      break;
    }
  }

  delegate->last_message_ = message;
  delegate->is_calling_wndproc = true;

  if (!delegate->user_gesture_message_posted_ && 
       IsUserGestureMessage(message)) {
    delegate->user_gesture_message_posted_ = true;

    delegate->instance()->PushPopupsEnabledState(true);

    MessageLoop::current()->PostTask(FROM_HERE,
        delegate->user_gesture_msg_factory_.NewRunnableMethod(
            &WebPluginDelegateImpl::OnUserGestureEnd));
  }

  LRESULT result = CallWindowProc(delegate->plugin_wnd_proc_, hwnd, message,
                                  wparam, lparam);
  delegate->is_calling_wndproc = false;
  return result;
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // Set this flag before entering the instance in case of side-effects.
  windowless_needs_set_window_ = true;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    WindowlessSetWindow(true);

    WINDOWPOS win_pos = {0};
    win_pos.x = window_rect_.x();
    win_pos.y = window_rect_.y();
    win_pos.cx = window_rect_.width();
    win_pos.cy = window_rect_.height();

    NPEvent pos_changed_event;
    pos_changed_event.event = WM_WINDOWPOSCHANGED;
    pos_changed_event.wParam = 0;
    pos_changed_event.lParam = PtrToUlong(&win_pos);

    instance()->NPP_HandleEvent(&pos_changed_event);
  }
}

void WebPluginDelegateImpl::WindowlessPaint(HDC hdc,
                                            const gfx::Rect& damage_rect) {
  DCHECK(hdc);

  RECT damage_rect_win;
  damage_rect_win.left   = damage_rect.x();  // + window_rect_.x();
  damage_rect_win.top    = damage_rect.y();  // + window_rect_.y();
  damage_rect_win.right  = damage_rect_win.left + damage_rect.width();
  damage_rect_win.bottom = damage_rect_win.top + damage_rect.height();

  // We need to pass the HDC to the plugin via NPP_SetWindow in the
  // first paint to ensure that it initiates rect invalidations.
  if (window_.window == NULL)
    windowless_needs_set_window_ = true;

  window_.window = hdc;
  // TODO(darin): we should avoid calling NPP_SetWindow here since it may
  // cause page layout to be invalidated.

  // We really don't need to continually call SetWindow.
  // m_needsSetWindow flags when the geometry has changed.
  if (windowless_needs_set_window_)
    WindowlessSetWindow(false);

  NPEvent paint_event;
  paint_event.event = WM_PAINT;
  // NOTE: NPAPI is not 64bit safe.  It puts pointers into 32bit values.
  paint_event.wParam = PtrToUlong(hdc);
  paint_event.lParam = PtrToUlong(&damage_rect_win);
  static StatsRate plugin_paint(L"Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);
  instance()->NPP_HandleEvent(&paint_event);
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  if (!force_set_window)
    // Reset this flag before entering the instance in case of side-effects.
    windowless_needs_set_window_ = false;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  DCHECK(instance()->windowless());

  NPEvent focus_event;
  focus_event.event = WM_SETFOCUS;
  focus_event.wParam = 0;
  focus_event.lParam = 0;

  instance()->NPP_HandleEvent(&focus_event);
}

bool WebPluginDelegateImpl::HandleEvent(NPEvent* event,
                                        WebCursor* cursor) {
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  // To ensure that the plugin receives keyboard events we set focus to the
  // dummy window.
  // TODO(iyengar) We need a framework in the renderer to identify which
  // windowless plugin is under the mouse and to handle this. This would
  // also require some changes in RenderWidgetHost to detect this in the
  // WM_MOUSEACTIVATE handler and inform the renderer accordingly.
  HWND prev_focus_window = NULL;
  if (event->event == WM_RBUTTONDOWN) {
    prev_focus_window = ::SetFocus(dummy_window_for_activation_);
  }

  if (ShouldTrackEventForModalLoops(event)) {
    // A windowless plugin can enter a modal loop in a NPP_HandleEvent call.
    // For e.g. Flash puts up a context menu when we right click on the
    // windowless plugin area. We detect this by setting up a message filter
    // hook pror to calling NPP_HandleEvent on the plugin and unhook on
    // return from NPP_HandleEvent. If the plugin does enter a modal loop
    // in that context we unhook on receiving the first notification in
    // the message filter hook.
    handle_event_message_filter_hook_ =
        SetWindowsHookEx(WH_MSGFILTER, HandleEventMessageFilterHook, NULL,
                         GetCurrentThreadId());
  }

  bool old_task_reentrancy_state =
      MessageLoop::current()->NestableTasksAllowed();

  current_plugin_instance_ = this;

  handle_event_depth_++;

  bool pop_user_gesture = false;

  if (IsUserGestureMessage(event->event)) {
    pop_user_gesture = true;
    instance()->PushPopupsEnabledState(true);
  }

  bool ret = instance()->NPP_HandleEvent(event) != 0;

  if (pop_user_gesture) {
    instance()->PopPopupsEnabledState();
  }

  handle_event_depth_--;

  current_plugin_instance_ = NULL;

  MessageLoop::current()->SetNestableTasksAllowed(old_task_reentrancy_state);

  if (handle_event_message_filter_hook_) {
    UnhookWindowsHookEx(handle_event_message_filter_hook_);
    handle_event_message_filter_hook_ = NULL;
  }

  // We could have multiple NPP_HandleEvent calls nested together in case
  // the plugin enters a modal loop. Reset the pump messages event when
  // the outermost NPP_HandleEvent call unwinds.
  if (handle_event_depth_ == 0) {
    ResetEvent(handle_event_pump_messages_event_);
  }

  if (::IsWindow(prev_focus_window)) {
    ::SetFocus(prev_focus_window);
  }

  if (WM_MOUSEMOVE == event->event) {
    HCURSOR actual_cursor = ::GetCursor();
    *cursor = GetCursorType(actual_cursor);
  }

  return ret;
}

WebCursor::Type WebPluginDelegateImpl::GetCursorType(
    HCURSOR cursor) const {
  static HCURSOR standard_cursors[] = {
    LoadCursor(NULL, IDC_ARROW),
    LoadCursor(NULL, IDC_IBEAM),
    LoadCursor(NULL, IDC_WAIT),
    LoadCursor(NULL, IDC_CROSS),
    LoadCursor(NULL, IDC_UPARROW),
    LoadCursor(NULL, IDC_SIZE),
    LoadCursor(NULL, IDC_ICON),
    LoadCursor(NULL, IDC_SIZENWSE),
    LoadCursor(NULL, IDC_SIZENESW),
    LoadCursor(NULL, IDC_SIZEWE),
    LoadCursor(NULL, IDC_SIZENS),
    LoadCursor(NULL, IDC_SIZEALL),
    LoadCursor(NULL, IDC_NO),
    LoadCursor(NULL, IDC_HAND),
    LoadCursor(NULL, IDC_APPSTARTING),
    LoadCursor(NULL, IDC_HELP),
  };

  for (int cursor_index = 0; cursor_index < arraysize(standard_cursors);
      cursor_index++) {
    if (cursor == standard_cursors[cursor_index])
      return static_cast<WebCursor::Type>(cursor_index);
  }

  return WebCursor::ARROW;
}

WebPluginResourceClient* WebPluginDelegateImpl::CreateResourceClient(
    int resource_id, const std::string &url, bool notify_needed,
    void *notify_data, void* existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);
    
    plugin_stream->CancelRequest();

    return plugin_stream->AsResourceClient();
  }

  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(resource_id,
                                                            url,
                                                            mime_type,
                                                            notify_needed,
                                                            notify_data);
  return stream;
}

void WebPluginDelegateImpl::URLRequestRouted(const std::string&url,
                                             bool notify_needed,
                                             void* notify_data) {
  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
}

void WebPluginDelegateImpl::OnModalLoopEntered() {
  DCHECK(handle_event_pump_messages_event_ != NULL);
  SetEvent(handle_event_pump_messages_event_);

  MessageLoop::current()->SetNestableTasksAllowed(true);

  UnhookWindowsHookEx(handle_event_message_filter_hook_);
  handle_event_message_filter_hook_ = NULL;
}

bool WebPluginDelegateImpl::ShouldTrackEventForModalLoops(NPEvent* event) {
  if (event->event == WM_RBUTTONDOWN)
    return true;
  return false;
}

bool WebPluginDelegateImpl::IsUserGestureMessage(unsigned int message) {
  switch (message) {
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_KEYUP:
      return true;

    default:
      break;
  }

  return false;
}

void WebPluginDelegateImpl::OnUserGestureEnd() {
  user_gesture_message_posted_ = false;
  instance()->PopPopupsEnabledState();
}
