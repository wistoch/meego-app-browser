// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_mac.h"

#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#include "chrome/browser/cocoa/sad_tab_view.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"

#include "chrome/common/temp_scaffolding_stubs.h"

@interface WebContentsViewCocoa (Private)
- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w;
- (void)processKeyboardEvent:(NSEvent*)event;
@end

// static
WebContentsView* WebContentsView::Create(WebContents* web_contents) {
  return new WebContentsViewMac(web_contents);
}

WebContentsViewMac::WebContentsViewMac(WebContents* web_contents)
    : WebContentsView(web_contents) {
  registrar_.Add(this, NotificationType::WEB_CONTENTS_CONNECTED,
                 Source<WebContents>(web_contents));
  registrar_.Add(this, NotificationType::WEB_CONTENTS_DISCONNECTED,
                 Source<WebContents>(web_contents));
}

WebContentsViewMac::~WebContentsViewMac() {
}

void WebContentsViewMac::CreateView() {
  WebContentsViewCocoa* view =
      [[WebContentsViewCocoa alloc] initWithWebContentsViewMac:this];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* WebContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewMac* view =
      new RenderWidgetHostViewMac(render_widget_host);

  // Fancy layout comes later; for now just make it our size and resize it
  // with us.
  NSView* view_view = view->native_view();
  [cocoa_view_.get() addSubview:view_view];
  [view_view setFrame:[cocoa_view_.get() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  return view;
}

gfx::NativeView WebContentsViewMac::GetNativeView() const {
  return cocoa_view_.get();
}

gfx::NativeView WebContentsViewMac::GetContentNativeView() const {
  if (!web_contents()->render_widget_host_view())
    return NULL;
  return web_contents()->render_widget_host_view()->GetPluginNativeView();
}

gfx::NativeWindow WebContentsViewMac::GetTopLevelNativeWindow() const {
  return [cocoa_view_.get() window];
}

void WebContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  *out = [cocoa_view_.get() NSRectToRect:[cocoa_view_.get() bounds]];
}

void WebContentsViewMac::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();

  // Until we have d'n'd implemented, just immediately pretend we're
  // already done with the drag and drop so we don't get stuck
  // thinking we're in mid-drag.
  // TODO(port): remove me when the above NOTIMPLEMENTED is fixed.
  if (web_contents()->render_view_host())
    web_contents()->render_view_host()->DragSourceSystemDragEnded();
}

void WebContentsViewMac::OnContentsDestroy() {
  // TODO(avi):Close the find bar if any.
  if (find_bar_.get())
    find_bar_->Close();
}

void WebContentsViewMac::SetPageTitle(const std::wstring& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewMac::Invalidate() {
  [cocoa_view_.get() setNeedsDisplay:YES];
}

void WebContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See web_contents_view.h.
  NOTIMPLEMENTED();  // Leaving the hack unimplemented.
}

void WebContentsViewMac::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  if (!find_bar_.get()) {
    // We want the Chrome top-level (Frame) window.
    NSWindow* window =
        static_cast<NSWindow*>(browser.window()->GetNativeHandle());
    find_bar_.reset(new FindBarMac(this, window));
  } else {
    find_bar_->Show();
  }

  if (find_next && !find_bar_->find_string().empty())
    find_bar_->StartFinding(forward_direction);
}

void WebContentsViewMac::HideFindBar(bool end_session) {
  if (find_bar_.get()) {
    if (end_session)
      find_bar_->EndFindSession();
    else
      find_bar_->DidBecomeUnselected();
  }
}

bool WebContentsViewMac::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  if (!find_bar_.get() ||
      [find_bar_->GetView() isHidden]) {
    *position = gfx::Point(0, 0);
    *fully_visible = false;
    return false;
  }

  NSRect frame = [find_bar_->GetView() frame];
  *position = gfx::Point(frame.origin.x, frame.origin.y);
  *fully_visible = find_bar_->IsVisible() && !find_bar_->IsAnimating();
  return true;
}

void WebContentsViewMac::Focus() {
}

void WebContentsViewMac::SetInitialFocus() {
  // TODO(port)
}

void WebContentsViewMac::StoreFocus() {
  // TODO(port)
}

void WebContentsViewMac::RestoreFocus() {
  // TODO(port)
}

void WebContentsViewMac::SetChildSize(RenderWidgetHostView* rwh_view) {
  rwh_view->SetSize(GetContainerSize());
}

void WebContentsViewMac::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void WebContentsViewMac::TakeFocus(bool reverse) {
  [cocoa_view_.get() becomeFirstResponder];
}

void WebContentsViewMac::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  [cocoa_view_.get() processKeyboardEvent:event.os_event];
}

void WebContentsViewMac::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  if (find_bar_.get()) {
    find_bar_->OnFindReply(request_id, number_of_matches, selection_rect,
                           active_match_ordinal, final_update);
  }
}

void WebContentsViewMac::ShowContextMenu(const ContextMenuParams& params) {
  RenderViewContextMenuMac menu(web_contents(),
                                params,
                                GetNativeView());
}

RenderWidgetHostView* WebContentsViewMac::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosted.
  RenderWidgetHostView* widget_view =
      WebContentsView::CreateNewWidgetInternal(route_id, activatable);
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_view);
  [widget_view_mac->native_view() retain];

  return widget_view;
}

void WebContentsViewMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  WebContentsView::ShowCreatedWidgetInternal(widget_host_view, initial_pos);

  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the retain we
  // took in CreateNewWidgetInternal().
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_host_view);
  [widget_view_mac->native_view() release];
}

void WebContentsViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::WEB_CONTENTS_CONNECTED: {
      if (sad_tab_.get()) {
        [sad_tab_.get() removeFromSuperview];
        sad_tab_.reset();
      }
      break;
    }
    case NotificationType::WEB_CONTENTS_DISCONNECTED: {
      SadTabView* view = [[SadTabView alloc] initWithFrame:NSZeroRect];
      sad_tab_.reset(view);

      // Set as the dominant child.
      [cocoa_view_.get() addSubview:view];
      [view setFrame:[cocoa_view_.get() bounds]];
      [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

@implementation WebContentsViewCocoa

- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    webContentsView_ = w;
  }
  return self;
}

- (void)processKeyboardEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown)
    [super keyDown:event];
  else if ([event type] == NSKeyUp)
    [super keyUp:event];
}

- (void)mouseEvent:(NSEvent *)theEvent {
  if (webContentsView_->web_contents()->delegate()) {
    if ([theEvent type] == NSMouseMoved)
      webContentsView_->web_contents()->delegate()->
          ContentsMouseEvent(webContentsView_->web_contents(), true);
    if ([theEvent type] == NSMouseExited)
      webContentsView_->web_contents()->delegate()->
          ContentsMouseEvent(webContentsView_->web_contents(), false);
  }
}

// In the Windows version, we always have cut/copy/paste enabled. This is sub-
// optimal, but we do it too. TODO(avi): Plumb the "can*" methods up from
// WebCore.

- (void)cut:(id)sender {
  webContentsView_->web_contents()->Cut();
}

- (void)copy:(id)sender {
  webContentsView_->web_contents()->Copy();
}

- (void)paste:(id)sender {
  webContentsView_->web_contents()->Paste();
}

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

@end
