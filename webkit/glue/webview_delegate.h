// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WebCore provides hooks for several kinds of functionality, allowing separate
// classes termed "delegates" to receive notifications (in the form of direct
// function calls) when certain events are about to occur or have just occurred.
// In some cases, the delegate implements the needed functionality; in others,
// the delegate has some control over the behavior but doesn't actually
// implement it.  For example, the UI delegate is responsible for showing a
// dialog box or otherwise handling a JavaScript window.alert() call, via the
// RunJavaScriptAlert() method. On the other hand, the editor delegate doesn't
// actually handle editing functionality, although it could (for example)
// override whether a content-editable node accepts editing focus by returning
// false from ShouldBeginEditing(). (It would also possible for a more
// special-purpose editing delegate to act on the edited node in some way, e.g.
// to highlight modified text in the DidChangeContents() method.)

// WebKit divides the delegated tasks into several different classes, but we
// combine them into a single WebViewDelegate. This single delegate encompasses
// the needed functionality of the WebKit UIDelegate, ContextMenuDelegate,
// PolicyDelegate, FrameLoadDelegate, and EditorDelegate; additional portions
// of ChromeClient and FrameLoaderClient not delegated in the WebKit
// implementation; and some WebView additions.

#ifndef WEBKIT_GLUE_WEBVIEW_DELEGATE_H_
#define WEBKIT_GLUE_WEBVIEW_DELEGATE_H_

#include <vector>

#include "webkit/api/public/WebDragOperation.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebTextDirection.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/glue/context_menu.h"

namespace WebCore {
class AccessibilityObject;
}

namespace WebKit {
class WebDragData;
class WebNotificationPresenter;
class WebWidget;
struct WebPopupMenuInfo;
struct WebPoint;
struct WebRect;
}

class FilePath;
class SkBitmap;
class WebDevToolsAgentDelegate;
class WebView;
struct ContextMenuMediaParams;

// TODO(darin): Eliminate WebViewDelegate in favor of WebViewClient.
class WebViewDelegate : public WebKit::WebViewClient {
 public:
  // WebView additions -------------------------------------------------------

  // Returns whether this WebView was opened by a user gesture.
  virtual bool WasOpenedByUserGesture() const {
    return true;
  }

  // Called by ChromeClientImpl::focus() if accessibility on the renderer side
  // is enabled, and a focus change has occurred. Will retrieve the id of the
  // input AccessibilityObject and send it through IPC for handling on the
  // browser side.
  virtual void FocusAccessibilityObject(WebCore::AccessibilityObject* acc_obj) {
  }

  // ChromeClient ------------------------------------------------------------

  // Queries the browser for suggestions to be shown for the form text field
  // named |field_name|.  |text| is the text entered by the user so far and
  // |node_id| is the id of the node of the input field.
  virtual void QueryFormFieldAutofill(const std::wstring& field_name,
                                      const std::wstring& text,
                                      int64 node_id) {
  }

  // Instructs the browser to remove the autofill entry specified from it DB.
  virtual void RemoveStoredAutofillEntry(const std::wstring& name,
                                         const std::wstring& value) {
  }

  // UIDelegate --------------------------------------------------------------

  // @abstract Shows a context menu with commands relevant to a specific
  //           element on the current page.
  // @param webview The WebView sending the delegate method.
  // @param node_type The type of the node(s) the context menu is being
  // invoked on
  // @param x The x position of the mouse pointer (relative to the webview)
  // @param y The y position of the mouse pointer (relative to the webview)
  // @param link_url The absolute URL of the link that contains the node the
  // mouse right clicked on
  // @param image_url The absolute URL of the image that the mouse right
  // clicked on
  // @param page_url The URL of the page the mouse right clicked on
  // @param frame_url The URL of the subframe the mouse right clicked on
  // @param media_params Extra attributed needed by the context menu for
  // media elements.
  // @param selection_text The raw text of the selection that the mouse right
  // clicked on
  // @param misspelled_word The editable (possibily) misspelled word
  // in the Editor on which dictionary lookup for suggestions will be done.
  // @param edit_flags which edit operations the renderer believes are available
  // @param security_info
  // @param frame_charset which indicates the character encoding of
  // the currently focused frame.
  virtual void ShowContextMenu(WebView* webview,
                               ContextNodeType node_type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const ContextMenuMediaParams& media_params,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info,
                               const std::string& frame_charset) {
  }


  // DevTools ----------------------------------------------------------------

  virtual WebDevToolsAgentDelegate* GetWebDevToolsAgentDelegate() {
    return NULL;
  }

 protected:
  ~WebViewDelegate() { }
};

#endif  // WEBKIT_GLUE_WEBVIEW_DELEGATE_H_
