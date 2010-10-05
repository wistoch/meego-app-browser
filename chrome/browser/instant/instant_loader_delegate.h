// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_
#pragma once

#include "base/string16.h"

namespace gfx {
class Rect;
}

class InstantLoader;

// InstantLoader's delegate. This interface is implemented by InstantController.
class InstantLoaderDelegate {
 public:
  // Invoked when the loader is ready to be shown.
  virtual void ShowInstantLoader(InstantLoader* loader) = 0;

  // Invoked when the loader has suggested text.
  virtual void SetSuggestedTextFor(InstantLoader* loader,
                                   const string16& text) = 0;

  // Returns the bounds of instant.
  virtual gfx::Rect GetInstantBounds() = 0;

  // Returns true if instant should be committed on mouse up.
  virtual bool ShouldCommitInstantOnMouseUp() = 0;

  // Invoked when the the loader should be committed.
  virtual void CommitInstantLoader(InstantLoader* loader) = 0;

 protected:
  virtual ~InstantLoaderDelegate() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_
