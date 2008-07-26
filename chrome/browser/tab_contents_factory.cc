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

#include "chrome/browser/tab_contents.h"

#include "chrome/browser/about_internets_status_view.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/ipc_status_view.h"
#include "chrome/browser/native_ui_contents.h"
#include "chrome/browser/network_status_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/tab_contents_factory.h"
#include "chrome/browser/view_source_contents.h"
#include "chrome/browser/web_contents.h"
#include "base/string_util.h"
#include "net/base/net_util.h"

typedef std::map<TabContentsType, TabContentsFactory*> TabContentsFactoryMap;
static TabContentsFactoryMap* g_extra_types;  // Only allocated if needed.

/*static*/
TabContents* TabContents::CreateWithType(TabContentsType type,
                                         HWND parent,
                                         Profile* profile,
                                         SiteInstance* instance) {
  TabContents* contents;

  switch (type) {
    case TAB_CONTENTS_WEB:
      contents = new WebContents(profile, instance, NULL, MSG_ROUTING_NONE, NULL);
      break;
    case TAB_CONTENTS_NETWORK_STATUS_VIEW:
      contents = new NetworkStatusView();
      break;
#ifdef IPC_MESSAGE_LOG_ENABLED
    case TAB_CONTENTS_IPC_STATUS_VIEW:
      contents = new IPCStatusView();
      break;
#endif
    case TAB_CONTENTS_NEW_TAB_UI:
      contents = new NewTabUIContents(profile, instance, NULL);
      break;
    case TAB_CONTENTS_HTML_DIALOG:
      contents = new HtmlDialogContents(profile, instance, NULL);
      break;
    case TAB_CONTENTS_NATIVE_UI:
      contents = new NativeUIContents(profile);
      break;
    case TAB_CONTENTS_ABOUT_INTERNETS_STATUS_VIEW:
      contents = new AboutInternetsStatusView();
      break;
    case TAB_CONTENTS_VIEW_SOURCE:
      contents = new ViewSourceContents(profile, instance);
      break;
    case TAB_CONTENTS_ABOUT_UI:
      contents = new BrowserAboutHandler(profile, instance, NULL);
      break;
    default:
      if (g_extra_types) {
        TabContentsFactoryMap::const_iterator it = g_extra_types->find(type);
        if (it != g_extra_types->end()) {
          contents = it->second->CreateInstance();
          break;
        }
      }
      NOTREACHED() << "Don't know how to create tab contents of type " << type;
      contents = NULL;
  }

  if (contents)
    contents->CreateView(parent, gfx::Rect());

  return contents;
}

/*static*/
TabContentsType TabContents::TypeForURL(GURL* url) {
  DCHECK(url);
  if (g_extra_types) {
    TabContentsFactoryMap::const_iterator it = g_extra_types->begin();
    for (; it != g_extra_types->end(); ++it) {
      if (it->second->CanHandleURL(*url))
        return it->first;
    }
  }

  // Try to handle as a browser URL. If successful, |url| will end up
  // containing the real url being loaded (browser url's are just an alias).
  TabContentsType type(TAB_CONTENTS_UNKNOWN_TYPE);
  if (BrowserURLHandler::HandleBrowserURL(url, &type))
    return type;

  if (url->SchemeIs(NativeUIContents::GetScheme().c_str()))
    return TAB_CONTENTS_NATIVE_UI;

  if (HtmlDialogContents::IsHtmlDialogUrl(*url))
    return TAB_CONTENTS_HTML_DIALOG;

  if (url->SchemeIs("view-source")) {
    // Load the inner URL instead, but render it using a ViewSourceContents.
    *url = GURL(url->path());
    return TAB_CONTENTS_VIEW_SOURCE;
  }

  // NOTE: Even the empty string can be loaded by a WebContents.
  return TAB_CONTENTS_WEB;
}

/*static*/
TabContentsFactory* TabContents::RegisterFactory(TabContentsType type,
                                                 TabContentsFactory* factory) {
  if (!g_extra_types)
    g_extra_types = new TabContentsFactoryMap;

  TabContentsFactory* prev_factory = NULL;
  TabContentsFactoryMap::const_iterator prev = g_extra_types->find(type);
  if (prev != g_extra_types->end())
    prev_factory = prev->second;

  if (factory) {
    g_extra_types->insert(TabContentsFactoryMap::value_type(type, factory));
  } else {
    g_extra_types->erase(type);
    if (g_extra_types->empty()) {
      delete g_extra_types;
      g_extra_types = NULL;
    }
  }

  return prev_factory;
}
