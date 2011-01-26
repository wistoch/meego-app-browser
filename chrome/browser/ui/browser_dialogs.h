// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#pragma once

#include "ipc/ipc_message.h"

namespace browser {

#if defined(IPC_MESSAGE_LOG_ENABLED)

// The dialog is a singleton. If the dialog is already opened, it won't do
// anything, so you can just blindly call this function all you want.
// This is Called from chrome/browser/browser_about_handler.cc
void ShowAboutIPCDialog();

#endif  // IPC_MESSAGE_LOG_ENABLED

} // namespace browser

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
