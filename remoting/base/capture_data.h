// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CAPTURE_DATA_H_
#define REMOTING_BASE_CAPTURE_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "gfx/rect.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

typedef std::vector<gfx::Rect> RectVector;

struct DataPlanes {
  static const int kPlaneCount = 3;
  uint8* data[kPlaneCount];
  int strides[kPlaneCount];

  DataPlanes() {
    for (int i = 0; i < kPlaneCount; ++i) {
      data[i] = NULL;
      strides[i] = 0;
    }
  }
};

// Stores the data and information of a capture to pass off to the
// encoding thread.
class CaptureData : public base::RefCountedThreadSafe<CaptureData> {
 public:
  CaptureData(const DataPlanes &data_planes,
              int width,
              int height,
              PixelFormat format) :
      data_planes_(data_planes), dirty_rects_(),
      width_(width), height_(height), pixel_format_(format) { }

  // Get the data_planes data of the last capture.
  const DataPlanes& data_planes() const { return data_planes_; }

  // Get the list of updated rectangles in the last capture. The result is
  // written into |rects|.
  const RectVector& dirty_rects() const { return dirty_rects_; }

  // Get the width of the image captured.
  int width() const { return width_; }

  // Get the height of the image captured.
  int height() const { return height_; }

  // Get the pixel format of the image captured.
  PixelFormat pixel_format() const { return pixel_format_; }

  // Mutating methods.
  RectVector& mutable_dirty_rects() { return dirty_rects_; }

 private:
  const DataPlanes data_planes_;
  RectVector dirty_rects_;
  int width_;
  int height_;
  PixelFormat pixel_format_;

  friend class base::RefCountedThreadSafe<CaptureData>;
  ~CaptureData() {}
};

}  // namespace remoting

#endif  // REMOTING_BASE_CAPTURE_DATA_H_
