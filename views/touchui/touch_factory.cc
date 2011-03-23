// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_factory.h"

#include <X11/cursorfont.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"

// The X cursor is hidden if it is idle for kCursorIdleSeconds seconds.
static int kCursorIdleSeconds = 5;

namespace views {

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

TouchFactory::TouchFactory()
    : is_cursor_visible_(true),
      cursor_timer_(),
      touch_device_list_() {
  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Display* display = ui::GetXDisplay();
  Pixmap blank = XCreateBitmapFromData(display, ui::GetX11RootWindow(),
                                       nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(display, blank, blank,
                                          &black, &black, 0, 0);
  arrow_cursor_ = XCreateFontCursor(display, XC_arrow);

  SetCursorVisible(false, false);

  // Detect touch devices.
  // NOTE: The new API for retrieving the list of devices (XIQueryDevice) does
  // not provide enough information to detect a touch device. As a result, the
  // old version of query function (XListInputDevices) is used instead.
  int count = 0;
  XDeviceInfo* devlist = XListInputDevices(display, &count);
  for (int i = 0; i < count; i++) {
    const char* devtype = XGetAtomName(display, devlist[i].type);
    if (devtype && !strcmp(devtype, XI_TOUCHSCREEN)) {
      touch_device_lookup_[devlist[i].id] = true;
      touch_device_list_.push_back(devlist[i].id);
    }
  }
  XFreeDeviceList(devlist);
}

TouchFactory::~TouchFactory() {
  SetCursorVisible(true, false);
  Display* display = ui::GetXDisplay();
  XFreeCursor(display, invisible_cursor_);
  XFreeCursor(display, arrow_cursor_);
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<unsigned int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (std::vector<unsigned int>::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    DCHECK(*iter < touch_device_lookup_.size());
    touch_device_lookup_[*iter] = true;
    touch_device_list_.push_back(*iter);
  }
}

bool TouchFactory::IsTouchDevice(unsigned deviceid) {
  return deviceid < touch_device_lookup_.size() ?
      touch_device_lookup_[deviceid] : false;
}

bool TouchFactory::GrabTouchDevices(Display* display, ::Window window) {
  if (touch_device_list_.empty())
    return true;

  unsigned char mask[(XI_LASTEVENT + 7) / 8];
  bool success = true;

  memset(mask, 0, sizeof(mask));
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    evmask.deviceid = *iter;
    Status status = XIGrabDevice(display, *iter, window, CurrentTime, None,
                                 GrabModeAsync, GrabModeAsync, False, &evmask);
    success = success && status == GrabSuccess;
  }

  return success;
}

bool TouchFactory::UngrabTouchDevices(Display* display) {
  bool success = true;
  for (std::vector<int>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    Status status = XIUngrabDevice(display, *iter, CurrentTime);
    success = success && status == GrabSuccess;
  }
  return success;
}

void TouchFactory::SetCursorVisible(bool show, bool start_timer) {
  // The cursor is going to be shown. Reset the timer for hiding it.
  if (show && start_timer) {
    cursor_timer_.Stop();
    cursor_timer_.Start(base::TimeDelta::FromSeconds(kCursorIdleSeconds),
        this, &TouchFactory::HideCursorForInactivity);
  } else {
    cursor_timer_.Stop();
  }

  if (show == is_cursor_visible_)
    return;

  is_cursor_visible_ = show;

  Display* display = ui::GetXDisplay();
  Window window = DefaultRootWindow(display);

  if (is_cursor_visible_) {
    XDefineCursor(display, window, arrow_cursor_);
  } else {
    XDefineCursor(display, window, invisible_cursor_);
  }
}

}  // namespace views
