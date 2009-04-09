// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "chrome/browser/renderer_host/render_view_host_delegate.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "chrome/browser/views/hwnd_html_view.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Extension;
struct WebPreferences;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// priviliges available to extensions.  The view may be drawn to the screen or
// hidden.
class ExtensionView : public HWNDHtmlView,
                      public RenderViewHostDelegate {
 public:
  ExtensionView(Extension* extension, const GURL& url, SiteInstance* instance);

  // HWNDHtmlView
  virtual void CreatingRenderer();

  // RenderViewHostDelegate
  // TODO(mpcomplete): GetProfile is unused.
  virtual Profile* GetProfile() const { return NULL; }
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void DidContentsPreferredWidthChange(const int pref_width);
  virtual void DidStopLoading(RenderViewHost* render_view_host,
      int32 page_id);
  virtual WebPreferences GetWebkitPrefs();
  virtual void RunJavaScriptMessage(
      const std::wstring& message,
      const std::wstring& default_prompt,
      const GURL& frame_url,
      const int flags,
      IPC::Message* reply_msg,
      bool* did_suppress_message);
  virtual void DidStartLoading(RenderViewHost* render_view_host,
                               int32 page_id);

  Extension* extension() { return extension_; }
 private:
  // The extension that we're hosting in this view.
  Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
