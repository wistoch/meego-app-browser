// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
#define WEBKIT_GLUE_WEBPLUGIN_IMPL_H_

#include <string>
#include <map>
#include <vector>

#include "Widget.h"

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/linked_ptr.h"
#include "webkit/api/public/WebPlugin.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webplugin.h"


class WebFrameImpl;
class WebPluginDelegate;

namespace WebCore {
class Event;
class Frame;
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class KURL;
class MouseEvent;
class ResourceError;
class ResourceResponse;
class ScrollView;
class String;
class Widget;
}

namespace WebKit {
class WebPluginContainer;
class WebURLResponse;
class WebURLLoader;
}

namespace webkit_glue {
class MultipartResponseDelegate;
}

// This is the WebKit side of the plugin implementation that forwards calls,
// after changing out of WebCore types, to a delegate.  The delegate may
// be in a different process.
class WebPluginImpl : public WebPlugin,
                      public WebKit::WebPlugin,
                      public WebKit::WebURLLoaderClient {
 public:
  // Creates a WebPlugin instance, as long as the delegate's initialization
  // succeeds.  If it fails, the delegate is deleted and NULL is returned.
  // Note that argn and argv are UTF8.
  static PassRefPtr<WebCore::Widget> Create(const GURL& url,
                                 char** argn,
                                 char** argv,
                                 int argc,
                                 WebCore::HTMLPlugInElement* element,
                                 WebFrameImpl* frame,
                                 WebPluginDelegate* delegate,
                                 bool load_manually,
                                 const std::string& mime_type);
  virtual ~WebPluginImpl();

  // Helper function for sorting post data.
  static bool SetPostData(WebKit::WebURLRequest* request,
                          const char* buf,
                          uint32 length);

 private:
  WebPluginImpl(
      WebFrameImpl* frame, WebPluginDelegate* delegate, const GURL& plugin_url,
      bool load_manually, const std::string& mime_type, int arg_count,
      char** arg_names, char** arg_values);

  // WebKit::WebPlugin methods:
  virtual void destroy();
  virtual NPObject* scriptableObject();
  virtual void paint(
      WebKit::WebCanvas* canvas, const WebKit::WebRect& paint_rect);
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect, const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs, bool is_visible);
  virtual void updateFocus(bool focused);
  virtual void updateVisibility(bool visible);
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(
      const WebKit::WebInputEvent& event, WebKit::WebCursorInfo& cursor_info);
  virtual void didReceiveResponse(const WebKit::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const WebKit::WebURLError& error);

  // WebPlugin implementation:
  void SetWindow(gfx::PluginWindowHandle window);
  void WillDestroyWindow(gfx::PluginWindowHandle window);
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event) { }
#endif

  // Given a (maybe partial) url, completes using the base url.
  GURL CompleteURL(const char* url);

  // Executes the script passed in. The notify_needed and notify_data arguments
  // are passed in by the plugin process. These indicate whether the plugin
  // expects a notification on script execution. We pass them back to the
  // plugin as is. This avoids having to track the notification arguments
  // in the plugin process.
  bool ExecuteScript(const std::string& url, const std::wstring& script,
                     bool notify_needed, intptr_t notify_data, bool popups_allowed);

  // Given a download request, check if we need to route the output
  // to a frame.  Returns ROUTED if the load is done and routed to
  // a frame, NOT_ROUTED or corresponding error codes otherwise.
  RoutingStatus RouteToFrame(const char *method, bool is_javascript_url,
                             const char* target, unsigned int len,
                             const char* buf, bool is_file_data, bool notify,
                             const char* url, GURL* completeURL);

  // Cancels a pending request.
  void CancelResource(int id);

  // Returns the next avaiable resource id.
  int GetNextResourceId();

  // Initiates HTTP GET/POST requests.
  // Returns true on success.
  bool InitiateHTTPRequest(int resource_id, WebPluginResourceClient* client,
                           const char* method, const char* buf, int buf_len,
                           const GURL& url, const char* range_info,
                           bool use_plugin_src_as_referer);

  gfx::Rect GetWindowClipRect(const gfx::Rect& rect);

  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();

  void SetCookie(const GURL& url,
                 const GURL& policy_url,
                 const std::string& cookie);
  std::string GetCookies(const GURL& url,
                         const GURL& policy_url);

  void ShowModalHTMLDialog(const GURL& url, int width, int height,
                           const std::string& json_arguments,
                           std::string* json_retval);
  void OnMissingPluginStatus(int status);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);

  // Sets the actual Widget for the plugin.
  void SetContainer(WebKit::WebPluginContainer* container);

  // Destroys the plugin instance.
  // The response_handle_to_ignore parameter if not NULL indicates the
  // resource handle to be left valid during plugin shutdown.
  void TearDownPluginInstance(WebKit::WebURLLoader* loader_to_ignore);

  // WebURLLoaderClient implementation.  We implement this interface in the
  // renderer process, and then use the simple WebPluginResourceClient interface
  // to relay the callbacks to the plugin.
  virtual void willSendRequest(WebKit::WebURLLoader* loader,
                               WebKit::WebURLRequest& request,
                               const WebKit::WebURLResponse&);
  virtual void didSendData(WebKit::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(WebKit::WebURLLoader* loader,
                                  const WebKit::WebURLResponse& response);
  virtual void didReceiveData(WebKit::WebURLLoader* loader, const char *buffer,
                              int length, long long total_length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader);
  virtual void didFail(WebKit::WebURLLoader* loader, const WebKit::WebURLError&);

  // Helper function to remove the stored information about a resource
  // request given its index in m_clients.
  void RemoveClient(size_t i);

  // Helper function to remove the stored information about a resource
  // request given a handle.
  void RemoveClient(WebKit::WebURLLoader* loader);

  WebCore::Frame* frame() { return webframe_ ? webframe_->frame() : NULL; }

  void HandleURLRequest(const char *method,
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        intptr_t notify_data, bool popups_allowed);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url, const char* range_info,
                                intptr_t existing_stream, bool notify_needed,
                                intptr_t notify_data);

  void SetDeferResourceLoading(int resource_id, bool defer);

  // Ignore in-process plugins mode for this flag.
  bool IsOffTheRecord() { return false; }

  // Handles HTTP multipart responses, i.e. responses received with a HTTP
  // status code of 206.
  void HandleHttpMultipartResponse(const WebKit::WebURLResponse& response,
                                   WebPluginResourceClient* client);

  void HandleURLRequestInternal(const char *method, bool is_javascript_url,
                                const char* target, unsigned int len,
                                const char* buf, bool is_file_data,
                                bool notify, const char* url,
                                intptr_t notify_data, bool popups_allowed,
                                bool use_plugin_src_as_referrer);

  // Tears down the existing plugin instance and creates a new plugin instance
  // to handle the response identified by the loader parameter.
  bool ReinitializePluginForResponse(WebKit::WebURLLoader* loader);

  // Helper functions to convert an array of names/values to a vector.
  static void ArrayToVector(int total_values, char** values,
                            std::vector<std::string>* value_vector);

  // Delayed task for downloading the plugin source URL.
  void OnDownloadPluginSrcUrl();

  // Returns the WebViewDelegate associated with webframe_;
  WebViewDelegate* GetWebViewDelegate();

  struct ClientInfo {
    int id;
    WebPluginResourceClient* client;
    WebKit::WebURLRequest request;
    linked_ptr<WebKit::WebURLLoader> loader;
  };

  // Helper functions
  WebPluginResourceClient* GetClientFromLoader(WebKit::WebURLLoader* loader);
  ClientInfo* GetClientInfoFromLoader(WebKit::WebURLLoader* loader);

  std::vector<ClientInfo> clients_;

  bool windowless_;
  gfx::PluginWindowHandle window_;
  WebFrameImpl* webframe_;

  WebPluginDelegate* delegate_;

  // This is just a weak reference.
  WebKit::WebPluginContainer* container_;

  typedef std::map<WebPluginResourceClient*,
                   webkit_glue::MultipartResponseDelegate*>
      MultiPartResponseHandlerMap;
  // Tracks HTTP multipart response handlers instantiated for
  // a WebPluginResourceClient instance.
  MultiPartResponseHandlerMap multi_part_response_map_;

  // The plugin source URL.
  GURL plugin_url_;

  // Indicates if the download would be initiated by the plugin or us.
  bool load_manually_;

  // Indicates if this is the first geometry update received by the plugin.
  bool first_geometry_update_;

  // Set to true if the next response error should be ignored.
  bool ignore_response_error_;

  // The current plugin geometry and clip rectangle.
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;

  // The mime type of the plugin.
  std::string mime_type_;

  // Holds the list of argument names passed to the plugin.
  std::vector<std::string> arg_names_;

  // Holds the list of argument values passed to the plugin.
  std::vector<std::string> arg_values_;

  ScopedRunnableMethodFactory<WebPluginImpl> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_IMPL_H_
