// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_COCOA_BROWSER_WINDOW_COCOA_H_

#include "chrome/browser/browser_window.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

class Browser;
@class BrowserWindowController;
@class FindBarCocoaController;
@class NSString;
@class NSWindow;
@class NSMenu;

// An implementation of BrowserWindow for Cocoa. Bridges between C++ and
// the Cocoa NSWindow. Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowCocoa : public BrowserWindow,
                           public NotificationObserver {
 public:
  BrowserWindowCocoa(Browser* browser,
                     BrowserWindowController* controller,
                     NSWindow* window);
  virtual ~BrowserWindowCocoa();

  // Overridden from BrowserWindow
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetNormalBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ConfirmBrowserCloseWithPendingDownloads();
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              void* parent_window);
  virtual void UserChangedTheme();

  // Overridden from NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Adds the given FindBar cocoa controller to this browser window.
  void AddFindBar(FindBarCocoaController* find_bar_cocoa_controller);

 protected:
  virtual void DestroyBrowser();

 private:
  void SetMinimizedWindowTitle(NSWindow* window, NSString* title);

  NSWindow* window_;  // weak, owned by controller
  Browser* browser_;  // weak, owned by controller
  BrowserWindowController* controller_;  // weak, owns us
};

#endif  // CHROME_BROWSER_COCOA_BROWSER_WINDOW_COCOA_H_
