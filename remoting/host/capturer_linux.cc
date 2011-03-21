// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_linux.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>

#include <set>

#include "remoting/base/types.h"
#include "remoting/host/x_server_pixel_buffer.h"

namespace remoting {

// Private Implementation pattern to avoid leaking the X11 types into the header
// file.
class CapturerLinuxPimpl {
 public:
  explicit CapturerLinuxPimpl(CapturerLinux* capturer);
  ~CapturerLinuxPimpl();

  bool Init();  // TODO(ajwong): Do we really want this to be synchronous?
  void CalculateInvalidRects();
  void CaptureRects(const InvalidRects& rects,
                    Capturer::CaptureCompletedCallback* callback);

 private:
  void DeinitXlib();
  // We expose two forms of blitting to handle variations in the pixel format.
  // In FastBlit, the operation is effectively a memcpy.
  void FastBlit(uint8* image, const gfx::Rect& rect, CaptureData* capture_data);
  void SlowBlit(uint8* image, const gfx::Rect& rect, CaptureData* capture_data);

  static const int kBytesPerPixel = 4;

  // Reference to containing class so we can access friend functions.
  // Not owned.
  CapturerLinux* capturer_;

  // X11 graphics context.
  Display* display_;
  GC gc_;
  Window root_window_;
  int width_;
  int height_;

  // XDamage information.
  Damage damage_handle_;
  int damage_event_base_;
  int damage_error_base_;

  // Access to the X Server's pixel buffer.
  XServerPixelBuffer x_server_pixel_buffer_;

  // Capture state.
  uint8* buffers_[CapturerLinux::kNumBuffers];
  int stride_;
  bool capture_fullscreen_;

  // Invalid rects in the last capture. This is used to synchronize current with
  // the previous buffer used.
  InvalidRects last_invalid_rects_;

  // Last capture buffer used.
  uint8* last_buffer_;
};

CapturerLinux::CapturerLinux(MessageLoop* message_loop)
    : Capturer(message_loop),
      pimpl_(new CapturerLinuxPimpl(this)) {
  // TODO(ajwong): This should be moved into an Init() method on Capturer
  // itself.  Then we can remove the CHECK.
  CHECK(pimpl_->Init());
}

CapturerLinux::~CapturerLinux() {
}

void CapturerLinux::ScreenConfigurationChanged() {
  // TODO(ajwong): Support resolution changes.
  NOTIMPLEMENTED();
}

void CapturerLinux::CalculateInvalidRects() {
  pimpl_->CalculateInvalidRects();
}

void CapturerLinux::CaptureRects(const InvalidRects& rects,
                                 CaptureCompletedCallback* callback) {
  pimpl_->CaptureRects(rects, callback);
}

CapturerLinuxPimpl::CapturerLinuxPimpl(CapturerLinux* capturer)
    : capturer_(capturer),
      display_(NULL),
      gc_(NULL),
      root_window_(BadValue),
      width_(0),
      height_(0),
      damage_handle_(BadValue),
      damage_event_base_(-1),
      damage_error_base_(-1),
      stride_(0),
      capture_fullscreen_(true),
      last_buffer_(NULL) {
  for (int i = 0; i < CapturerLinux::kNumBuffers; i++) {
    buffers_[i] = NULL;
  }
}

CapturerLinuxPimpl::~CapturerLinuxPimpl() {
  DeinitXlib();

  for (int i = 0; i < CapturerLinux::kNumBuffers; i++) {
    delete [] buffers_[i];
    buffers_[i] = NULL;
  }
}

bool CapturerLinuxPimpl::Init() {
  // TODO(ajwong): We should specify the display string we are attaching to
  // in the constructor.
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    LOG(ERROR) << "Unable to open display";
    return false;
  }

  x_server_pixel_buffer_.Init(display_);

  root_window_ = RootWindow(display_, DefaultScreen(display_));
  if (root_window_ == BadValue) {
    LOG(ERROR) << "Unable to get the root window";
    DeinitXlib();
    return false;
  }

  gc_ = XCreateGC(display_, root_window_, 0, NULL);
  if (gc_ == NULL) {
    LOG(ERROR) << "Unable to get graphics context";
    DeinitXlib();
    return false;
  }

  // Setup XDamage to report changes in the damage window.  Mark the whole
  // window as invalid.
  if (!XDamageQueryExtension(display_, &damage_event_base_,
                             &damage_error_base_)) {
    LOG(ERROR) << "Server does not support XDamage.";
    DeinitXlib();
    return false;
  }
  damage_handle_ = XDamageCreate(display_, root_window_,
                                 XDamageReportDeltaRectangles);
  if (damage_handle_ == BadValue) {
    LOG(ERROR) << "Unable to create damage handle.";
    DeinitXlib();
    return false;
  }

  // TODO(ajwong): We should be able to replace this with a XDamageAdd().
  capture_fullscreen_ = true;

  // Set up the dimensions of the catpure framebuffer.
  XWindowAttributes root_attr;
  XGetWindowAttributes(display_, root_window_, &root_attr);
  width_ = root_attr.width;
  height_ = root_attr.height;
  stride_ = width_ * kBytesPerPixel;
  VLOG(1) << "Initialized with Geometry: " << width_ << "x" << height_;

  // Allocate the screen buffers.
  for (int i = 0; i < CapturerLinux::kNumBuffers; i++) {
    buffers_[i] = new uint8[width_ * height_ * kBytesPerPixel];
  }

  return true;
}

void CapturerLinuxPimpl::CalculateInvalidRects() {
  if (capturer_->IsCaptureFullScreen(width_, height_))
    capture_fullscreen_ = true;

  // TODO(ajwong): The capture_fullscreen_ logic here is very ugly. Refactor.

  // Find the number of events that are outstanding "now."  We don't just loop
  // on XPending because we want to guarantee this terminates.
  int events_to_process = XPending(display_);
  XEvent e;
  InvalidRects invalid_rects;
  for (int i = 0; i < events_to_process; i++) {
    XNextEvent(display_, &e);
    if (e.type == damage_event_base_ + XDamageNotify) {
      // If we're doing a full screen capture, we should just drain the events.
      if (!capture_fullscreen_) {
        XDamageNotifyEvent *event = reinterpret_cast<XDamageNotifyEvent*>(&e);
        gfx::Rect damage_rect(event->area.x, event->area.y, event->area.width,
                              event->area.height);

        // TODO(hclam): Perform more checks on the rect.
        if (damage_rect.width() <= 0 && damage_rect.height() <= 0)
          continue;

        invalid_rects.insert(damage_rect);
        VLOG(3) << "Damage received for rect at ("
                << damage_rect.x() << "," << damage_rect.y() << ") size ("
                << damage_rect.width() << "," << damage_rect.height() << ")";
      }
    } else {
      LOG(WARNING) << "Got unknown event type: " << e.type;
    }
  }

  if (capture_fullscreen_) {
    // TODO(hclam): Check the new dimension again.
    capturer_->InvalidateScreen(gfx::Size(width_, height_));
    capture_fullscreen_ = false;
  } else {
    capturer_->InvalidateRects(invalid_rects);
  }
}

void CapturerLinuxPimpl::CaptureRects(
    const InvalidRects& rects,
    Capturer::CaptureCompletedCallback* callback) {
  uint8* buffer = buffers_[capturer_->current_buffer_];
  DataPlanes planes;
  planes.data[0] = buffer;
  planes.strides[0] = stride_;

  scoped_refptr<CaptureData> capture_data(new CaptureData(
      planes, gfx::Size(width_, height_), media::VideoFrame::RGB32));

  // Synchronize the current buffer with the last one since we do not capture
  // the entire desktop. Note that encoder may be reading from the previous
  // buffer at this time so thread access complaints are false positives.

  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |rects| from |last_invalid_rects_|.
  for (InvalidRects::const_iterator it = last_invalid_rects_.begin();
       last_buffer_ && it != last_invalid_rects_.end();
       ++it) {
    int offset = it->y() * stride_ + it->x() * kBytesPerPixel;
    for (int i = 0; i < it->height(); ++i) {
      memcpy(buffer + offset, last_buffer_ + offset,
             it->width() * kBytesPerPixel);
      offset += width_ * kBytesPerPixel;
    }
  }

  for (InvalidRects::const_iterator it = rects.begin();
       it != rects.end();
       ++it) {
    uint8* image = x_server_pixel_buffer_.CaptureRect(*it);
    // Check if we can fastpath the blit.
    int depth = x_server_pixel_buffer_.GetDepth();
    int bpp = x_server_pixel_buffer_.GetBitsPerPixel();
    bool is_rgb = x_server_pixel_buffer_.IsRgb();
    if ((depth == 24 || depth == 32) && bpp == 32 && is_rgb) {
      VLOG(3) << "Fast blitting";
      FastBlit(image, *it, capture_data);
    } else {
      VLOG(3) << "Slow blitting";
      SlowBlit(image, *it, capture_data);
    }
  }

  // TODO(ajwong): We should only repair the rects that were copied!
  XDamageSubtract(display_, damage_handle_, None, None);

  capture_data->mutable_dirty_rects() = rects;
  last_invalid_rects_ = rects;
  last_buffer_ = buffer;

  // TODO(ajwong): These completion signals back to the upper class are very
  // strange.  Fix it.
  capturer_->FinishCapture(capture_data, callback);
}

void CapturerLinuxPimpl::DeinitXlib() {
  if (gc_) {
    XFreeGC(display_, gc_);
    gc_ = NULL;
  }

  if (display_) {
    XCloseDisplay(display_);
    display_ = NULL;
  }
}

void CapturerLinuxPimpl::FastBlit(uint8* image, const gfx::Rect& rect,
                                  CaptureData* capture_data) {
  uint8* src_pos = image;
  int src_stride = x_server_pixel_buffer_.GetStride();
  int dst_x = rect.x(), dst_y = rect.y();

  DataPlanes planes = capture_data->data_planes();
  uint8* dst_buffer = planes.data[0];

  const int dst_stride = planes.strides[0];

  uint8* dst_pos = dst_buffer + dst_stride * dst_y;
  dst_pos += dst_x * kBytesPerPixel;

  int height = rect.height(), row_bytes = rect.width() * kBytesPerPixel;
  for (int y = 0; y < height; ++y) {
    memcpy(dst_pos, src_pos, row_bytes);
    src_pos += src_stride;
    dst_pos += dst_stride;
  }
}

void CapturerLinuxPimpl::SlowBlit(uint8* image, const gfx::Rect& rect,
                                  CaptureData* capture_data) {
  DataPlanes planes = capture_data->data_planes();
  uint8* dst_buffer = planes.data[0];
  const int dst_stride = planes.strides[0];
  int src_stride = x_server_pixel_buffer_.GetStride();
  int dst_x = rect.x(), dst_y = rect.y();
  int width = rect.width(), height = rect.height();

  unsigned int red_mask = x_server_pixel_buffer_.GetRedMask();
  unsigned int blue_mask = x_server_pixel_buffer_.GetBlueMask();
  unsigned int green_mask = x_server_pixel_buffer_.GetGreenMask();
  unsigned int red_shift = x_server_pixel_buffer_.GetRedShift();
  unsigned int blue_shift = x_server_pixel_buffer_.GetBlueShift();
  unsigned int green_shift = x_server_pixel_buffer_.GetGreenShift();

  unsigned int max_red = red_mask >> red_shift;
  unsigned int max_blue = blue_mask >> blue_shift;
  unsigned int max_green = green_mask >> green_shift;

  unsigned int bits_per_pixel = x_server_pixel_buffer_.GetBitsPerPixel();

  uint8* dst_pos = dst_buffer + dst_stride * dst_y;
  uint8* src_pos = image;
  dst_pos += dst_x * kBytesPerPixel;
  // TODO(hclam): Optimize, perhaps using MMX code or by converting to
  // YUV directly
  for (int y = 0; y < height; y++) {
    uint32_t* dst_pos_32 = reinterpret_cast<uint32_t*>(dst_pos);
    uint32_t* src_pos_32 = reinterpret_cast<uint32_t*>(src_pos);
    uint16_t* src_pos_16 = reinterpret_cast<uint16_t*>(src_pos);
    for (int x = 0; x < width; x++) {
      // Dereference through an appropriately-aligned pointer.
      uint32_t pixel;
      if (bits_per_pixel == 32)
        pixel = src_pos_32[x];
      else if (bits_per_pixel == 16)
        pixel = src_pos_16[x];
      else
        pixel = src_pos[x];
      uint32_t r = (((pixel & red_mask) >> red_shift) * 255) / max_red;
      uint32_t b = (((pixel & blue_mask) >> blue_shift) * 255) / max_blue;
      uint32_t g = (((pixel & green_mask) >> green_shift) * 255) / max_green;
      // Write as 32-bit RGB.
      dst_pos_32[x] = r << 16 | g << 8 | b;
    }
    dst_pos += dst_stride;
    src_pos += src_stride;
  }
}

// static
Capturer* Capturer::Create(MessageLoop* message_loop) {
  return new CapturerLinux(message_loop);
}

}  // namespace remoting
