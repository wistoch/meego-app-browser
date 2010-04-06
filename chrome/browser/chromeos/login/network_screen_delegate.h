// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_

#include "app/combobox_model.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox.h"

namespace chromeos {

class LanguageSwitchModel;

// Interface that NetworkScreen exposes to the NetworkSelectionView.
class NetworkScreenDelegate : public ComboboxModel,
                              public views::Combobox::Listener,
                              public views::ButtonListener,
                              public NetworkLibrary::Observer {
 public:
  virtual LanguageSwitchModel* language_switch_model() = 0;

 protected:
  virtual ~NetworkScreenDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SCREEN_DELEGATE_H_
