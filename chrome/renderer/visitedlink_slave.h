// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WIN_RENDERER_VISITEDLINK_SLAVE_H__
#define CHROME_WIN_RENDERER_VISITEDLINK_SLAVE_H__

#include "base/shared_memory.h"
#include "chrome/common/visitedlink_common.h"

// Reads the link coloring database provided by the master. There can be any
// number of slaves reading the same database.
class VisitedLinkSlave : public VisitedLinkCommon
{
 public:
  VisitedLinkSlave();
  virtual ~VisitedLinkSlave();

  // Called to initialize this object, nothing will work until this is called.
  // It can also be called again at any time to update the table that we're
  // using. The handle should be the handle generated by the VisitedLinkMaster.
  // Returns true on success.
  bool Init(SharedMemoryHandle shared_memory);

 private:
  void FreeTable();

  // shared memory consists of a SharedHeader followed by the table
  SharedMemory* shared_memory_;

  DISALLOW_EVIL_CONSTRUCTORS(VisitedLinkSlave);
};

#endif // WIN_RENDERER_VISITEDLINK_SLAVE_H__
