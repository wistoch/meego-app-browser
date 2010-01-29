// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <unistd.h>
#include <set>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/coregraphics_private_symbols_mac.h"
#include "webkit/glue/plugins/fake_plugin_window_tracker_mac.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

// If we're compiling support for the QuickDraw drawing model, turn off GCC
// warnings about deprecated functions (since QuickDraw is a deprecated API).
// According to the GCC documentation, this can only be done per file, not
// pushed and popped like some options can be.
#ifndef NP_NO_QUICKDRAW
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

// Important implementation notes: The Mac definition of NPAPI, particularly
// the distinction between windowed and windowless modes, differs from the
// Windows and Linux definitions.  Most of those differences are
// accomodated by the WebPluginDelegate class.

namespace {

base::LazyInstance<std::set<WebPluginDelegateImpl*> > g_active_delegates(
    base::LINKER_INITIALIZED);

WebPluginDelegateImpl* g_active_delegate;

// Helper to simplify correct usage of g_active_delegate.  Instantiating will
// set the active delegate to |delegate| for the lifetime of the object, then
// NULL when it goes out of scope.
class ScopedActiveDelegate {
public:
  explicit ScopedActiveDelegate(WebPluginDelegateImpl* delegate) {
    g_active_delegate = delegate;
  }
  ~ScopedActiveDelegate() {
    g_active_delegate = NULL;
  }
private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveDelegate);
};

#ifndef NP_NO_CARBON
// Timer periods for sending idle events to Carbon plugins. The visible value
// (50Hz) matches both Safari and Firefox. The hidden value (8Hz) matches
// Firefox; according to https://bugzilla.mozilla.org/show_bug.cgi?id=525533
// going lower than that causes issues.
const int kVisibleIdlePeriodMs = 20;     // (50Hz)
const int kHiddenIdlePeriodMs = 125;  // (8Hz)

class CarbonIdleEventSource {
 public:
  // Returns the shared Carbon idle event source.
  static CarbonIdleEventSource* SharedInstance() {
    DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
    static CarbonIdleEventSource* event_source = new CarbonIdleEventSource();
    return event_source;
  }

  // Registers the plugin delegate as interested in receiving idle events
  // suitable for a visible plugin.
  // Registering a delegate as visible automatically unregisters it from the
  // hidden event source.
  void RegisterVisibleDelegate(WebPluginDelegateImpl* delegate) {
    UnregisterDelegate(delegate);
    if (visible_delegates_.empty()) {
      visible_timer_.Start(
          base::TimeDelta::FromMilliseconds(kVisibleIdlePeriodMs), this,
          &CarbonIdleEventSource::SendVisiblePluginEvents);
    }
    visible_delegates_.insert(delegate);
  }

  // Registers the plugin delegate as interested in receiving idle events
  // suitable for a plugin that isn't visible.
  // Registering a delegate as hidden automatically unregisters it from the
  // visible event source.
  void RegisterHiddenDelegate(WebPluginDelegateImpl* delegate) {
    UnregisterDelegate(delegate);
    if (hidden_delegates_.empty()) {
      hidden_timer_.Start(
          base::TimeDelta::FromMilliseconds(kHiddenIdlePeriodMs), this,
          &CarbonIdleEventSource::SendHiddenPluginEvents);
    }
    hidden_delegates_.insert(delegate);
  }

  // Removes the plugin delegate from the list of plugins receiving idle events.
  void UnregisterDelegate(WebPluginDelegateImpl* delegate) {
    size_t removed = visible_delegates_.erase(delegate);
    if (removed > 0 && visible_delegates_.empty())
      visible_timer_.Stop();
    removed = hidden_delegates_.erase(delegate);
    if (removed > 0 && hidden_delegates_.empty())
      hidden_timer_.Stop();
  }

 private:
  CarbonIdleEventSource() {}

  void SendVisiblePluginEvents() {
    SendIdleEventsToDelegates(visible_delegates_);
  }

  void SendHiddenPluginEvents() {
    SendIdleEventsToDelegates(hidden_delegates_);
  }

  void SendIdleEventsToDelegates(
      const std::set<WebPluginDelegateImpl*>& delegates) const {
    for (std::set<WebPluginDelegateImpl*>::iterator i = delegates.begin();
         i != delegates.end();) {
      // If the plugin changes size or position during idle event handling, it
      // may be removed from this set; increment the iterator before calling
      // into the delegate to ensure that the iteration won't be corrupted.
      WebPluginDelegateImpl* delegate = *(i++);
      delegate->FireIdleEvent();
    }
  }

  base::RepeatingTimer<CarbonIdleEventSource> visible_timer_;
  base::RepeatingTimer<CarbonIdleEventSource> hidden_timer_;
  std::set<WebPluginDelegateImpl*> visible_delegates_;
  std::set<WebPluginDelegateImpl*> hidden_delegates_;
};
#endif  // !NP_NO_CARBON

}  // namespace

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : windowless_needs_set_window_(true),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      quirks_(0),
      last_window_x_offset_(0),
      last_window_y_offset_(0),
      last_mouse_x_(0),
      last_mouse_y_(0),
      have_focus_(false),
      focus_notifier_(NULL),
      containing_window_has_focus_(false),
      handle_event_depth_(0),
      user_gesture_message_posted_(this),
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));
#ifndef NP_NO_CARBON
  memset(&cg_context_, 0, sizeof(cg_context_));
#endif
#ifndef NP_NO_QUICKDRAW
  memset(&qd_port_, 0, sizeof(qd_port_));
#endif
  instance->set_windowless(true);

  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  delegates->insert(this);
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  delegates->erase(this);
#ifndef NP_NO_CARBON
  if (cg_context_.window) {
    FakePluginWindowTracker::SharedInstance()->RemoveFakeWindowForDelegate(
        this, reinterpret_cast<WindowRef>(cg_context_.window));
  }
#endif
}

void WebPluginDelegateImpl::PluginDestroyed() {
  if (instance()->event_model() == NPEventModelCarbon) {
    if (instance()->drawing_model() == NPDrawingModelQuickDraw) {
      // Tell the plugin it should stop drawing into the window (which will go
      // away when the next idle event arrives).
      window_.x = 0;
      window_.y = 0;
      window_.width = 0;
      window_.height = 0;
      window_.clipRect.top = 0;
      window_.clipRect.left = 0;
      window_.clipRect.bottom = 0;
      window_.clipRect.right = 0;
      instance()->NPP_SetWindow(&window_);
      QDFlushPortBuffer(qd_port_.port, NULL);
    }
  }
  DestroyInstance();
  delete this;
}

void WebPluginDelegateImpl::PlatformInitialize() {
  // Don't set a NULL window handle on destroy for Mac plugins.  This matches
  // Safari and other Mac browsers (see PluginView::stop() in PluginView.cpp,
  // where code to do so is surrounded by an #ifdef that excludes Mac OS X, or
  // destroyPlugin in WebNetscapePluginView.mm, for examples).
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // Create a stand-in for the browser window so that the plugin will have
    // a non-NULL WindowRef to which it can refer.
    FakePluginWindowTracker* window_tracker =
        FakePluginWindowTracker::SharedInstance();
    cg_context_.window = window_tracker->GenerateFakeWindowForDelegate(this);
    cg_context_.context = NULL;
    Rect window_bounds = { 0, 0, window_rect_.height(), window_rect_.width() };
    SetWindowBounds(reinterpret_cast<WindowRef>(cg_context_.window),
                    kWindowContentRgn, &window_bounds);
    qd_port_.port =
        GetWindowPort(reinterpret_cast<WindowRef>(cg_context_.window));
  }
#endif

  switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      window_.window = &qd_port_;
      window_.type = NPWindowTypeDrawable;
      break;
#endif
    case NPDrawingModelCoreGraphics:
#ifndef NP_NO_CARBON
      if (instance()->event_model() == NPEventModelCarbon)
        window_.window = &cg_context_;
#endif
      window_.type = NPWindowTypeDrawable;
      break;
    default:
      NOTREACHED();
      break;
  }

#ifndef NP_NO_CARBON
  // If the plugin wants Carbon events, hook up to the source of idle events.
  if (instance()->event_model() == NPEventModelCarbon)
    UpdateIdleEventRate();
#endif
  plugin_->SetWindow(NULL);

}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  if (instance()->event_model() == NPEventModelCarbon)
    CarbonIdleEventSource::SharedInstance()->UnregisterDelegate(this);
}

void WebPluginDelegateImpl::UpdateContext(CGContextRef context) {
#ifndef NP_NO_CARBON
  // Flash on the Mac apparently caches the context from the struct it receives
  // in NPP_SetWindow, and continues to use it even when the contents of the
  // struct have changed, so we need to call NPP_SetWindow again if the context
  // changes.
  if (instance()->event_model() == NPEventModelCarbon &&
      context != cg_context_.context) {
    cg_context_.context = context;
    WindowlessSetWindow(true);
  }
#endif
}

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  DCHECK(windowless_);
  WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
  // Disabling the call to NPP_Print as it causes a crash in
  // flash in some cases. In any case this does not work as expected
  // as the EMF meta file dc passed in needs to be created with the
  // the plugin window dc as its sibling dc and the window rect
  // in .01 mm units.
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  NOTREACHED();
}

bool WebPluginDelegateImpl::WindowedReposition(const gfx::Rect& window_rect,
                                               const gfx::Rect& clip_rect) {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  NOTREACHED();
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  bool old_clip_was_empty = clip_rect_.IsEmpty();
  bool new_clip_is_empty = clip_rect.IsEmpty();
  clip_rect_ = clip_rect;

  // Only resend to the instance if the geometry has changed (see note in
  // WindowlesSetWindow for why we only care about the clip rect switching
  // empty state).
  if (window_rect == window_rect_ && old_clip_was_empty == new_clip_is_empty)
    return;

#ifndef NP_NO_CARBON
  // If visibility has changed, switch our idle event rate.
  if (instance()->event_model() == NPEventModelCarbon &&
      old_clip_was_empty != new_clip_is_empty) {
    UpdateIdleEventRate();
  }
#endif

  window_rect_ = window_rect;
  WindowlessSetWindow(true);
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    // If we somehow get a paint before we've set up the plugin window, bail.
    if (!cg_context_.context)
      return;
    DCHECK(cg_context_.context == context);
  }
#endif

  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);

  ScopedActiveDelegate active_delegate(this);

  switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw: {
      // Plugins using the QuickDraw drawing model do not restrict their
      // drawing to update events the way that CoreGraphics-based plugins
      // do.  When we are asked to paint, we therefore just copy from the
      // plugin's hidden window into our shared memory bitmap context.
      CGRect window_bounds = CGRectMake(0, 0,
                                        window_rect_.width(),
                                        window_rect_.height());
      CGWindowID window_id = HIWindowGetCGWindowID(
          reinterpret_cast<WindowRef>(cg_context_.window));
      CGContextSaveGState(context);
      CGContextTranslateCTM(context, 0, window_rect_.height());
      CGContextScaleCTM(context, 1.0, -1.0);
      CGContextCopyWindowCaptureContentsToRect(context, window_bounds,
                                               _CGSDefaultConnection(),
                                               window_id, 0);
      CGContextRestoreGState(context);
    }
#endif
    case NPDrawingModelCoreGraphics: {
      CGContextSaveGState(context);
      switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon: {
          NPEvent paint_event = { 0 };
          paint_event.what = updateEvt;
          paint_event.message = reinterpret_cast<uint32>(cg_context_.window);
          paint_event.when = TickCount();
          instance()->NPP_HandleEvent(&paint_event);
          break;
        }
#endif
        case NPEventModelCocoa: {
          NPCocoaEvent paint_event;
          memset(&paint_event, 0, sizeof(NPCocoaEvent));
          paint_event.type = NPCocoaEventDrawRect;
          paint_event.data.draw.context = context;
          paint_event.data.draw.x = damage_rect.x();
          paint_event.data.draw.y = damage_rect.y();
          paint_event.data.draw.width = damage_rect.width();
          paint_event.data.draw.height = damage_rect.height();
          instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&paint_event));
          break;
        }
      }
      CGContextRestoreGState(context);
    }
  }
}

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  window_.x = 0;
  window_.y = 0;
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.clipRect.left = window_.x;
  window_.clipRect.top = window_.y;
  window_.clipRect.right = window_.clipRect.left;
  window_.clipRect.bottom = window_.clipRect.top;
  if (!clip_rect_.IsEmpty()) {
    // We never tell plugins that they are only partially visible; because the
    // drawing target doesn't change size, the positioning of what plugins drew
    // would be wrong, as would any transforms they did on the context.
    window_.clipRect.right += window_.width;
    window_.clipRect.bottom += window_.height;
  }

  UpdateDummyWindowBoundsWithOffset(window_rect_.x(), window_rect_.y(),
                                    window_rect_.width(),
                                    window_rect_.height());

  NPError err = instance()->NPP_SetWindow(&window_);

  // TODO(stuartmorgan): Once we are getting window information via IPC, use
  // that to set the right value. For now, just pretend plugins are always in
  // active windows so they don't throw away events.
  SetWindowHasFocus(true);

  DCHECK(err == NPERR_NO_ERROR);
}

WebPluginDelegateImpl* WebPluginDelegateImpl::GetActiveDelegate() {
  return g_active_delegate;
}

std::set<WebPluginDelegateImpl*> WebPluginDelegateImpl::GetActiveDelegates() {
  std::set<WebPluginDelegateImpl*>* delegates = g_active_delegates.Pointer();
  return *delegates;
}

void WebPluginDelegateImpl::FocusNotify(WebPluginDelegateImpl* delegate) {
  have_focus_ = (delegate == this);

  ScopedActiveDelegate active_delegate(this);

  switch (instance()->event_model()) {
    case NPEventModelCarbon: {
      NPEvent focus_event = { 0 };
      if (have_focus_)
        focus_event.what = NPEventType_GetFocusEvent;
      else
        focus_event.what = NPEventType_LoseFocusEvent;
      focus_event.when = TickCount();
      instance()->NPP_HandleEvent(&focus_event);
      break;
    }
    case NPEventModelCocoa: {
      NPCocoaEvent focus_event;
      memset(&focus_event, 0, sizeof(focus_event));
      focus_event.type = NPCocoaEventFocusChanged;
      focus_event.data.focus.hasFocus = have_focus_;
      instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&focus_event));
      break;
    }
  }
}

void WebPluginDelegateImpl::SetFocus() {
  if (focus_notifier_)
    focus_notifier_(this);
  else
    FocusNotify(this);
}

void WebPluginDelegateImpl::SetWindowHasFocus(bool has_focus) {
  containing_window_has_focus_ = has_focus;

  if (instance()->event_model() == NPEventModelCocoa) {
    ScopedActiveDelegate active_delegate(this);
    NPCocoaEvent focus_event;
    memset(&focus_event, 0, sizeof(focus_event));
    focus_event.type = NPCocoaEventWindowFocusChanged;
    focus_event.data.focus.hasFocus = has_focus;
    instance()->NPP_HandleEvent(reinterpret_cast<NPEvent*>(&focus_event));
  }
}

void WebPluginDelegateImpl::SetThemeCursor(ThemeCursor cursor) {
  current_windowless_cursor_.InitFromThemeCursor(cursor);
}

void WebPluginDelegateImpl::SetCursor(const Cursor* cursor) {
  current_windowless_cursor_.InitFromCursor(cursor);
}

void WebPluginDelegateImpl::SetNSCursor(NSCursor* cursor) {
  current_windowless_cursor_.InitFromNSCursor(cursor);
}

void WebPluginDelegateImpl::UpdatePluginLocation(const WebMouseEvent& event) {
  instance()->set_plugin_origin(gfx::Point(event.globalX - event.x,
                                           event.globalY - event.y));

  if (instance()->event_model() == NPEventModelCarbon) {
    last_window_x_offset_ = event.globalX - event.windowX;
    last_window_y_offset_ = event.globalY - event.windowY;
    last_mouse_x_ = event.globalX;
    last_mouse_y_ = event.globalY;

#ifndef NP_NO_CARBON
    UpdateDummyWindowBoundsWithOffset(event.windowX - event.x,
                                      event.windowY - event.y, 0, 0);
  }
#endif
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::UpdateDummyWindowBoundsWithOffset(
    int x_offset, int y_offset, int new_width, int new_height) {
  if (instance()->event_model() == NPEventModelCocoa)
    return;

  int target_x = last_window_x_offset_ + x_offset;
  int target_y = last_window_y_offset_ + y_offset;
  WindowRef window = reinterpret_cast<WindowRef>(cg_context_.window);
  Rect window_bounds;
  GetWindowBounds(window, kWindowContentRgn, &window_bounds);
  int old_width = window_bounds.right - window_bounds.left;
  int old_height = window_bounds.bottom - window_bounds.top;
  if (window_bounds.left != target_x ||
      window_bounds.top != target_y ||
      (new_width && new_width != old_width) ||
      (new_height && new_height != old_height)) {
    int height = new_height ? new_height : old_height;
    int width = new_width ? new_width : old_width;
    window_bounds.left = target_x;
    window_bounds.top = target_y;
    window_bounds.right = window_bounds.left + width;
    window_bounds.bottom = window_bounds.top + height;
    SetWindowBounds(window, kWindowContentRgn, &window_bounds);
  }
}

void WebPluginDelegateImpl::UpdateIdleEventRate() {
  if (clip_rect_.IsEmpty())
    CarbonIdleEventSource::SharedInstance()->RegisterHiddenDelegate(this);
  else
    CarbonIdleEventSource::SharedInstance()->RegisterVisibleDelegate(this);
}
#endif  // !NP_NO_CARBON

static bool WebInputEventIsWebMouseEvent(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
      if (event.size < sizeof(WebMouseEvent)) {
        NOTREACHED();
        return false;
      }
      return true;
    default:
      return false;
  }
}

static bool WebInputEventIsWebKeyboardEvent(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return true;
    default:
      return false;
  }
}

#ifndef NP_NO_CARBON
static NSInteger CarbonModifiersFromWebEvent(const WebInputEvent& event) {
  NSInteger modifiers = 0;
  if (event.modifiers & WebInputEvent::ControlKey)
    modifiers |= controlKey;
  if (event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= shiftKey;
  if (event.modifiers & WebInputEvent::AltKey)
    modifiers |= optionKey;
  if (event.modifiers & WebInputEvent::MetaKey)
    modifiers |= cmdKey;
  return modifiers;
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent *np_event) {
  np_event->where.h = event.globalX;
  np_event->where.v = event.globalY;

  np_event->modifiers |= CarbonModifiersFromWebEvent(event);

  // default to "button up"; override this for mouse down events below.
  np_event->modifiers |= btnState;

  switch (event.button) {
    case WebMouseEvent::ButtonLeft:
      break;
    case WebMouseEvent::ButtonMiddle:
      np_event->modifiers |= cmdKey;
      break;
    case WebMouseEvent::ButtonRight:
      np_event->modifiers |= controlKey;
      break;
    default:
      NOTIMPLEMENTED();
  }
  switch (event.type) {
    case WebInputEvent::MouseMove:
      np_event->what = nullEvent;
      return true;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      np_event->what = NPEventType_AdjustCursorEvent;
      return true;
    case WebInputEvent::MouseDown:
      np_event->modifiers &= ~btnState;
      np_event->what = mouseDown;
      return true;
    case WebInputEvent::MouseUp:
      np_event->what = mouseUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        NPEvent *np_event) {
  // TODO: figure out how to handle Unicode input to plugins, if that's
  // even possible in the NPAPI Carbon event model.
  np_event->message = (event.nativeKeyCode << 8) & keyCodeMask;
  np_event->message |= event.text[0] & charCodeMask;
  np_event->modifiers |= btnState;
  np_event->modifiers |= CarbonModifiersFromWebEvent(event);

  switch (event.type) {
    case WebInputEvent::KeyDown:
      if (event.modifiers & WebInputEvent::IsAutoRepeat)
        np_event->what = autoKey;
      else
        np_event->what = keyDown;
      return true;
    case WebInputEvent::KeyUp:
      np_event->what = keyUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebInputEvent(const WebInputEvent& event,
                                     NPEvent* np_event) {
  np_event->when = TickCount();
  if (WebInputEventIsWebMouseEvent(event)) {
    return NPEventFromWebMouseEvent(*static_cast<const WebMouseEvent*>(&event),
                                    np_event);
  } else if (WebInputEventIsWebKeyboardEvent(event)) {
    return NPEventFromWebKeyboardEvent(
        *static_cast<const WebKeyboardEvent*>(&event), np_event);
  }
  DLOG(WARNING) << "unknown event type" << event.type;
  return false;
}
#endif  // !NP_NO_CARBON

static NSInteger CocoaModifiersFromWebEvent(const WebInputEvent& event) {
  NSInteger modifiers = 0;
  if (event.modifiers & WebInputEvent::ControlKey)
    modifiers |= NSControlKeyMask;
  if (event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= NSShiftKeyMask;
  if (event.modifiers & WebInputEvent::AltKey)
    modifiers |= NSAlternateKeyMask;
  if (event.modifiers & WebInputEvent::MetaKey)
    modifiers |= NSCommandKeyMask;
  return modifiers;
}

static bool KeyIsModifier(int native_key_code) {
  switch (native_key_code) {
    case 55:  // Left command
    case 54:  // Right command
    case 58:  // Left option
    case 61:  // Right option
    case 59:  // Left control
    case 62:  // Right control
    case 56:  // Left shift
    case 60:  // Right shift
    case 57:  // Caps lock
      return true;
    default:
      return false;
  }
}

static bool NPCocoaEventFromWebMouseEvent(const WebMouseEvent& event,
                                          NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.mouse.pluginX = event.x;
  np_cocoa_event->data.mouse.pluginY = event.y;
  np_cocoa_event->data.mouse.modifierFlags |= CocoaModifiersFromWebEvent(event);
  np_cocoa_event->data.mouse.clickCount = event.clickCount;
  switch (event.button) {
    case WebMouseEvent::ButtonLeft:
      np_cocoa_event->data.mouse.buttonNumber = 0;
      break;
    case WebMouseEvent::ButtonMiddle:
      np_cocoa_event->data.mouse.buttonNumber = 2;
      break;
    case WebMouseEvent::ButtonRight:
      np_cocoa_event->data.mouse.buttonNumber = 1;
      break;
    default:
      np_cocoa_event->data.mouse.buttonNumber = event.button;
      break;
  }
  switch (event.type) {
    case WebInputEvent::MouseDown:
      np_cocoa_event->type = NPCocoaEventMouseDown;
      return true;
    case WebInputEvent::MouseUp:
      np_cocoa_event->type = NPCocoaEventMouseUp;
      return true;
    case WebInputEvent::MouseMove: {
      bool mouse_is_down = (event.modifiers & WebInputEvent::LeftButtonDown) ||
                           (event.modifiers & WebInputEvent::RightButtonDown) ||
                           (event.modifiers & WebInputEvent::MiddleButtonDown);
      np_cocoa_event->type = mouse_is_down ? NPCocoaEventMouseDragged
                                           : NPCocoaEventMouseMoved;
      return true;
    }
    case WebInputEvent::MouseEnter:
      np_cocoa_event->type = NPCocoaEventMouseEntered;
      return true;
    case WebInputEvent::MouseLeave:
      np_cocoa_event->type = NPCocoaEventMouseExited;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPCocoaEventFromWebMouseWheelEvent(const WebMouseWheelEvent& event,
                                               NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->type = NPCocoaEventScrollWheel;
  np_cocoa_event->data.mouse.pluginX = event.x;
  np_cocoa_event->data.mouse.pluginY = event.y;
  np_cocoa_event->data.mouse.modifierFlags |= CocoaModifiersFromWebEvent(event);
  np_cocoa_event->data.mouse.deltaX = event.deltaX;
  np_cocoa_event->data.mouse.deltaY = event.deltaY;
  return true;
}

static bool NPCocoaEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                             NPCocoaEvent *np_cocoa_event) {
  np_cocoa_event->data.key.keyCode = event.nativeKeyCode;

  np_cocoa_event->data.key.modifierFlags |= CocoaModifiersFromWebEvent(event);

  // Modifier keys have their own event type, and don't get character or
  // repeat data.
  if (KeyIsModifier(event.nativeKeyCode)) {
    np_cocoa_event->type = NPCocoaEventFlagsChanged;
    return true;
  }

  np_cocoa_event->data.key.characters = reinterpret_cast<NPNSString*>(
      [NSString stringWithFormat:@"%S", event.text]);
  np_cocoa_event->data.key.charactersIgnoringModifiers =
      reinterpret_cast<NPNSString*>(
          [NSString stringWithFormat:@"%S", event.unmodifiedText]);

  if (event.modifiers & WebInputEvent::IsAutoRepeat)
    np_cocoa_event->data.key.isARepeat = true;

  switch (event.type) {
    case WebInputEvent::KeyDown:
      np_cocoa_event->type = NPCocoaEventKeyDown;
      return true;
    case WebInputEvent::KeyUp:
      np_cocoa_event->type = NPCocoaEventKeyUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPCocoaEventFromWebInputEvent(const WebInputEvent& event,
                                          NPCocoaEvent *np_cocoa_event) {
  memset(np_cocoa_event, 0, sizeof(NPCocoaEvent));
  if (event.type == WebInputEvent::MouseWheel) {
    return NPCocoaEventFromWebMouseWheelEvent(
        *static_cast<const WebMouseWheelEvent*>(&event), np_cocoa_event);
  } else if (WebInputEventIsWebMouseEvent(event)) {
    return NPCocoaEventFromWebMouseEvent(
        *static_cast<const WebMouseEvent*>(&event), np_cocoa_event);
  } else if (WebInputEventIsWebKeyboardEvent(event)) {
    return NPCocoaEventFromWebKeyboardEvent(
        *static_cast<const WebKeyboardEvent*>(&event), np_cocoa_event);
  }
  DLOG(WARNING) << "unknown event type " << event.type;
  return false;
}

bool WebPluginDelegateImpl::HandleInputEvent(const WebInputEvent& event,
                                             WebCursorInfo* cursor) {
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      !cg_context_.context) {
    // If we somehow get an event before we've set up the plugin window, bail.
    return false;
  }
#endif

  if (WebInputEventIsWebMouseEvent(event)) {
    // Make sure we update our plugin location tracking before we send the
    // event to the plugin, so that any coordinate conversion the plugin does
    // will work out.
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    UpdatePluginLocation(*mouse_event);

    current_windowless_cursor_.GetCursorInfo(cursor);
  }

  // if we do not currently have focus and this is a mouseDown, trigger a
  // notification that we are taking the keyboard focus.  We can't just key
  // off of incoming calls to SetFocus, since WebKit may already think we
  // have it if we were the most recently focused element on our parent tab.
  if (event.type == WebInputEvent::MouseDown && !have_focus_)
    SetFocus();

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon) {
    if (event.type == WebInputEvent::MouseMove) {
      return true;  // The recurring OnNull will send null events.
    }

    switch (instance()->drawing_model()) {
#ifndef NP_NO_QUICKDRAW
      case NPDrawingModelQuickDraw:
        SetPort(qd_port_.port);
        break;
#endif
      case NPDrawingModelCoreGraphics:
        CGContextSaveGState(cg_context_.context);
        break;
    }
  }
#endif

  ScopedActiveDelegate active_delegate(this);

#ifndef NP_NO_CARBON
  // cgcontext_.context can change during event handling (because of a geometry
  // change triggered by the event); we need to know if that happens so we
  // don't keep trying to use the context. It is not an owning ref, so shouldn't
  // be used for anything but pointer comparison.
  CGContextRef old_context_weak = cg_context_.context;
#endif

  // Create the plugin event structure, and send it to the plugin.
  bool ret = false;
  switch (instance()->event_model()) {
#ifndef NP_NO_CARBON
    case NPEventModelCarbon: {
      NPEvent np_event = {0};
      if (!NPEventFromWebInputEvent(event, &np_event)) {
        LOG(WARNING) << "NPEventFromWebInputEvent failed";
        return false;
      }
      ret = instance()->NPP_HandleEvent(&np_event) != 0;
      break;
    }
#endif
    case NPEventModelCocoa: {
      NPCocoaEvent np_cocoa_event;
      if (!NPCocoaEventFromWebInputEvent(event, &np_cocoa_event)) {
        LOG(WARNING) << "NPCocoaEventFromWebInputEvent failed";
        return false;
      }
      NPAPI::ScopedCurrentPluginEvent event_scope(instance(), &np_cocoa_event);
      ret = instance()->NPP_HandleEvent(
          reinterpret_cast<NPEvent*>(&np_cocoa_event)) != 0;
      break;
    }
  }

  if (WebInputEventIsWebMouseEvent(event)) {
    // Plugins are not good about giving accurate information about whether or
    // not they handled events, and other browsers on the Mac generally ignore
    // the return value. We may need to expand this to other input types, but
    // we'll need to be careful about things like Command-keys.
    ret = true;
  }

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon &&
      instance()->drawing_model() == NPDrawingModelCoreGraphics &&
      cg_context_.context == old_context_weak)
    CGContextRestoreGState(cg_context_.context);
#endif

  return ret;
}

#ifndef NP_NO_CARBON
void WebPluginDelegateImpl::FireIdleEvent() {
  // Avoid a race condition between IO and UI threads during plugin shutdown
  if (!instance_)
    return;

  ScopedActiveDelegate active_delegate(this);

  if (!webkit_glue::IsPluginRunningInRendererProcess()) {
    switch (instance()->event_model()) {
      case NPEventModelCarbon:
        // If the plugin is running in a subprocess, drain any pending system
        // events so that the plugin's event handlers will get called on any
        // windows it has created.  Filter out activate/deactivate events on
        // the fake browser window, but pass everything else through.
        EventRecord event;
        while (GetNextEvent(everyEvent, &event)) {
          if (event.what == activateEvt && cg_context_.window &&
              reinterpret_cast<void *>(event.message) != cg_context_.window)
            continue;
          instance()->NPP_HandleEvent(&event);
        }
        break;
    }
  }

  if (instance()->event_model() == NPEventModelCarbon) {
    // Send an idle event so that the plugin can do background work
    NPEvent np_event = {0};
    np_event.what = nullEvent;
    np_event.when = TickCount();
    np_event.modifiers = GetCurrentKeyModifiers();
    if (!Button())
      np_event.modifiers |= btnState;
    np_event.where.h = last_mouse_x_;
    np_event.where.v = last_mouse_y_;
    instance()->NPP_HandleEvent(&np_event);
  }

#ifndef NP_NO_QUICKDRAW
  // Quickdraw-based plugins can draw at any time, so tell the renderer to
  // repaint.
  // TODO: only do this if the contents of the offscreen window has changed,
  // so as not to spam the renderer with an unchanging image.
  if (instance()->drawing_model() == NPDrawingModelQuickDraw)
    instance()->webplugin()->Invalidate();
#endif
}
#endif  // !NP_NO_CARBON
