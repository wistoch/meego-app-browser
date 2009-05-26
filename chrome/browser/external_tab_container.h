// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_
#define CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/focus/focus_manager.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

class AutomationProvider;
class TabContents;
class Profile;
class TabContentsContainer;
// This class serves as the container window for an external tab.
// An external tab is a Chrome tab that is meant to displayed in an
// external process. This class provides the FocusManger needed by the
// TabContents as well as an implementation of TabContentsDelagate.
// TODO(beng): Should override WidgetWin instead of Widget.
class ExternalTabContainer : public TabContentsDelegate,
                             public NotificationObserver,
                             public views::Widget,
                             public views::KeystrokeListener,
                             public CWindowImpl<ExternalTabContainer,
                                                CWindow,
                                                CWinTraits<WS_POPUP |
                                                    WS_CLIPCHILDREN>> {
 public:
  BEGIN_MSG_MAP(ExternalTabContainer)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

  DECLARE_WND_CLASS(chrome::kExternalTabWindowClass)

  ExternalTabContainer(AutomationProvider* automation);
  ~ExternalTabContainer();

  TabContents* tab_contents() const {
    return tab_contents_;
  }

  // Temporary hack so we can send notifications back
  void set_tab_handle(int handle) {
    tab_handle_ = handle;
  }

  bool Init(Profile* profile, HWND parent, const gfx::Rect& dimensions,
            unsigned int style);

  // This function gets called from two places, which is fine.
  // 1. OnFinalMessage
  // 2. In the destructor.
  bool Uninitialize(HWND window);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
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
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual void ContentsZoomChange(bool zoom_in);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void ForwardMessageToExternalHost(const std::string& message,
                                            const std::string& origin,
                                            const std::string& target);
  virtual bool IsExternalTabContainer() const {
    return true;
  };

  // Creates an ExtensionFunctionDispatcher that has no browser
  virtual ExtensionFunctionDispatcher *CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host,
      const std::string& extension_id);

  virtual bool TakeFocus(bool reverse);

  // Notification service callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  /////////////////////////////////////////////////////////////////////////////
  // views::Widget
  /////////////////////////////////////////////////////////////////////////////
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window);
  virtual void Close();
  virtual void CloseNow();
  virtual void Show();
  virtual void Hide();
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual void SetOpacity(unsigned char opacity);
  virtual views::RootView* GetRootView();
  virtual Widget* GetRootWidget() const { return NULL; }
  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual bool GetAccelerator(int cmd_id,
                              views::Accelerator* accelerator) {
    return false;
  }

  // views::KeystrokeListener implementation
  // This method checks whether this key[down|up] message is needed by the
  // external host. If so, it sends it over to the external host
  virtual bool ProcessKeyStroke(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam);

  // Sets the keyboard accelerators needed by the external host
  void SetAccelerators(HACCEL accel_table, int accel_table_entry_count);

  // This is invoked when the external host reflects back to us a keyboard
  // message it did not process
  void ProcessUnhandledAccelerator(const MSG& msg);

  // See TabContents::SetInitialFocus
  void SetInitialFocus(bool reverse);

  // A helper method that tests whether the given window is an
  // ExternalTabContainer window
  static bool IsExternalTabContainer(HWND window);

  // A helper method that retrieves the ExternalTabContainer object that
  // hosts the given tab window.
  static ExternalTabContainer* GetContainerForTab(HWND tab_window);

 protected:
  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL& handled);
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled);
  void OnFinalMessage(HWND window);

 protected:
  TabContents* tab_contents_;
  scoped_refptr<AutomationProvider> automation_;

  NotificationRegistrar registrar_;

  // Root view
  views::RootView root_view_;
  // The accelerator table of the external host.
  HACCEL external_accel_table_;
  unsigned int external_accel_entry_count_;
  // A view to handle focus cycling
  TabContentsContainer* tab_contents_container_;

 private:
  int tab_handle_;
  // A failed navigation like a 404 is followed in chrome with a success
  // navigation for the 404 page. We need to ignore the next navigation
  // to avoid confusing the clients of the external tab. This member variable
  // is set when we need to ignore the next load notification.
  bool ignore_next_load_notification_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTabContainer);
};

#endif  // CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_
