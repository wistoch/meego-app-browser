// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__

#include <set>
#include <string>

#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugin_delegate.h"

class GURL;
struct PluginHostMsg_RouteToFrame_Params;
class RenderView;
class SkBitmap;

// An implementation of WebPluginDelegate that proxies all calls to
// the plugin process.
class WebPluginDelegateProxy : public WebPluginDelegate,
                               public IPC::Channel::Listener,
                               public IPC::Message::Sender  {
 public:
  static WebPluginDelegateProxy* Create(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& clsid,
                                        RenderView* render_view);

  // Called to drop our back-pointer to the containing RenderView.
  void DropRenderView() { render_view_ = NULL; }

  // Called to drop our pointer to the window script object.
  void DropWindowScriptObject() { window_script_object_ = NULL; }

  // Called to flush any deferred geometry changes to the plugin process.
  virtual void FlushGeometryUpdates();

  // WebPluginDelegate implementation:
  virtual void PluginDestroyed();
  virtual bool Initialize(const GURL& url, char** argn, char** argv, int argc,
                          WebPlugin* plugin, bool load_manually);
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect, bool visible);
  virtual void Paint(HDC hdc, const gfx::Rect& rect);
  virtual void Print(HDC hdc);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(NPReason reason);
  virtual void SetFocus();
  virtual bool HandleEvent(NPEvent* event, WebCursor* cursor);
  virtual int GetProcessId();
  virtual HWND GetWindowHandle();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result,
                                    bool success, bool notify_needed,
                                    int notify_data);

  virtual void DidReceiveManualResponse(const std::string& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual std::wstring GetPluginPath();
  virtual void InstallMissingPlugin();
  virtual WebPluginResourceClient* CreateResourceClient(int resource_id,
                                                        const std::string &url,
                                                        bool notify_needed,
                                                        void *notify_data);

  // Notifies the delegate about a Get/Post URL request getting routed
  virtual void URLRequestRouted(const std::string&url, bool notify_needed,
                                void* notify_data);

 private:
  WebPluginDelegateProxy(const std::string& mime_type,
                         const std::string& clsid,
                         RenderView* render_view);
  ~WebPluginDelegateProxy();

  // Message handlers for messages that proxy WebPlugin methods, which
  // we translate into calls to the real WebPlugin.
  void OnSetWindow(HWND window, HANDLE modal_loop_pump_messages_event);
  void OnCompleteURL(const std::string& url_in, std::string* url_out,
                     bool* result);
  void OnHandleURLRequest(const PluginHostMsg_URLRequest_Params& params);
  void OnCancelResource(int id);
  void OnInvalidate();
  void OnInvalidateRect(const gfx::Rect& rect);
  void OnGetWindowScriptNPObject(int route_id, bool* success, void** npobject_ptr);
  void OnGetPluginElement(int route_id, bool* success, void** npobject_ptr);
  void OnSetCookie(const GURL& url,
                   const GURL& policy_url,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& policy_url,
                    std::string* cookies);
  void OnShowModalHTMLDialog(const GURL& url, int width, int height,
                             const std::string& json_arguments,
                             std::string* json_retval);
  void OnMissingPluginStatus(int status);
  void OnGetCPBrowsingContext(uint32* context);

  // Draw a graphic indicating a crashed plugin.
  void PaintSadPlugin(HDC hdc, const gfx::Rect& rect);

  RenderView* render_view_;
  WebPlugin* plugin_;
  bool windowless_;
  bool first_paint_;
  scoped_refptr<PluginChannelHost> channel_host_;
  std::string mime_type_;
  std::string clsid_;
  int instance_id_;
  std::wstring plugin_path_;

  gfx::Rect plugin_rect_;
  gfx::Rect deferred_clip_rect_;
  bool send_deferred_update_geometry_;
  bool visible_;

  NPObject* npobject_;
  NPObjectStub* window_script_object_;

  // Event passed in by the plugin process and is used to decide if
  // messages need to be pumped in the NPP_HandleEvent sync call.
  HANDLE modal_loop_pump_messages_event_;

  // Bitmap for crashed plugin
  SkBitmap* sad_plugin_;

  DISALLOW_EVIL_CONSTRUCTORS(WebPluginDelegateProxy);
};

#endif  // #ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__
