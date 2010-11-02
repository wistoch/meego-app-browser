// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer_linux.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>

#include <set>

#include "remoting/base/types.h"

namespace remoting {

static int IndexOfLowestBit(unsigned int mask) {
  int i = 0;

  // Extra-special do-while premature optimization, just to make dmaclach@
  // happy.
  do {
    if (mask & 1) {
      return i;
    }
    mask >>= 1;
    ++i;
  } while (mask);

  NOTREACHED() << "mask should never be 0.";
  return 0;
}

static bool IsRgb32(XImage* image) {
  return (IndexOfLowestBit(image->red_mask) == 16) &&
         (IndexOfLowestBit(image->green_mask) == 8) &&
         (IndexOfLowestBit(image->blue_mask) == 0);
}

static uint32_t* GetRowStart(uint8* buffer_start, int stride, int cur_row) {
  return reinterpret_cast<uint32_t*>(buffer_start + (cur_row * stride));
}

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
  void FastBlit(XImage* image, int dest_x, int dest_y,
                CaptureData* capture_data);
  void SlowBlit(XImage* image, int dest_x, int dest_y,
                CaptureData* capture_data);

  static const int kBytesPerPixel = 4;

  // Reference to containing class so we can access friend functions.
  CapturerLinux* capturer_;

  // X11 graphics context.
  Display* display_;
  GC gc_;
  Window root_window_;

  // XDamage information.
  Damage damage_handle_;
  int damage_event_base_;
  int damage_error_base_;

  // Capture state.
  uint8* buffers_[CapturerLinux::kNumBuffers];
  int stride_;
  bool capture_fullscreen_;
};

CapturerLinux::CapturerLinux()
    : pimpl_(new CapturerLinuxPimpl(this)) {
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
      damage_handle_(BadValue),
      damage_event_base_(-1),
      damage_error_base_(-1),
      stride_(0),
      capture_fullscreen_(true) {
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
  capturer_->width_ = root_attr.width;
  capturer_->height_ = root_attr.height;
  stride_ = capturer_->width() * kBytesPerPixel;
  VLOG(1) << "Initialized with Geometry: " << capturer_->width()
          << "x" << capturer_->height();

  // Allocate the screen buffers.
  for (int i = 0; i < CapturerLinux::kNumBuffers; i++) {
    buffers_[i] =
        new uint8[capturer_->width() * capturer_->height() * kBytesPerPixel];
  }

  return true;
}

void CapturerLinuxPimpl::CalculateInvalidRects() {
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
        invalid_rects.insert(damage_rect);
        VLOG(3) << "Damage receved for rect at ("
                << damage_rect.x() << "," << damage_rect.y() << ") size ("
                << damage_rect.width() << "," << damage_rect.height() << ")";
      }
    } else {
      LOG(WARNING) << "Got unknown event type: " << e.type;
    }
  }

  if (capture_fullscreen_) {
    capturer_->InvalidateFullScreen();
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

  scoped_refptr<CaptureData> capture_data(
      new CaptureData(planes, capturer_->width(), capturer_->height(),
                      PIXEL_FORMAT_RGB32));

  for (InvalidRects::const_iterator it = rects.begin();
       it != rects.end();
       ++it) {
    XImage* image = XGetImage(display_, root_window_, it->x(), it->y(),
                              it->width(), it->height(), AllPlanes, ZPixmap);

    // Check if we can fastpath the blit.
    if ((image->depth == 24 || image->depth == 32) &&
        image->bits_per_pixel == 32 &&
        IsRgb32(image)) {
      VLOG(3) << "Fast blitting";
      FastBlit(image, it->x(), it->y(), capture_data);
    } else {
      VLOG(3) << "Slow blitting";
      SlowBlit(image, it->x(), it->y(), capture_data);
    }

    XDestroyImage(image);
  }

  // TODO(ajwong): We should only repair the rects that were copied!
  XDamageSubtract(display_, damage_handle_, None, None);

  capture_data->mutable_dirty_rects() = rects;
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

void CapturerLinuxPimpl::FastBlit(XImage* image, int dest_x, int dest_y,
                                  CaptureData* capture_data) {
  uint8* src_pos = reinterpret_cast<uint8*>(image->data);

  DataPlanes planes = capture_data->data_planes();
  uint8* dst_buffer = planes.data[0];

  const int dst_stride = planes.strides[0];
  const int src_stride = image->bytes_per_line;

  // TODO(ajwong): I think this can never happen anyways due to the way we size
  // the buffer. Check to be certain and possibly remove check.
  CHECK((dst_stride - dest_x) >= src_stride);

  // Flip the coordinate system to match the client.
  for (int y = image->height - 1; y >= 0; y--) {
    uint32_t* dst_pos = GetRowStart(dst_buffer, dst_stride, y + dest_y);
    dst_pos += dest_x;

    memcpy(dst_pos, src_pos, src_stride);

    src_pos += src_stride;
  }
}

void CapturerLinuxPimpl::SlowBlit(XImage* image, int dest_x, int dest_y,
                                  CaptureData* capture_data) {
  DataPlanes planes = capture_data->data_planes();
  uint8* dst_buffer = planes.data[0];
  const int dst_stride = planes.strides[0];

  unsigned int red_shift = IndexOfLowestBit(image->red_mask);
  unsigned int blue_shift = IndexOfLowestBit(image->blue_mask);
  unsigned int green_shift = IndexOfLowestBit(image->green_mask);

  unsigned int max_red = image->red_mask >> red_shift;
  unsigned int max_blue = image->blue_mask >> blue_shift;
  unsigned int max_green = image->green_mask >> green_shift;

  for (int y = 0; y < image->height; y++) {
    // Flip the coordinate system to match the client.
    int dst_row = image->height - y - 1;
    uint32_t* dst_pos = GetRowStart(dst_buffer, dst_stride, dst_row + dest_y);
    dst_pos += dest_x;
    for (int x = 0; x < image->width; x++) {
      unsigned long pixel = XGetPixel(image, x, y);
      uint32_t r = (((pixel & image->red_mask) >> red_shift) * max_red) / 255;
      uint32_t g =
          (((pixel & image->green_mask) >> green_shift) * max_blue) / 255;
      uint32_t b =
          (((pixel & image->blue_mask) >> blue_shift) * max_green) / 255;

      // Write as 32-bit RGB.
      *dst_pos = r << 16 | g << 8 | b;
      dst_pos++;
    }
  }
}

}  // namespace remoting
