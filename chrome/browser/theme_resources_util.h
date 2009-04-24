// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEME_RESOURCES_UTIL_H_
#define CHROME_BROWSER_THEME_RESOURCES_UTIL_H_

#include <string>

#include "base/basictypes.h"

class ThemeResourcesUtil {
 public:
  // Returns the theme resource id or -1 if no resource with the name exists.
  static int GetId(const std::string& resource_name);

 private:
  ThemeResourcesUtil() {}
  DISALLOW_COPY_AND_ASSIGN(ThemeResourcesUtil);
};

#endif  // CHROME_BROWSER_THEME_RESOURCES_UTIL_H_
