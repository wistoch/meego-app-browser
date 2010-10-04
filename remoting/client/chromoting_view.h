// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_VIEW_H_
#define REMOTING_CLIENT_CHROMOTING_VIEW_H_

#include "base/ref_counted.h"
#include "media/base/video_frame.h"

class MessageLoop;

namespace base {
class WaitableEvent;
}  // namespace base

namespace remoting {

// ChromotingView defines the behavior of an object that draws a view of the
// remote desktop. Its main function is to render the update stream onto the
// screen.
class ChromotingView {
 public:
  ChromotingView();
  virtual ~ChromotingView();

  // Get screen dimensions.
  // TODO(garykac): This will need to be extended to support multi-monitors.
  void GetScreenSize(int* width, int* height);

  // Initialize the common structures for the view.
  virtual bool Initialize() = 0;

  // Free up resources allocated by this view.
  virtual void TearDown() = 0;

  // Tells the ChromotingView to paint the current image on the screen.
  // TODO(hclam): Add rects as parameter if needed.
  virtual void Paint() = 0;

  // Fill the screen with one single static color, and ignore updates.
  // Useful for debugging.
  virtual void SetSolidFill(uint32 color) = 0;

  // Removes a previously set solid fill.  If no fill was previous set, this
  // does nothing.
  virtual void UnsetSolidFill() = 0;

  // Reposition and resize the viewport into the backing store. If the viewport
  // extends past the end of the backing store, it is filled with black.
  virtual void SetViewport(int x, int y, int width, int height) = 0;

 protected:
  // Framebuffer for the decoder.
  scoped_refptr<media::VideoFrame> frame_;

  // Dimensions of |frame_| bitmap.
  int frame_width_;
  int frame_height_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_VIEW_H_
