// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/scoped_cftyperef.h"

namespace mac_util {

std::string PathFromFSRef(const FSRef& ref) {
  scoped_cftyperef<CFURLRef> url(
      CFURLCreateFromFSRef(kCFAllocatorDefault, &ref));
  NSString *path_string = [(NSURL *)url.get() path];
  return [path_string fileSystemRepresentation];
}

bool FSRefFromPath(const std::string& path, FSRef* ref) {
  OSStatus status = FSPathMakeRef((const UInt8*)path.c_str(),
                                  ref, nil);
  return status == noErr;
}

// Adapted from http://developer.apple.com/carbon/tipsandtricks.html#AmIBundled
bool AmIBundled() {
  ProcessSerialNumber psn = {0, kCurrentProcess};

  FSRef fsref;
  if (GetProcessBundleLocation(&psn, &fsref) != noErr)
    return false;

  FSCatalogInfo info;
  if (FSGetCatalogInfo(&fsref, kFSCatInfoNodeFlags, &info,
                       NULL, NULL, NULL) != noErr) {
    return false;
  }

  return info.nodeFlags & kFSNodeIsDirectoryMask;
}

}  // namespace mac_util
