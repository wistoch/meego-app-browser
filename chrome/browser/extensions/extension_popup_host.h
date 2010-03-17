// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_HOST_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "build/build_config.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/browser_bubble.h"
#endif
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/native_widget_types.h"

#if defined(TOOLKIT_VIEWS)
class ExtensionPopup;
#endif

class Browser;
class Profile;
class RenderViewHost;

// ExtensionPopupHost objects implement the environment necessary to host
// ExtensionPopup views.  This class manages the creation and life-time
// of extension pop-up views.
class ExtensionPopupHost :  // NOLINT
#if defined(TOOLKIT_VIEWS)
                           public BrowserBubble::Delegate,
#endif
                           public NotificationObserver {
 public:
  // Classes wishing to host pop-ups should inherit from this class, and
  // implement the virtual methods below.  This class manages the lifetime
  // of an ExtensionPopupHost instance.
  class PopupDelegate {
   public:
    PopupDelegate() {}
    virtual ~PopupDelegate();
    virtual Browser* GetBrowser() const = 0;
    virtual RenderViewHost* GetRenderViewHost() = 0;
    virtual gfx::NativeView GetNativeViewOfHost() = 0;
    virtual Profile* GetProfile();

    // Constructs, or returns the existing ExtensionPopupHost instance.
    ExtensionPopupHost* popup_host();

   private:
    scoped_ptr<ExtensionPopupHost> popup_host_;

    DISALLOW_COPY_AND_ASSIGN(PopupDelegate);
  };

  explicit ExtensionPopupHost(PopupDelegate* delegate);
  virtual ~ExtensionPopupHost();

  PopupDelegate* delegate() { return delegate_; }
  void RevokeDelegate() { delegate_ = NULL; }

  // Dismiss the hosted pop-up, if one is present.
  void DismissPopup();

#if defined(TOOLKIT_VIEWS)
  ExtensionPopup* child_popup() const { return child_popup_; }
  void set_child_popup(ExtensionPopup* popup);

  // BrowserBubble::Delegate implementation.
  // Called when the Browser Window that this bubble is attached to moves.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);

  // Called with the Browser Window that this bubble is attached to is
  // about to close.
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);
#endif  // defined(TOOLKIT_VIEWS)

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Posts a task to the current thread's message-loop that will dismiss the
  // popup.
  void DismissPopupAsync();

#if defined(TOOLKIT_VIEWS)
  // A native-view focus listener that monitors when the pop-up should be
  // dismissed due to focus change events.
  class PopupFocusListener;
  scoped_ptr<PopupFocusListener> listener_;

  // A popup view that is anchored to and owned by this ExtensionHost.  However,
  // the popup contains its own separate ExtensionHost
  ExtensionPopup* child_popup_;
#endif

  NotificationRegistrar registrar_;

  // Non-owning pointer to the delegate for this host.
  PopupDelegate* delegate_;

  // Boolean value used to ensure that the host only registers for event
  // notifications once.
  bool listeners_registered_;

  ScopedRunnableMethodFactory<ExtensionPopupHost> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_HOST_H_
