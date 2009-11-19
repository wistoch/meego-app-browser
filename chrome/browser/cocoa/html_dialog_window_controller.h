// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_

#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "googleurl/src/gurl.h"

class Browser;

// Thin bridge that routes notifications to
// HtmlDialogWindowController's member variables.
//
// TODO(akalin): This doesn't need to be in the .h file; move it to the
// .mm file.
class HtmlDialogWindowDelegateBridge : public HtmlDialogUIDelegate,
                                       public TabContentsDelegate {
 public:
  // All parameters must be non-NULL/non-nil.
  HtmlDialogWindowDelegateBridge(NSWindowController* controller,
                                 HtmlDialogUIDelegate* delegate,
                                 Browser* browser);

  virtual ~HtmlDialogWindowDelegateBridge();

  // Called when the window is directly closed, e.g. from the close
  // button or from an accelerator.
  void WindowControllerClosed();

  // HtmlDialogUIDelegate declarations.
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

  // TabContentsDelegate declarations.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

 private:
  NSWindowController* controller_;  // weak
  HtmlDialogUIDelegate* delegate_;  // weak, owned by controller_
  Browser* browser_;  // weak, owned by controller_

  // Calls delegate_'s OnDialogClosed() exactly once, nulling it out
  // afterwards so that no other HtmlDialogUIDelegate calls are sent
  // to it.  Returns whether or not the OnDialogClosed() was actually
  // called on the delegate.
  bool DelegateOnDialogClosed(const std::string& json_retval);

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogWindowDelegateBridge);
};

// This controller manages a dialog box with properties and HTML content taken
// from a HTMLDialogUIDelegate object.
@interface HtmlDialogWindowController : NSWindowController {
 @private
  // An HTML dialog can exist separately from a window in OS X, so this
  // controller needs its own browser.
  scoped_ptr<Browser> browser_;
  // Order here is important, as tab_contents_ may send messages to
  // delegate_ when it gets destroyed.
  scoped_ptr<HtmlDialogWindowDelegateBridge> delegate_;
  scoped_ptr<TabContents> tabContents_;
}

// Creates and shows an HtmlDialogWindowController with the given
// delegate, parent window, and profile, none of which may be NULL.
// The window is automatically destroyed when it is closed.
//
// TODO(akalin): Handle a NULL parentWindow as HTML dialogs may be launched
// without any browser windows being present (on OS X).
+ (void)showHtmlDialog:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile
          parentWindow:(gfx::NativeWindow)parent_window;

@end

@interface HtmlDialogWindowController (TestingAPI)

// This is the designated initializer.  However, this is exposed only
// for testing; use showHtmlDialog instead.
- (id)initWithDelegate:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile
          parentWindow:(gfx::NativeWindow)parentWindow;

// Loads the HTML content from the delegate; this is not a lightweight
// process which is why it is not part of the constructor.  Must be
// called before showWindow.
- (void)loadDialogContents;

@end

#endif  // CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_

