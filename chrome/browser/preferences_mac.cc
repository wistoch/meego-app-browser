// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preferences_mac.h"

CFPropertyListRef MacPreferences::CopyAppValue(CFStringRef key,
                                               CFStringRef applicationID) {
  return CFPreferencesCopyAppValue(key, applicationID);
}

Boolean MacPreferences::AppValueIsForced(CFStringRef key,
                                         CFStringRef applicationID) {
  return CFPreferencesAppValueIsForced(key, applicationID);
}
