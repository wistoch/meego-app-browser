// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#include "chrome/common/mac_app_names.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  bool success = false;
  if (result && PathService::Get(base::DIR_APP_DATA, result)) {
#if defined(GOOGLE_CHROME_BUILD)
    *result = result->Append("Google").Append("Chrome");
#else
    *result = result->Append("Chromium");
#endif
    success = true;
  }
  return success;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDownloadsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDesktop(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDesktopDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

FilePath GetFrameworkBundlePath() {
  // It's tempting to use +[NSBundle bundleWithIdentifier:], but it's really
  // slow (about 30ms on 10.5 and 10.6), despite Apple's documentation stating
  // that it may be more efficient than +bundleForClass:.  +bundleForClass:
  // itself takes 1-2ms.  Getting an NSBundle from a path, on the other hand,
  // essentially takes no time at all, at least when the bundle has already
  // been loaded as it will have been in this case.  The FilePath operations
  // needed to compute the framework's path are also effectively free, so that
  // is the approach that is used here.

  // Start out with the path to the running .app.
  FilePath path([[[NSBundle mainBundle] bundlePath] fileSystemRepresentation]);

  if (mac_util::IsBackgroundOnlyProcess()) {
    // path identifies the helper .app in the browser .app's Contents/Resources
    // directory.  Go up two levels to get to the browser's Contents directory.
    path = path.DirName().DirName();
  } else {
    // path identifies the browser .app.  Go into the Contents directory.
    path = path.Append("Contents");
  }

  // The framework bundle is at a known path and name from the browser .app's
  // Contents directory.
  return path.Append("Frameworks").Append(MAC_FRAMEWORK_NAME);
}

}  // namespace chrome
