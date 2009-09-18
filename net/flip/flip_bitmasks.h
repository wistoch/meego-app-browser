// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_FLIP_BITMASKS_H_
#define NET_FLIP_FLIP_BITMASKS_H_

namespace flip {

const int kStreamIdMask = 0x7fffffff;  // StreamId mask from the FlipHeader
const int kControlFlagMask = 0x8000;   // Control flag mask from the FlipHeader
const int kPriorityMask = 0xc0;        // Priority mask from the SYN_FRAME
}  // flip

#endif  // NET_FLIP_FLIP_BITMASKS_H_

