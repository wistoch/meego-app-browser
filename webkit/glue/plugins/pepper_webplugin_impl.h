// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_IMPL_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_IMPL_H_

#include <string>
#include <vector>

#include "base/weak_ptr.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPlugin.h"

namespace WebKit {
class WebFrame;
struct WebPluginParams;
}

namespace pepper {

class PluginDelegate;
class PluginInstance;
class PluginModule;

class WebPluginImpl : public WebKit::WebPlugin {
 public:
  WebPluginImpl(PluginModule* module,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const base::WeakPtr<PluginDelegate>& plugin_delegate);
  ~WebPluginImpl();

  // WebKit::WebPlugin implementation.
  virtual bool initialize(WebKit::WebPluginContainer* container);
  virtual void destroy();
  virtual NPObject* scriptableObject();
  virtual void paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect,
      const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs_rects,
      bool is_visible);
  virtual void updateFocus(bool focused);
  virtual void updateVisibility(bool visible);
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo& cursor_info);
  virtual void didReceiveResponse(const WebKit::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const WebKit::WebURLError&);
  virtual void didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                            void* notify_data);
  virtual void didFailLoadingFrameRequest(const WebKit::WebURL& url,
                                          void* notify_data,
                                          const WebKit::WebURLError& error);

 public:
  struct InitData {
    scoped_refptr<PluginModule> module;
    base::WeakPtr<PluginDelegate> delegate;
    std::vector<std::string> arg_names;
    std::vector<std::string> arg_values;
  };

  scoped_ptr<InitData> init_data_;  // Cleared upon successful initialization.
  scoped_refptr<PluginInstance> instance_;
  WebKit::WebPluginContainer* container_;  // Can be NULL.
  gfx::Rect plugin_rect_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_WEBPLUGIN_IMPL_H_
