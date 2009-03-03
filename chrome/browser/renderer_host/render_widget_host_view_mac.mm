// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/histogram.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/webinputevent.h"

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
      is_loading_(false),
      is_hidden_(false) {
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

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

  // TODO(avi): the WebContents object uses this method to size the newly
  // created widget to the correct size. At the time of this call, we're not yet
  // in the view hierarchy so |size| ends up being 0x0. However, this works for
  // us because we're using the Cocoa view struture and resizer flags to fix
  // things up as soon as the view gets added to the hierarchy. Figure out if we
  // want to keep this flow or switch back to the flow Windows uses which sets
  // the size upon creation. http://crbug.com/8285.
}

gfx::NativeView RenderWidgetHostViewMac::GetPluginNativeView() {
  // All plugin stuff is TBD. TODO(avi,awalker): fill in
  // http://crbug.com/8192
  return nil;
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

void RenderWidgetHostViewMac::Redraw(const gfx::Rect& rect) {
  [cocoa_view_ setNeedsDisplayInRect:[cocoa_view_ RectToNSRect:rect]];
}

void RenderWidgetHostViewMac::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  Redraw(rect);
}

void RenderWidgetHostViewMac::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;

  [cocoa_view_ scrollRect:[cocoa_view_ RectToNSRect:rect]
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

void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite.
    if (tooltip_text_.length() > kMaxTooltipLength)
      tooltip_text_ = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(tooltip_text_);
    [cocoa_view_ setToolTip:tooltip_nsstring];
  }
}

BackingStore* RenderWidgetHostViewMac::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(size);
}

void RenderWidgetHostViewMac::ShutdownHost() {
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

@implementation RenderWidgetHostViewCocoa

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    renderWidgetHostView_ = r;
  }
  return self;
}

- (void)dealloc {
  delete renderWidgetHostView_;
  
  [super dealloc];
}

- (void)mouseEvent:(NSEvent *)theEvent {
  WebMouseEvent event(theEvent, self);
  renderWidgetHostView_->render_widget_host()->ForwardMouseEvent(event);
}

- (void)keyEvent:(NSEvent *)theEvent {
  WebKeyboardEvent event(theEvent);
  renderWidgetHostView_->render_widget_host()->ForwardKeyboardEvent(event);
}

- (void)scrollWheel:(NSEvent *)theEvent {
  WebMouseWheelEvent event(theEvent, self);
  renderWidgetHostView_->render_widget_host()->ForwardWheelEvent(event);
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  renderWidgetHostView_->render_widget_host()->WasResized();
}

- (void)drawRect:(NSRect)dirtyRect {
  DCHECK(renderWidgetHostView_->render_widget_host()->process()->channel());

  BackingStore* backing_store =
      renderWidgetHostView_->render_widget_host()->GetBackingStore();
  skia::PlatformCanvas* canvas = backing_store->canvas();

  if (backing_store) {
    gfx::Rect damaged_rect([self NSRectToRect:dirtyRect]);

    gfx::Rect bitmap_rect(0, 0,
                          backing_store->size().width(),
                          backing_store->size().height());

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      if ([self lockFocusIfCanDraw]) {
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);

        CGRect damaged_rect_cg = damaged_rect.ToCGRect();
        NSRect damaged_rect_ns = [self RectToNSRect:damaged_rect];
        canvas->getTopPlatformDevice().DrawToContext(
            context, damaged_rect_ns.origin.x, damaged_rect_ns.origin.y,
            &damaged_rect_cg);
        
        [self unlockFocus];
      }
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
    if (!renderWidgetHostView_->whiteout_start_time().is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time();
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time() = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time().is_null())
      renderWidgetHostView_->whiteout_start_time() = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  return YES;  // TODO(avi): be smarter
}

- (BOOL)acceptsFirstResponder {
  return YES;  // TODO(avi): be smarter
}

- (BOOL)becomeFirstResponder {
  renderWidgetHostView_->render_widget_host()->Focus();
  
  return YES;
}

- (BOOL)resignFirstResponder {
  renderWidgetHostView_->render_widget_host()->Blur();
  
  return YES;
}

@end
