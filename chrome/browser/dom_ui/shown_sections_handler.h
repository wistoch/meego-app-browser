// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class DOMUI;
class Value;
class PrefService;

// Use for the shown sections bitmask.
enum Section {
  THUMB = 1,
  LIST = 2,
  RECENT = 4,
  RECOMMENDATIONS = 8
};

class ShownSectionsHandler : public DOMMessageHandler {
 public:
  explicit ShownSectionsHandler(DOMUI* dom_ui);

  // Callback for "getShownSections" message.
  void HandleGetShownSections(const Value* value);

  // Callback for "setShownSections" message.
  void HandleSetShownSections(const Value* value);

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DOMUI* dom_ui_;

  DISALLOW_COPY_AND_ASSIGN(ShownSectionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_
