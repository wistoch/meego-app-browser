// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_ACCESSIBILITY_EVENT_ROUTER_GTK_H_
#define CHROME_BROWSER_GTK_ACCESSIBILITY_EVENT_ROUTER_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/singleton.h"
#include "base/task.h"
#include "chrome/browser/accessibility_events.h"

class Profile;

// Allows us to use (GtkWidget*) in a hash_map with gcc.
namespace __gnu_cxx {
template<>
struct hash<GtkWidget*> {
  size_t operator()(GtkWidget* widget) const {
    return reinterpret_cast<size_t>(widget);
  }
};
}  // namespace __gnu_cxx

// Struct to keep track of event listener hook ids to remove them later.
struct InstalledHook {
  InstalledHook(guint _signal_id, gulong _hook_id)
      : signal_id(_signal_id), hook_id(_hook_id) { }
  guint signal_id;
  gulong hook_id;
};

// Singleton class that adds a signal emission hook to many gtk events, and
// then sends an accessibility notification whenever a relevant event is
// sent to an accessible control.
//
// Gtk widgets are not accessible by default. When you register a root widget,
// that widget and all of its descendants will start sending accessibility
// event notifications. You can then override the default behavior for
// specific descendants using other methods.
//
// You can use Profile::PauseAccessibilityEvents to prevent a flurry
// of accessibility events when a window is being created or initialized.
class AccessibilityEventRouter {
 public:
  // Internal information about a particular widget to override the
  // information we get directly from gtk.
  struct WidgetInfo {
    WidgetInfo() : ignore(false) { }

    // If nonempty, will use this name instead of the widget's label.
    std::string name;

    // If true, will ignore this widget and not send accessibility events.
    bool ignore;
  };

  // Get the single instance of this class.
  static AccessibilityEventRouter* GetInstance();

  // Start sending accessibility events for this widget and all of its
  // descendants.  Notifications will go to the specified profile.
  void AddRootWidget(GtkWidget* root_widget, Profile* profile);

  // Stop sending accessibility events for this widget and all of its
  // descendants.
  void RemoveRootWidget(GtkWidget* root_widget);

  // Don't send any events for this widget.
  void IgnoreWidget(GtkWidget* widget);

  // Use the following string as the name of this widget, instead of the
  // gtk label associated with the widget.
  void SetWidgetName(GtkWidget* widget, std::string name);

  // Forget all information about this widget.
  void RemoveWidget(GtkWidget* widget);

  //
  // The following methods are only for use by gtk signal handlers.
  //

  // Returns true if this widget is a descendant of one of our registered
  // root widgets and not in the set of ignored widgets.  If |profile| is
  // not null, return the profile where notifications associated with this
  // widget should be sent.
  bool IsWidgetAccessible(GtkWidget* widget, Profile** profile);

  // Return the name of a widget.
  std::string GetWidgetName(GtkWidget* widget);

  // Called by the signal handler.  Checks the type of the widget and
  // calls one of the more specific Send*Notification methods, below.
  void DispatchAccessibilityNotification(
      GtkWidget* widget, NotificationType type);

  // Post a task to call DispatchAccessibilityNotification the next time
  // through the event loop.
  void PostDispatchAccessibilityNotification(
      GtkWidget* widget, NotificationType type);

  // Each of these methods constructs an AccessibilityControlInfo object
  // and sends a notification of a specific accessibility event.
  void SendButtonNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendCheckboxNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendComboBoxNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendListBoxNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendRadioButtonNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendTabNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);
  void SendTextBoxNotification(
      GtkWidget* widget, NotificationType type, Profile* profile);

  void InstallEventListeners();
  void RemoveEventListeners();

  // Start and stop listening to signals.
  void StartListening();
  void StopListening();

 private:
  AccessibilityEventRouter();
  virtual ~AccessibilityEventRouter();

  // Add a signal emission hook for one particular signal name and
  // widget type, and save the hook_id in installed_hooks so we can
  // remove it later.
  void InstallEventListener(
      const char *signal_name,
      GType widget_type,
      GSignalEmissionHook hook_func);

  friend struct DefaultSingletonTraits<AccessibilityEventRouter>;

  // The set of all root widgets; only descendants of these will generate
  // accessibility notifications.
  base::hash_map<GtkWidget*, Profile*> root_widget_profile_map_;

  // Extra information about specific widgets.
  base::hash_map<GtkWidget*, WidgetInfo> widget_info_map_;

  // Installed event listener hook ids so we can remove them later.
  std::vector<InstalledHook> installed_hooks_;

  // True if we are currently listening to signals.
  bool listening_;

  // Used to schedule invocations of StartListening().
  ScopedRunnableMethodFactory<AccessibilityEventRouter> method_factory_;
};

#endif  // CHROME_BROWSER_GTK_ACCESSIBILITY_EVENT_ROUTER_GTK_H_
