// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/core_options_handler.h"

namespace chromeos {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public ::CoreOptionsHandler {
 public:

 protected:
  // ::CoreOptionsHandler overrides
  virtual Value* FetchPref(const std::string& pref_name);
  virtual void ObservePref(const std::string& pref_name);
  virtual void SetPref(const std::string& pref_name,
                       Value::ValueType pref_type,
                       const std::string& value_string);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string* setting_name);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_
