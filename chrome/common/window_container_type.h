// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WINDOW_CONTAINER_TYPE_H_
#define CHROME_COMMON_WINDOW_CONTAINER_TYPE_H_

namespace WebKit {

struct WebWindowFeatures;

}

// "Container" types which can be requested via the window.open feature
// string.
enum WindowContainerType {
  // A window shown in popup or tab.
  WINDOW_CONTAINER_TYPE_NORMAL = 0,

  // A window run as a hidden "background" page.
  WINDOW_CONTAINER_TYPE_BACKGROUND,

  // A window run as a hidden "background" page that wishes to be started
  // upon browser launch and run beyond the lifetime of the pages that
  // reference it.
  WINDOW_CONTAINER_TYPE_PERSISTENT,

  WINDOW_CONTAINER_TYPE_MAX_VALUE
};

// Conversion function:
WindowContainerType WindowFeaturesToContainerType(
    const WebKit::WebWindowFeatures& window_features);

#endif  // CHROME_COMMON_WINDOW_CONTAINER_TYPE_H_
