// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APP_MODE_COMMON_MAC_H_
#define CHROME_COMMON_APP_MODE_COMMON_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

// This file contains constants, interfaces, etc. which are common to the
// browser application and the app mode loader (a.k.a. shim).

namespace app_mode {

// The ID under which app mode preferences will be recorded
// ("org.chromium.Chromium" or "com.google.Chrome").
extern const CFStringRef kAppPrefsID;

// The key under which to record the path to the (user-visible) application
// bundle; this key is recorded under the ID given by |kAppPrefsID|.
extern const CFStringRef kLastRunAppBundlePathPrefsKey;

// Current major/minor version numbers of |ChromeAppModeInfo| (defined below).
const unsigned kCurrentChromeAppModeInfoMajorVersion = 0;
const unsigned kCurrentChromeAppModeInfoMinorVersion = 1;

// The structure used to pass information from the app mode loader to the
// (browser) framework. This is versioned using major and minor version numbers,
// written below as v<major>.<minor>. Version-number checking is done by the
// framework, and the framework must accept all structures with the same major
// version number. It may refuse to load if the major version of the structure
// is different from the one it accepts.
struct ChromeAppModeInfo {
  // Major and minor version number of this structure.
  unsigned major_version;  // Required: all versions
  unsigned minor_version;  // Required: all versions

  // Original |argc| and |argv|.
  int argc;  // Required: v0.1
  char** argv;  // Required: v0.1

  // Versioned path to the browser which is being loaded.
  char* chrome_versioned_path;  // Required: v0.1

  // Information about the App Mode shortcut:

  // Path to the App Mode Loader application bundle originally run.
  char* app_mode_bundle_path;  // Optional: v0.1

  // Short ID string, preferably derived from |app_mode_short_name|. Should be
  // safe for the file system.
  char* app_mode_id;  // Required: v0.1

  // Short (e.g., one-word) UTF8-encoded name for the shortcut.
  char* app_mode_short_name;  // Optional: v0.1

  // Unrestricted (e.g., several-word) UTF8-encoded name for the shortcut.
  char* app_mode_name;  // Optional: v0.1

  // URL for the shortcut. Must be a valid URL.
  char* app_mode_url;  // Required: v0.1
};

}  // namespace app_mode

#endif  // CHROME_COMMON_APP_MODE_COMMON_MAC_H_
