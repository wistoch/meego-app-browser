// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_widget.h"

#include "base/command_line.h"
#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/renderer/render_process.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_POSIX)
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#endif  // defined(OS_POSIX)

#include "third_party/WebKit/WebKit/chromium/public/WebWidget.h"

using WebKit::WebCompositionCommand;
using WebKit::WebCursorInfo;
using WebKit::WebInputEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSize;
using WebKit::WebTextDirection;

RenderWidget::RenderWidget(RenderThreadBase* render_thread, bool activatable)
    : routing_id_(MSG_ROUTING_NONE),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      render_thread_(render_thread),
      host_window_(0),
      current_paint_buf_(NULL),
      next_paint_flags_(0),
      update_reply_pending_(false),
      did_show_(false),
      is_hidden_(false),
      needs_repainting_on_restore_(false),
      has_focus_(false),
      handling_input_event_(false),
      closing_(false),
      ime_is_active_(false),
      ime_control_enable_ime_(true),
      ime_control_x_(-1),
      ime_control_y_(-1),
      ime_control_new_state_(false),
      ime_control_updated_(false),
      ime_control_busy_(false),
      activatable_(activatable),
      pending_window_rect_count_(0),
      suppress_next_char_events_(false) {
  RenderProcess::current()->AddRefProcess();
  DCHECK(render_thread_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }
  RenderProcess::current()->ReleaseProcess();
}

/*static*/
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   RenderThreadBase* render_thread,
                                   bool activatable) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget = new RenderWidget(render_thread,
                                                        activatable);
  widget->Init(opener_id);  // adds reference
  return widget;
}

void RenderWidget::ConfigureAsExternalPopupMenu(const WebPopupMenuInfo& info) {
  popup_params_.reset(new ViewHostMsg_ShowPopup_Params);
  popup_params_->item_height = info.itemHeight;
  popup_params_->selected_item = info.selectedIndex;
  for (size_t i = 0; i < info.items.size(); ++i)
    popup_params_->popup_items.push_back(WebMenuItem(info.items[i]));
}

void RenderWidget::Init(int32 opener_id) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = WebPopupMenu::create(this);

  bool result = render_thread_->Send(
      new ViewHostMsg_CreateWidget(opener_id, activatable_, &routing_id_));
  if (result) {
    render_thread_->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
  } else {
    DCHECK(false);
  }
}

// This is used to complete pending inits and non-pending inits. For non-
// pending cases, the parent will be the same as the current parent. This
// indicates we do not need to reparent or anything.
void RenderWidget::CompleteInit(gfx::NativeViewId parent_hwnd) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  host_window_ = parent_hwnd;

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

IPC_DEFINE_MESSAGE_MAP(RenderWidget)
  IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
  IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
  IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
  IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
  IPC_MESSAGE_HANDLER(ViewMsg_WasRestored, OnWasRestored)
  IPC_MESSAGE_HANDLER(ViewMsg_UpdateRect_ACK, OnUpdateRectAck)
  IPC_MESSAGE_HANDLER(ViewMsg_HandleInputEvent, OnHandleInputEvent)
  IPC_MESSAGE_HANDLER(ViewMsg_MouseCaptureLost, OnMouseCaptureLost)
  IPC_MESSAGE_HANDLER(ViewMsg_SetFocus, OnSetFocus)
  IPC_MESSAGE_HANDLER(ViewMsg_ImeSetInputMode, OnImeSetInputMode)
  IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
  IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnMsgRepaint)
  IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
  IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
  IPC_MESSAGE_UNHANDLED_ERROR()
IPC_END_MESSAGE_MAP()

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close.
  if (closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return render_thread_->Send(message);
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck(gfx::NativeViewId parent) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit(parent);
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    render_thread_->RemoveRoute(routing_id_);
    SetHidden(false);
  }

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  MessageLoop::current()->PostNonNestableTask(FROM_HERE,
      NewRunnableMethod(this, &RenderWidget::Close));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnResize(const gfx::Size& new_size,
                            const gfx::Rect& resizer_rect) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // Remember the rect where the resize corner will be drawn.
  resizer_rect_ = resizer_rect;

  // TODO(darin): We should not need to reset this here.
  SetHidden(false);
  needs_repainting_on_restore_ = false;

  // We shouldn't be asked to resize to our current size.
  DCHECK(size_ != new_size);
  size_ = new_size;

  // We should not be sent a Resize message if we have not ACK'd the previous
  DCHECK(!next_paint_is_resize_ack());

  paint_aggregator_.ClearPendingUpdate();

  // When resizing, we want to wait to paint before ACK'ing the resize.  This
  // ensures that we only resize as fast as we can paint.  We only need to send
  // an ACK if we are resized to a non-empty rect.
  webwidget_->resize(new_size);
  if (!new_size.IsEmpty()) {
    // Resize should have caused an invalidation of the entire view.
    DCHECK(paint_aggregator_.HasPendingUpdate());

    // We will send the Resize_ACK flag once we paint again.
    set_next_paint_is_resize_ack();
  }
}

void RenderWidget::OnWasHidden() {
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
}

void RenderWidget::OnWasRestored(bool needs_repainting) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);

  if (!needs_repainting && !needs_repainting_on_restore_)
    return;
  needs_repainting_on_restore_ = false;

  // Tag the next paint as a restore ack, which is picked up by DoDeferredUpdate
  // when it sends out the next PaintRect message.
  set_next_paint_is_restore_ack();

  // Generate a full repaint.
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnUpdateRectAck() {
  DCHECK(update_reply_pending());
  update_reply_pending_ = false;

  // If we sent an UpdateRect message with a zero-sized bitmap, then we should
  // have no current update buf.
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }

  // Notify subclasses
  DidPaint();

  // Continue painting if necessary...
  CallDoDeferredUpdate();
}

void RenderWidget::OnHandleInputEvent(const IPC::Message& message) {
  void* iter = NULL;

  const char* data;
  int data_length;
  handling_input_event_ = true;
  if (!message.ReadData(&iter, &data, &data_length)) {
    handling_input_event_ = false;
    return;
  }

  const WebInputEvent* input_event =
      reinterpret_cast<const WebInputEvent*>(data);

  bool is_keyboard_shortcut = false;
  // is_keyboard_shortcut flag is only available for RawKeyDown events.
  if (input_event->type == WebInputEvent::RawKeyDown)
    message.ReadBool(&iter, &is_keyboard_shortcut);

  bool processed = false;
  if (input_event->type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (webwidget_)
      processed = webwidget_->handleInputEvent(*input_event);
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  if (!processed && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  IPC::Message* response = new ViewHostMsg_HandleInputEvent_ACK(routing_id_);
  response->WriteInt(input_event->type);
  response->WriteBool(processed);

  if (input_event->type == WebInputEvent::MouseMove &&
      paint_aggregator_.HasPendingUpdate()) {
    // We want to rate limit the input events in this case, so we'll wait for
    // painting to finish before ACKing this message.
    pending_input_event_ack_.reset(response);
  } else {
    Send(response);
  }

  handling_input_event_ = false;

  if (WebInputEvent::isKeyboardEventType(input_event->type))
    DidHandleKeyEvent();
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->mouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;
  if (webwidget_)
    webwidget_->setFocus(enable);
  if (enable) {
    // Force to retrieve the state of the focused widget to determine if we
    // should activate IMEs next time when this process calls the UpdateIME()
    // function.
    ime_control_updated_ = true;
    ime_control_new_state_ = true;
  }
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->setFocus(false);
}

void RenderWidget::PaintRect(const gfx::Rect& rect,
                             const gfx::Point& canvas_origin,
                             skia::PlatformCanvas* canvas) {
  canvas->save();

  // Bring the canvas into the coordinate system of the paint rect.
  canvas->translate(static_cast<SkScalar>(-canvas_origin.x()),
                    static_cast<SkScalar>(-canvas_origin.y()));

  // If there is a custom background, tile it.
  if (!background_.empty()) {
    SkPaint paint;
    SkShader* shader = SkShader::CreateBitmapShader(background_,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    paint.setShader(shader)->unref();
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    canvas->drawPaint(paint);
  }

  webwidget_->paint(webkit_glue::ToWebCanvas(canvas), rect);

  PaintDebugBorder(rect, canvas);

  // Flush to underlying bitmap.  TODO(darin): is this needed?
  canvas->getTopPlatformDevice().accessBitmap(false);

  canvas->restore();
}

void RenderWidget::PaintDebugBorder(const gfx::Rect& rect,
                                    skia::PlatformCanvas* canvas) {
  static bool kPaintBorder =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowPaintRects);
  if (!kPaintBorder)
    return;

  // Cycle through these colors to help distinguish new paint rects.
  const SkColor colors[] = {
    SkColorSetARGB(0x3F, 0xFF, 0, 0),
    SkColorSetARGB(0x3F, 0xFF, 0, 0xFF),
    SkColorSetARGB(0x3F, 0, 0, 0xFF),
  };
  static int color_selector = 0;

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(colors[color_selector++ % arraysize(colors)]);
  paint.setStrokeWidth(1);

  SkIRect irect;
  irect.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);
  canvas->drawIRect(irect, paint);
}

void RenderWidget::CallDoDeferredUpdate() {
  DoDeferredUpdate();

  if (pending_input_event_ack_.get()) {
    Send(pending_input_event_ack_.get());
    pending_input_event_ack_.release();
  }
}

void RenderWidget::DoDeferredUpdate() {
  if (!webwidget_ || !paint_aggregator_.HasPendingUpdate() ||
      update_reply_pending())
    return;

  // Suppress updating when we are hidden.
  if (is_hidden_ || size_.IsEmpty()) {
    paint_aggregator_.ClearPendingUpdate();
    needs_repainting_on_restore_ = true;
    return;
  }

  // Layout may generate more invalidation.
  webwidget_->layout();

  // OK, save the pending update to a local since painting may cause more
  // invalidation.  Some WebCore rendering objects only layout when painted.
  PaintAggregator::PendingUpdate update = paint_aggregator_.GetPendingUpdate();
  paint_aggregator_.ClearPendingUpdate();

  gfx::Rect scroll_damage = update.GetScrollDamage();
  gfx::Rect bounds = update.GetPaintBounds().Union(scroll_damage);

  // Compute a buffer for painting and cache it.
  scoped_ptr<skia::PlatformCanvas> canvas(
      RenderProcess::current()->GetDrawingCanvas(&current_paint_buf_, bounds));
  if (!canvas.get()) {
    NOTREACHED();
    return;
  }

  // We may get back a smaller canvas than we asked for.
  // TODO(darin): This seems like it could cause painting problems!
  DCHECK_EQ(bounds.width(), canvas->getDevice()->width());
  DCHECK_EQ(bounds.height(), canvas->getDevice()->height());
  bounds.set_width(canvas->getDevice()->width());
  bounds.set_height(canvas->getDevice()->height());

  HISTOGRAM_COUNTS_100("MPArch.RW_PaintRectCount", update.paint_rects.size());

  // TODO(darin): Re-enable painting multiple damage rects once the
  // page-cycler regressions are resolved.  See bug 29589.
  if (update.scroll_rect.IsEmpty()) {
    update.paint_rects.clear();
    update.paint_rects.push_back(bounds);
  }

  // The scroll damage is just another rectangle to paint and copy.
  std::vector<gfx::Rect> copy_rects;
  copy_rects.swap(update.paint_rects);
  if (!scroll_damage.IsEmpty())
    copy_rects.push_back(scroll_damage);

  for (size_t i = 0; i < copy_rects.size(); ++i)
    PaintRect(copy_rects[i], bounds.origin(), canvas.get());

  ViewHostMsg_UpdateRect_Params params;
  params.bitmap = current_paint_buf_->id();
  params.bitmap_rect = bounds;
  params.dx = update.scroll_delta.x();
  params.dy = update.scroll_delta.y();
  params.scroll_rect = update.scroll_rect;
  params.copy_rects.swap(copy_rects);  // TODO(darin): clip to bounds?
  params.view_size = size_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;

  update_reply_pending_ = true;
  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;

  UpdateIME();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetDelegate

void RenderWidget::didInvalidateRect(const WebRect& rect) {
  // We only want one pending DoDeferredUpdate call at any time...
  bool update_pending = paint_aggregator_.HasPendingUpdate();

  // The invalidated rect might be outside the bounds of the view.
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect damaged_rect = view_rect.Intersect(rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.InvalidateRect(damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (update_pending)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::CallDoDeferredUpdate));
}

void RenderWidget::didScrollRect(int dx, int dy, const WebRect& clip_rect) {
  // We only want one pending DoDeferredUpdate call at any time...
  bool update_pending = paint_aggregator_.HasPendingUpdate();

  // The scrolled rect might be outside the bounds of the view.
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect damaged_rect = view_rect.Intersect(clip_rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.ScrollRect(dx, dy, damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (update_pending)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::CallDoDeferredUpdate));
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor(cursor_info);

  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a constrained popup or as a new tab).
//
void RenderWidget::show(WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (!did_show_) {
    did_show_ = true;
    // NOTE: initial_pos_ may still have its default values at this point, but
    // that's okay.  It'll be ignored if as_popup is false, or the browser
    // process will impose a default position otherwise.
    if (popup_params_.get()) {
      popup_params_->bounds = initial_pos_;
      Send(new ViewHostMsg_ShowPopup(routing_id_, *popup_params_));
      popup_params_.reset();
    } else {
      Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_pos_));
    }
    SetPendingWindowRect(initial_pos_);
  }
}

void RenderWidget::didFocus() {
  // Note that didFocus() is invoked everytime a new node is focused in the
  // page.  It could be expected that it would be called only when the widget
  // gets the focus.  If the current behavior was to change in WebKit for the
  // expected one, the following notification would not work anymore.
  Send(new ViewHostMsg_FocusedNodeChanged(routing_id_));

  // Prevent the widget from stealing the focus if it does not have focus
  // already.  We do this by explicitely setting the focus to false again.
  // We only let the browser focus the renderer.
  if (!has_focus_ && webwidget_) {
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &RenderWidget::ClearFocus));
  }
}

void RenderWidget::didBlur() {
  Send(new ViewHostMsg_Blur(routing_id_));
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::closeWidgetSoon() {
  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::DoDeferredClose));
}

void RenderWidget::GenerateFullRepaint() {
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

void RenderWidget::Close() {
  if (webwidget_) {
    webwidget_->close();
    webwidget_ = NULL;
  }
}

WebRect RenderWidget::windowRect() {
  if (pending_window_rect_count_)
    return pending_window_rect_;

  gfx::Rect rect;
  Send(new ViewHostMsg_GetWindowRect(routing_id_, host_window_, &rect));
  return rect;
}

void RenderWidget::setWindowRect(const WebRect& pos) {
  if (did_show_) {
    Send(new ViewHostMsg_RequestMove(routing_id_, pos));
    SetPendingWindowRect(pos);
  } else {
    initial_pos_ = pos;
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

WebRect RenderWidget::rootWindowRect() {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    return pending_window_rect_;
  }

  gfx::Rect rect;
  Send(new ViewHostMsg_GetRootWindowRect(routing_id_, host_window_, &rect));
  return rect;
}

WebRect RenderWidget::windowResizerRect() {
  return resizer_rect_;
}

void RenderWidget::OnImeSetInputMode(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the IME attached to the browser process is active.
  ime_is_active_ = is_active;
}

void RenderWidget::OnImeSetComposition(WebCompositionCommand command,
                                       int cursor_position,
                                       int target_start, int target_end,
                                       const string16& ime_string) {
  if (!webwidget_)
    return;
  ime_control_busy_ = true;
  webwidget_->handleCompositionEvent(command, cursor_position,
                                     target_start, target_end,
                                     ime_string);
  ime_control_busy_ = false;
}

void RenderWidget::OnMsgRepaint(const gfx::Size& size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  set_next_paint_is_repaint_ack();
  gfx::Rect repaint_rect(size_to_paint.width(), size_to_paint.height());
  didInvalidateRect(repaint_rect);
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (!webwidget_)
    return;
  webwidget_->setTextDirection(direction);
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it.
  is_hidden_ = hidden;
  if (is_hidden_)
    render_thread_->WidgetHidden();
  else
    render_thread_->WidgetRestored();
}

void RenderWidget::SetBackground(const SkBitmap& background) {
  background_ = background;
  // Generate a full repaint.
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

bool RenderWidget::next_paint_is_restore_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_restore_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_restore_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
}

void RenderWidget::UpdateIME() {
  // If a browser process does not have IMEs, its IMEs are not active, or there
  // are not any attached widgets.
  // a renderer process does not have to retrieve information of the focused
  // control or send notification messages to a browser process.
  if (!ime_is_active_) {
    return;
  }
  // Retrieve the caret position from the focused widget and verify we should
  // enabled IMEs attached to the browser process.
  bool enable_ime = false;
  WebRect caret_rect;
  if (!webwidget_ ||
      !webwidget_->queryCompositionStatus(&enable_ime, &caret_rect)) {
    // There are not any editable widgets attached to this process.
    // We should disable the IME to prevent it from sending CJK strings to
    // non-editable widgets.
    ime_control_updated_ = true;
    ime_control_new_state_ = false;
  }
  if (ime_control_new_state_ != enable_ime) {
    ime_control_updated_ = true;
    ime_control_new_state_ = enable_ime;
  }
  if (ime_control_updated_) {
    // The input focus has been changed.
    // Compare the current state with the updated state and choose actions.
    if (ime_control_enable_ime_) {
      if (ime_control_new_state_) {
        // Case 1: a text input -> another text input
        // Complete the current composition and notify the caret position.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(),
                                             IME_COMPLETE_COMPOSITION,
                                             caret_rect));
      } else {
        // Case 2: a text input -> a password input (or a static control)
        // Complete the current composition and disable the IME.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(), IME_DISABLE,
                                             caret_rect));
      }
    } else {
      if (ime_control_new_state_) {
        // Case 3: a password input (or a static control) -> a text input
        // Enable the IME and notify the caret position.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(),
                                             IME_COMPLETE_COMPOSITION,
                                             caret_rect));
      } else {
        // Case 4: a password input (or a static contol) -> another password
        //         input (or another static control).
        // The IME has been already disabled and we don't have to do anything.
      }
    }
  } else {
    // The input focus is not changed.
    // Notify the caret position to a browser process only if it is changed.
    if (ime_control_enable_ime_) {
      if (caret_rect.x != ime_control_x_ ||
          caret_rect.y != ime_control_y_) {
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(), IME_MOVE_WINDOWS,
                                             caret_rect));
      }
    }
  }
  // Save the updated IME status to prevent from sending the same IPC messages.
  ime_control_updated_ = false;
  ime_control_enable_ime_ = ime_control_new_state_;
  ime_control_x_ = caret_rect.x;
  ime_control_y_ = caret_rect.y;
}

WebScreenInfo RenderWidget::screenInfo() {
  WebScreenInfo results;
  Send(new ViewHostMsg_GetScreenInfo(routing_id_, host_window_, &results));
  return results;
}

void RenderWidget::SchedulePluginMove(
    const webkit_glue::WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      if (move.rects_valid) {
        plugin_window_moves_[i] = move;
      } else {
        plugin_window_moves_[i].visible = move.visible;
      }
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

void RenderWidget::CleanupWindowInPluginMoves(gfx::PluginWindowHandle window) {
  for (WebPluginGeometryVector::iterator i = plugin_window_moves_.begin();
       i != plugin_window_moves_.end(); ++i) {
    if (i->window == window) {
      plugin_window_moves_.erase(i);
      break;
    }
  }
}
