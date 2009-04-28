// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "skia/include/SkBitmap.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "chrome/browser/views/hwnd_html_view.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Browser;
class Extension;
class ExtensionFunctionDispatcher;
class RenderWidgetHost;
class RenderWidgetHostView;
class WebContents;
struct WebPreferences;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// priviliges available to extensions.  The view may be drawn to the screen or
// hidden.
class ExtensionView : public HWNDHtmlView,
                      public RenderViewHostDelegate,
                      public RenderViewHostDelegate::View {
 public:
  // ExtensionView
  ExtensionView(Extension* extension,
                const GURL& url,
                SiteInstance* instance,
                Browser* browser);

  Extension* extension() { return extension_; }

  // HWNDHtmlView
  virtual void CreatingRenderer();

  virtual void SetBackground(const SkBitmap& background);

  // RenderViewHostDelegate
  // TODO(mpcomplete): GetProfile is unused.
  virtual Profile* GetProfile() const { return NULL; }
  virtual ExtensionFunctionDispatcher *CreateExtensionFunctionDispatcher(
      RenderViewHost *render_view_host,
      const std::string& extension_id);
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void DidContentsPreferredWidthChange(const int pref_width);
  virtual void DidStopLoading(RenderViewHost* render_view_host);
  virtual WebPreferences GetWebkitPrefs();
  virtual void RunJavaScriptMessage(
      const std::wstring& message,
      const std::wstring& default_prompt,
      const GURL& frame_url,
      const int flags,
      IPC::Message* reply_msg,
      bool* did_suppress_message);
  virtual void DidStartLoading(RenderViewHost* render_view_host);
  virtual RenderViewHostDelegate::View* GetViewDelegate() const;

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event);
  virtual void CreateNewWidget(int route_id, bool activatable);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

 private:
  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  // The extension that we're hosting in this view.
  Extension* extension_;

  // The browser window that this view is in.
  Browser* browser_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  int pending_preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
