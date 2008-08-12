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

// DOMUIHost is a special type of TabContents that can display Chrome-specific
// content using HTML.
// It provides methods to send data to and from the page Javascript.

// TODO(evanm): Add more limitations on what the contents of this view
//              can do (especially regarding network requests).

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_HOST_H__
#define CHROME_BROWSER_DOM_UI_DOM_UI_HOST_H__

#include "chrome/browser/web_contents.h"
#include "webkit/glue/webpreferences.h"

class DOMMessageDispatcher;
class RenderProcessHost;
class RenderViewHost;

// Messages sent from the DOM are forwarded via the DOMUIHost to handler
// classes.  These objects are owned by DOMUIHost and destroyed when the
// host is destroyed.
class DOMMessageHandler {
 public:
  virtual ~DOMMessageHandler() {}
};

// See the comments at the top of this file.
class DOMUIHost : public WebContents {
 public:
  DOMUIHost(Profile* profile,
            SiteInstance* instance,
            RenderViewHostFactory* render_view_factory);

  // Initializes the given renderer, after enabling DOM UI bindings on it.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);

  // Add type-specific javascript message handlers.
  // TODO(timsteele): Any implementation of this method should really be done
  // upon construction, but that won't work until the TabContents::controller()
  // API is fixed to never return NULL, and likewise for TabContents::profile().
  // Only then could any Handlers we attach here access the profile upon
  // construction, which is the most common case; currently they'll blow up.
  virtual void AttachMessageHandlers() = 0;

  // Add |handler| to the list of handlers owned by this object.
  // They will be destroyed when this page is hidden.
  void AddMessageHandler(DOMMessageHandler* handler);

  // Register a callback for a specific message.
  typedef Callback1<const Value*>::Type MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               MessageCallback* callback);


  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // There are two function variants for one-arg and two-arg calls.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg1,
                              const Value& arg2);


  // Overrides from WebContents.
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content);
  virtual DOMUIHost* AsDOMUIHost() { return this; }

  // Override this method so we can ensure that javascript and image loading
  // are always on even for DOMUIHost tabs.
  virtual WebPreferences GetWebkitPrefs();

  // We override updating history with a no-op so these pages
  // are not saved to history.
  virtual void UpdateHistoryForNavigation(const GURL& url,
      const ViewHostMsg_FrameNavigate_Params& params) { }

 protected:
  // Should delete via CloseContents.
  virtual ~DOMUIHost();

 private:
  // Execute a string of raw Javascript on the page.
  void ExecuteJavascript(const std::wstring& javascript);

  // The DOMMessageHandlers we own.
  std::vector<DOMMessageHandler*> handlers_;

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback*> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  DISALLOW_EVIL_CONSTRUCTORS(DOMUIHost);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_HOST_H__
