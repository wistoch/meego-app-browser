// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/histogram.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "skia/ext/platform_canvas.h"
#import "third_party/mozilla/include/ToolTip.h"
#include "webkit/api/public/mac/WebInputEventFactory.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/glue/webmenurunner_mac.h"

using WebKit::WebInputEventFactory;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

@interface RenderWidgetHostViewCocoa (Private)
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
@end

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

}

// RenderWidgetHostView --------------------------------------------------------

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : render_widget_host_(widget),
      about_to_validate_and_paint_(false),
      is_loading_(false),
      is_hidden_(false),
      shutdown_factory_(this),
      parent_view_(NULL) {
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  [parent_host_view->GetNativeView() addSubview:cocoa_view_];
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable_ ? YES : NO];

  // TODO(avi):Why the hell are these screen coordinates? The Windows code calls
  // ::MoveWindow() which indicates they should be local, but when running it I
  // get global ones instead!

  NSPoint global_origin = NSPointFromCGPoint(pos.origin().ToCGPoint());
  global_origin.y = [[[cocoa_view_ window] screen] frame].size.height -
      pos.height() - global_origin.y;
  NSPoint window_origin =
      [[cocoa_view_ window] convertScreenToBase:global_origin];
  NSPoint view_origin =
      [cocoa_view_ convertPoint:window_origin fromView:nil];
  NSRect initial_frame = NSMakeRect(view_origin.x,
                                    view_origin.y,
                                    pos.width(),
                                    pos.height());
  [cocoa_view_ setFrame:initial_frame];
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  render_widget_host_->WasRestored();
}

void RenderWidgetHostViewMac::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  if (is_hidden_)
    return;

  // TODO(avi): the TabContents object uses this method to size the newly
  // created widget to the correct size. At the time of this call, we're not yet
  // in the view hierarchy so |size| ends up being 0x0. However, this works for
  // us because we're using the Cocoa view struture and resizer flags to fix
  // things up as soon as the view gets added to the hierarchy. Figure out if we
  // want to keep this flow or switch back to the flow Windows uses which sets
  // the size upon creation. http://crbug.com/8285.
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() {
  return native_view();
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  // All plugin stuff is TBD. TODO(avi,awalker): fill in
  // http://crbug.com/8192
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostViewMac::Blur() {
  [[cocoa_view_ window] makeFirstResponder:nil];
}

bool RenderWidgetHostViewMac::HasFocus() {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

void RenderWidgetHostViewMac::Show() {
  [cocoa_view_ setHidden:NO];

  DidBecomeSelected();
}

void RenderWidgetHostViewMac::Hide() {
  [cocoa_view_ setHidden:YES];

  WasHidden();
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  return [cocoa_view_ NSRectToRect:[cocoa_view_ bounds]];
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::UpdateCursorIfOverSelf() {
  // Do something special (as Win Chromium does) for arrow cursor while loading
  // a page? TODO(avi): decide
  // Can we synchronize to the event stream? Switch to -[NSWindow
  // mouseLocationOutsideOfEventStream] if we cannot. TODO(avi): test and see
  NSEvent* event = [[cocoa_view_ window] currentEvent];
  if ([event window] != [cocoa_view_ window])
    return;

  NSPoint event_location = [event locationInWindow];
  NSPoint local_point = [cocoa_view_ convertPoint:event_location fromView:nil];

  if (!NSPointInRect(local_point, [cocoa_view_ bounds]))
    return;

  NSCursor* ns_cursor = current_cursor_.GetCursor();
  [ns_cursor set];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMac::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  NSRect ns_rect = [cocoa_view_ RectToNSRect:rect];

  if (about_to_validate_and_paint_) {
    // As much as we'd like to use -setNeedsDisplayInRect: here, we can't. We're
    // in the middle of executing a -drawRect:, and as soon as it returns Cocoa
    // will clear its record of what needs display. If we want to handle the
    // recursive drawing, we need to do it ourselves.
    invalid_rect_ = NSUnionRect(invalid_rect_, ns_rect);
  } else {
    [cocoa_view_ setNeedsDisplayInRect:ns_rect];
    [cocoa_view_ displayIfNeeded];
  }
}

void RenderWidgetHostViewMac::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;

  // Before asking the cocoa view to scroll, shorten the rect's bounds
  // by the amount we are scrolling.  This will prevent us from moving
  // data beyond the bounds of the original rect, which in turn
  // prevents us from accidentally drawing over other parts of the
  // page (scrolbars, other frames, etc).
  gfx::Rect scroll_rect = rect;
  scroll_rect.Inset(dx < 0 ? -dx : 0,
                    dy < 0 ? -dy : 0,
                    dx > 0 ? dx : 0,
                    dy > 0 ? dy : 0);
  [cocoa_view_ scrollRect:[cocoa_view_ RectToNSRect:scroll_rect]
                       by:NSMakeSize(dx, -dy)];

  gfx::Rect new_rect = rect;
  new_rect.Offset(dx, dy);
  gfx::Rect dirty_rect = rect.Subtract(new_rect);
  [cocoa_view_ setNeedsDisplayInRect:[cocoa_view_ RectToNSRect:dirty_rect]];
}

void RenderWidgetHostViewMac::RenderViewGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work. We cannot simply use [-NSView setToolTip:] because NSView
// can't handle the case where the tooltip text changes while the mouse is
// still inside the view. Since the page elements that get tooltips are all
// contained within this view (and are unknown to the NSView system), we
// are forced to implement our own tooltips with child windows.
// TODO(pinkerton): Do we want these tooltips to time out after a certain time?
// Gecko does this automatically in the back-end, hence the ToolTip class not
// needing that functionality. We can either modify ToolTip or add this
// functionality here with a timer.
// TODO(pinkerton): This code really needs to live at a higher level because
// right now it allows multiple views in multiple tabs to each be displaying
// a tooltip simultaneously (http://crbug.com/14178).
void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    std::wstring display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(display_text);
    if ([tooltip_nsstring length] == 0) {
      tooltip_.reset(NULL);  // The dtor closes the tooltip.
    } else {
      // Get the current mouse location in the window's coordinate system and
      // use that as the point for displaying the tooltip.
      if (!tooltip_.get())
        tooltip_.reset([[ToolTip alloc] init]);
      NSPoint event_point =
          [[cocoa_view_ window] mouseLocationOutsideOfEventStream];
      [tooltip_ showToolTipAtPoint:event_point
                        withString:tooltip_nsstring
                        overWindow:[cocoa_view_ window]];
    }
  }
}

BackingStore* RenderWidgetHostViewMac::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(render_widget_host_, size);
}

// Display a popup menu for WebKit using Cocoa widgets.
void RenderWidgetHostViewMac::ShowPopupWithItems(
    gfx::Rect bounds,
    int item_height,
    int selected_item,
    const std::vector<WebMenuItem>& items) {
  NSRect view_rect = [cocoa_view_ bounds];
  NSRect parent_rect = [parent_view_ bounds];
  int y_offset = bounds.y() + bounds.height();
  NSRect position = NSMakeRect(bounds.x(), parent_rect.size.height - y_offset,
                               bounds.width(), bounds.height());

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items]);

  [menu_runner runMenuInView:parent_view_
                  withBounds:position
                initialIndex:selected_item];

  int window_num = [[parent_view_ window] windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Simulate a menu selection event.
    const WebMouseEvent& mouse_event =
        WebInputEventFactory::mouseEvent(event, cocoa_view_);
    render_widget_host_->ForwardMouseEvent(mouse_event);
  } else {
    // Simulate a menu dismiss event.
    NativeWebKeyboardEvent keyboard_event(event);
    render_widget_host_->ForwardKeyboardEvent(keyboard_event);
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (shutdown_factory_.empty()) {
    [cocoa_view_ setHidden:YES];
    MessageLoop::current()->PostTask(FROM_HERE,
        shutdown_factory_.NewRunnableMethod(
            &RenderWidgetHostViewMac::ShutdownHost));
  }
}

void RenderWidgetHostViewMac::ShutdownHost() {
  shutdown_factory_.RevokeAll();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

@implementation RenderWidgetHostViewCocoa

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    editCommand_helper_.reset(new RWHVMEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_ = r;
    canBeKeyView_ = YES;
    closeOnDeactivate_ = NO;
  }
  return self;
}

- (void)dealloc {
  delete renderWidgetHostView_;

  [super dealloc];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)mouseEvent:(NSEvent *)theEvent {
  const WebMouseEvent& event =
      WebInputEventFactory::mouseEvent(theEvent, self);
  renderWidgetHostView_->render_widget_host_->ForwardMouseEvent(event);
}

- (void)keyEvent:(NSEvent *)theEvent {
  // TODO(avi): Possibly kill self? See RenderWidgetHostViewWin::OnKeyEvent and
  // http://b/issue?id=1192881 .

  NativeWebKeyboardEvent event(theEvent);
  renderWidgetHostView_->render_widget_host_->ForwardKeyboardEvent(event);
}

- (void)scrollWheel:(NSEvent *)theEvent {
  const WebMouseWheelEvent& event =
      WebInputEventFactory::mouseWheelEvent(theEvent, self);
  renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(event);
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)drawRect:(NSRect)dirtyRect {
  DCHECK(renderWidgetHostView_->render_widget_host_->process()->channel());
  DCHECK(!renderWidgetHostView_->about_to_validate_and_paint_);

  renderWidgetHostView_->invalid_rect_ = dirtyRect;
  renderWidgetHostView_->about_to_validate_and_paint_ = true;
  BackingStore* backing_store =
      renderWidgetHostView_->render_widget_host_->GetBackingStore(true);
  skia::PlatformCanvas* canvas = backing_store->canvas();
  renderWidgetHostView_->about_to_validate_and_paint_ = false;
  dirtyRect = renderWidgetHostView_->invalid_rect_;

  if (backing_store) {
    gfx::Rect damaged_rect([self NSRectToRect:dirtyRect]);

    gfx::Rect bitmap_rect(0, 0,
                          backing_store->size().width(),
                          backing_store->size().height());

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      CGContextRef context = static_cast<CGContextRef>(
          [[NSGraphicsContext currentContext] graphicsPort]);

      CGRect paint_rect_cg = paint_rect.ToCGRect();
      NSRect paint_rect_ns = [self RectToNSRect:paint_rect];
      canvas->getTopPlatformDevice().DrawToContext(
          context, paint_rect_ns.origin.x, paint_rect_ns.origin.y,
          &paint_rect_cg);
    }

    // Fill the remaining portion of the damaged_rect with white
    if (damaged_rect.right() > bitmap_rect.right()) {
      NSRect r;
      r.origin.x = std::max(bitmap_rect.right(), damaged_rect.x());
      r.origin.y = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      r.size.width = damaged_rect.right() - r.origin.x;
      r.size.height = damaged_rect.y() - r.origin.y;
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      NSRect r;
      r.origin.x = damaged_rect.x();
      r.origin.y = damaged_rect.bottom();
      r.size.width = damaged_rect.right() - r.origin.x;
      r.size.height = std::max(bitmap_rect.bottom(), damaged_rect.y()) -
          r.origin.y;
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (!renderWidgetHostView_->whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time_.is_null())
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  return canBeKeyView_;
}

- (BOOL)becomeFirstResponder {
  if (![self superview]) {
    // We're dead, so becoming first responder is probably a bad idea.
    return NO;
  }

  renderWidgetHostView_->render_widget_host_->Focus();
  return YES;
}

- (BOOL)resignFirstResponder {
  if (![self superview]) {
    // We're dead, so touching renderWidgetHostView_ is probably a bad
    // idea.
    return YES;
  }

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->render_widget_host_->Blur();

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_;
}

@end

