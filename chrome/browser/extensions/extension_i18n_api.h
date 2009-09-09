// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_I18N_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_I18N_API_H__

#include "chrome/browser/extensions/extension_function.h"

namespace extension_i18n_api_functions {
  extern const char kGetAcceptLanguagesFunction[];
};  // namespace extension_i18n_api_functions

class GetAcceptLanguagesFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_I18N_API_H__
