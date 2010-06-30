// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "gfx/rect.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"

typedef struct _pp_Var PP_Var;
typedef struct _ppb_Instance PPB_Instance;
typedef struct _ppp_Instance PPP_Instance;

namespace gfx {
class Rect;
}

namespace WebKit {
struct WebCursorInfo;
class WebInputEvent;
class WebPluginContainer;
}

namespace pepper {

class DeviceContext2D;
class PluginDelegate;
class PluginModule;

class PluginInstance : public base::RefCounted<PluginInstance> {
 public:
  PluginInstance(PluginDelegate* delegate,
                 PluginModule* module,
                 const PPP_Instance* instance_interface);
  ~PluginInstance();

  static const PPB_Instance* GetInterface();

  // Converts the given instance ID to an actual instance object.
  static PluginInstance* FromPPInstance(PP_Instance instance);

  PluginDelegate* delegate() const { return delegate_; }
  PluginModule* module() const { return module_.get(); }

  WebKit::WebPluginContainer* container() const { return container_; }

  const gfx::Rect& position() const { return position_; }
  const gfx::Rect& clip() const { return clip_; }

  PP_Instance GetPPInstance();

  // Paints the current backing store to the web page.
  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  // Schedules a paint of the page for the given region. The coordinates are
  // relative to the top-left of the plugin. This does nothing if the plugin
  // has not yet been positioned. You can supply an empty gfx::Rect() to
  // invalidate the entire plugin.
  void InvalidateRect(const gfx::Rect& rect);

  // PPB_Instance implementation.
  PP_Var GetWindowObject();
  PP_Var GetOwnerElementObject();
  bool BindGraphicsDeviceContext(PP_Resource device_id);

  // PPP_Instance pass-through.
  void Delete();
  bool Initialize(WebKit::WebPluginContainer* container,
                  const std::vector<std::string>& arg_names,
                  const std::vector<std::string>& arg_values);
  bool HandleInputEvent(const WebKit::WebInputEvent& event,
                        WebKit::WebCursorInfo* cursor_info);
  PP_Var GetInstanceObject();
  void ViewChanged(const gfx::Rect& position, const gfx::Rect& clip);

  // Notifications that the view has rendered the page and that it has been
  // flushed to the screen. These messages are used to send Flush callbacks to
  // the plugin for DeviceContext2D.
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

 private:
  PluginDelegate* delegate_;
  scoped_refptr<PluginModule> module_;
  const PPP_Instance* instance_interface_;

  // NULL until we have been initialized.
  WebKit::WebPluginContainer* container_;

  // Position in the viewport (which moves as the page is scrolled) of this
  // plugin. This will be a 0-sized rectangle if the plugin has not yet been
  // laid out.
  gfx::Rect position_;

  // Current clip rect. This will be empty if the plugin is not currently
  // visible. This is in the plugin's coordinate system, so fully visible will
  // be (0, 0, w, h) regardless of scroll position.
  gfx::Rect clip_;

  // The current device context for painting in 2D.
  scoped_refptr<DeviceContext2D> device_context_2d_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_INSTANCE_H_
