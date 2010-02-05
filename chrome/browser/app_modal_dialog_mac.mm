// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#import <Cocoa/Cocoa.h>

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
  NOTIMPLEMENTED();
}

void AppModalDialog::ActivateModalDialog() {
  NOTIMPLEMENTED();
}

void AppModalDialog::CloseModalDialog() {
  NSAlert* alert = dialog_;
  DCHECK([alert isKindOfClass:[NSAlert class]]);
  [NSApp endSheet:[alert window]];
  dialog_ = nil;
}
