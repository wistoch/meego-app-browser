// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CALayer.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/cocoa/base_view.h"
#include "chrome/browser/renderer_host/accelerated_surface_container_manager_mac.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webmenuitem.h"

class RenderWidgetHostViewMac;
class RWHVMEditCommandHelper;
@class ToolTip;

@protocol RenderWidgetHostViewMacOwner
- (RenderWidgetHostViewMac*)renderWidgetHostViewMac;
@end

// This is the view that lives in the Cocoa view hierarchy. In Windows-land,
// RenderWidgetHostViewWin is both the view and the delegate. We split the roles
// but that means that the view needs to own the delegate and will dispose of it
// when it's removed from the view system.

@interface RenderWidgetHostViewCocoa
    : BaseView <RenderWidgetHostViewMacOwner, NSTextInput, NSChangeSpelling> {
 @private
  scoped_ptr<RenderWidgetHostViewMac> renderWidgetHostView_;
  BOOL canBeKeyView_;
  BOOL closeOnDeactivate_;
  scoped_ptr<RWHVMEditCommandHelper> editCommand_helper_;

  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id trackingRectOwner_;              // (not retained)
  void *trackingRectUserData_;
  NSTrackingRectTag lastToolTipTag_;
  scoped_nsobject<NSString> toolTip_;

  // Is YES if there was a mouse-down as yet unbalanced with a mouse-up.
  BOOL hasOpenMouseDown_;

  NSWindow* lastWindow_;  // weak

  // The Core Animation layer, if any, hosting the accelerated plugins' output.
  scoped_nsobject<CALayer> acceleratedPluginLayer_;

  // Variables used by our implementaion of the NSTextInput protocol.
  // An input method of Mac calls the methods of this protocol not only to
  // notify an application of its status, but also to retrieve the status of
  // the application. That is, an application cannot control an input method
  // directly.
  // This object keeps the status of a composition of the renderer and returns
  // it when an input method asks for it.
  // We need to implement Objective-C methods for the NSTextInput protocol. On
  // the other hand, we need to implement a C++ method for an IPC-message
  // handler which receives input-method events from the renderer.

  // Represents the input-method attributes supported by this object.
  scoped_nsobject<NSArray> validAttributesForMarkedText_;

  // Represents the cursor position in this view coordinate.
  // The renderer sends the cursor position through an IPC message.
  // We save the latest cursor position here and return it when an input
  // methods needs it.
  NSRect caretRect_;

  // Indicates if we are currently handling a key down event.
  BOOL handlingKeyDown_;

  // Indicates if there is any marked text.
  BOOL hasMarkedText_;

  // The range of current marked text inside the whole content of the DOM node
  // being edited.
  // TODO(suzhe): This is currently a fake value, as we do not support accessing
  // the whole content yet.
  NSRange markedRange_;

  // The selected range inside current marked text.
  // TODO(suzhe): Currently it's only valid when there is any marked text.
  // In the future, we may need to support accessing the whole content of the
  // DOM node being edited, then this should be the selected range inside the
  // DOM node.
  NSRange selectedRange_;

  // Text to be inserted which was generated by handling a key down event.
  string16 textToBeInserted_;

  // New marked text which was generated by handling a key down event.
  string16 newMarkedText_;
}

@property(assign, nonatomic) NSRect caretRect;

- (void)setCanBeKeyView:(BOOL)can;
- (void)setCloseOnDeactivate:(BOOL)b;
- (void)setToolTipAtMousePoint:(NSString *)string;
// Makes sure that the initial layer setup for accelerated plugin drawing has
// been done. Can be called multiple times.
- (void)ensureAcceleratedPluginLayer;
// Triggers a refresh of the accelerated plugin layer; should be called whenever
// the shared surface for one of the plugins is updated.
- (void)drawAcceleratedPluginLayer;
// Set frame, then notify the RenderWidgetHost that the frame has been changed,
// but do it in a separate task, using |performSelector:withObject:afterDelay:|.
// This stops the flickering issue in http://crbug.com/31970
- (void)setFrameWithDeferredUpdate:(NSRect)frame;
// Notify the RenderWidgetHost that the frame was updated so it can resize
// its contents.
- (void)renderWidgetHostWasResized;
// Cancel ongoing composition (abandon the marked text).
- (void)cancelComposition;

@end

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the Cocoa view system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
class RenderWidgetHostViewMac : public RenderWidgetHostView {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  explicit RenderWidgetHostViewMac(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewMac();

  RenderWidgetHostViewCocoa* native_view() const { return cocoa_view_; }

  // Implementation of RenderWidgetHostView:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos);
  virtual RenderWidgetHost* GetRenderWidgetHost() const;
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void SetSize(const gfx::Size& size);
  virtual gfx::NativeView GetNativeView();
  virtual void MovePluginWindows(
      const std::vector<webkit_glue::WebPluginGeometry>& moves);
  virtual void Focus();
  virtual void Blur();
  virtual bool HasFocus();
  virtual void Show();
  virtual void Hide();
  virtual bool IsShowing();
  virtual gfx::Rect GetViewBounds() const;
  virtual void UpdateCursor(const WebCursor& cursor);
  virtual void SetIsLoading(bool is_loading);
  virtual void IMEUpdateStatus(int control, const gfx::Rect& caret_rect);
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects);
  virtual void RenderViewGone();
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) {};
  virtual void Destroy();
  virtual void SetTooltipText(const std::wstring& tooltip_text);
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);
  virtual VideoLayer* AllocVideoLayer(const gfx::Size& size);
  virtual void ShowPopupWithItems(gfx::Rect bounds,
                                  int item_height,
                                  double item_font_size,
                                  int selected_item,
                                  const std::vector<WebMenuItem>& items,
                                  bool right_aligned);
  virtual gfx::Rect GetWindowRect();
  virtual gfx::Rect GetRootWindowRect();
  virtual void SetActive(bool active);
  virtual void SetWindowVisibility(bool visible);
  virtual void WindowFrameChanged();
  virtual void SetBackground(const SkBitmap& background);
  virtual bool ContainsNativeView(gfx::NativeView native_view) const;

  // Methods associated with GPU-accelerated plug-in instances.
  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(bool opaque);
  virtual void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window);
  virtual void AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                              int32 width,
                                              int32 height,
                                              uint64 io_surface_identifier);
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib);
  virtual void AcceleratedSurfaceBuffersSwapped(gfx::PluginWindowHandle window);
  // Draws the current GPU-accelerated plug-in instances into the given context.
  virtual void DrawAcceleratedSurfaceInstances(CGLContextObj context);
  // Informs the plug-in instances that their drawing context has changed.
  virtual void AcceleratedSurfaceContextChanged();

  virtual void SetVisuallyDeemphasized(bool deemphasized);

  void KillSelf();

  void set_parent_view(BaseView* parent_view) { parent_view_ = parent_view; }

  // These member variables should be private, but the associated ObjC class
  // needs access to them and can't be made a friend.

  // The associated Model.  Can be NULL if Destroy() is called when
  // someone (other than superview) has retained |cocoa_view_|.
  RenderWidgetHost* render_widget_host_;

  // This is true when we are currently painting and thus should handle extra
  // paint requests by expanding the invalid rect rather than actually painting.
  bool about_to_validate_and_paint_;

  // This is true when we have already scheduled a call to
  // |-callSetNeedsDisplayInRect:| but it has not been fulfilled yet.  Used to
  // prevent us from scheduling multiple calls.
  bool call_set_needs_display_in_rect_pending_;

  // The invalid rect that needs to be painted by callSetNeedsDisplayInRect.
  // This value is only meaningful when
  // |call_set_needs_display_in_rect_pending_| is true.
  NSRect invalid_rect_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks tab_switch_paint_time_;

 private:
  // Updates the display cursor to the current cursor if the cursor is over this
  // render view.
  void UpdateCursorIfOverSelf();

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  // The associated view. This is weak and is inserted into the view hierarchy
  // to own this RenderWidgetHostViewMac object unless is_popup_menu_ is true.
  // In that case, cocoa_view_ is never inserted into the view hierarchy, so
  // the RenderWidgetHostViewMac will treat it as a strong reference and will
  // release it when told to destroy (for example, because a pop-up menu has
  // closed).
  RenderWidgetHostViewCocoa* cocoa_view_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Indicates if the page is loading.
  bool is_loading_;

  // true if the View is not visible.
  bool is_hidden_;

  // True if the widget is a native popup menu.  The renderer code calls this
  // an "external popup."
  bool is_popup_menu_;

  // The text to be shown in the tooltip, supplied by the renderer.
  std::wstring tooltip_text_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  ScopedRunnableMethodFactory<RenderWidgetHostViewMac> shutdown_factory_;

  // Used for positioning a popup menu.
  BaseView* parent_view_;

  // Helper class for managing instances of accelerated plug-ins.
  AcceleratedSurfaceContainerManagerMac plugin_container_manager_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMac);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
