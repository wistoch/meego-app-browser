// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SLIDER_SLIDER_H_
#define VIEWS_CONTROLS_SLIDER_SLIDER_H_

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include <string>

#include "base/basictypes.h"
#include "views/view.h"

namespace views {

class NativeSliderWrapper;
class Slider;

// An interface implemented by an object to let it know that the slider value
// was changed.
class SliderListener {
 public:
  virtual void SliderValueChanged(Slider* sender) = 0;
};

// This class implements a ChromeView that wraps a native slider.
class Slider : public View {
 public:
  // The slider's class name.
  static const char kViewClassName[];

  enum StyleFlags {
    STYLE_HORIZONTAL = 0,  // Horizontal is default type.
    STYLE_VERTICAL = 1<<0,
    STYLE_DRAW_VALUE = 1<<1,  // Display current value next to the slider.
    STYLE_ONE_DIGIT = 1<<2,  // 1 decimal place of precision for value.
    STYLE_TWO_DIGITS = 1<<3,  // 2 decimal places of precision for value.
    STYLE_UPDATE_ON_RELEASE = 1<<4,  // The slider will only notify value
                                     // changed on release of mouse
  };

  Slider();
  Slider(double min, double max, double step, StyleFlags style,
         SliderListener* listener);
  virtual ~Slider();

  // Cause the slider to notify the listener that the value has changed.
  virtual void NotifyValueChanged();

  // Gets/Sets the value in the slider.
  const double value() const { return value_; }
  void SetValue(double value);

  // Accessor for |style_|.
  StyleFlags style() const { return style_; }

  // Accessor for |min_|.
  const double min() const { return min_; }

  // Accessor for |max_|.
  const double max() const { return max_; }

  // Accessor for |step_|.
  const double step() const { return step_; }

  // Overridden from View:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual bool IsFocusable() const;
  virtual void SetEnabled(bool enabled);
  virtual void PaintFocusBorder(gfx::Canvas* canvas);

 protected:
  virtual void Focus();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual std::string GetClassName() const;

  // Creates a new native wrapper properly initialized and returns it. Ownership
  // is passed to the caller.
  NativeSliderWrapper* CreateWrapper();

 private:
  // The object that actually implements the native slider.
  NativeSliderWrapper* native_wrapper_;

  // The slider's listener. Notified when slider value changed.
  SliderListener* listener_;

  // The mask of style options for this Slider.
  StyleFlags style_;

  // The minimum value of the slider.
  double min_;

  // The maximum value of the slider.
  double max_;

  // The step increment of the slider.
  double step_;

  // The value displayed in the slider.
  double value_;

  DISALLOW_COPY_AND_ASSIGN(Slider);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SLIDER_SLIDER_H_
