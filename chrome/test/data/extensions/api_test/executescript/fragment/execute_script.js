// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Notify the background page that we ran.
chrome.extension.sendRequest("execute_script");
