// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_GRADIENT_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_GRADIENT_BUTTON_CELL_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

namespace ui {
class ThemeProvider;
}

// Base class for button cells for toolbar and bookmark bar.
//
// This is a button cell that handles drawing/highlighting of buttons.
// The appearance is determined by setting the cell's tag (not the
// view's) to one of the constants below (ButtonType).

// Set this as the cell's tag.
enum {
  kLeftButtonType = -1,
  kLeftButtonWithShadowType = -2,
  kStandardButtonType = 0,
  kRightButtonType = 1,
  kMiddleButtonType = 2,
  // Draws like a standard button, except when clicked where the interior
  // doesn't darken using the theme's "pressed" gradient. Instead uses the
  // normal un-pressed gradient.
  kStandardButtonTypeWithLimitedClickFeedback = 3,
};
typedef NSInteger ButtonType;

namespace gradient_button_cell {

// Pulsing state for this button.
typedef enum {
  // Stable states.
  kPulsedOn,
  kPulsedOff,
  // In motion which will end in a stable state.
  kPulsingOn,
  kPulsingOff,
  // In continuous motion.
  kPulsingContinuous,
} PulseState;

};


@interface GradientButtonCell : NSButtonCell {
 @private
  // Custom drawing means we need to perform our own mouse tracking if
  // the cell is setShowsBorderOnlyWhileMouseInside:YES.
  BOOL isMouseInside_;
  scoped_nsobject<NSTrackingArea> trackingArea_;
  BOOL shouldTheme_;
  CGFloat hoverAlpha_;  // 0-1. Controls the alpha during mouse hover
  NSTimeInterval lastHoverUpdate_;
  scoped_nsobject<NSGradient> gradient_;
  gradient_button_cell::PulseState pulseState_;
  CGFloat pulseMultiplier_;  // for selecting pulse direction when continuous.
  CGFloat outerStrokeAlphaMult_;  // For pulsing.
  scoped_nsobject<NSImage> overlayImage_;
}

// Turn off theming.  Temporary work-around.
- (void)setShouldTheme:(BOOL)shouldTheme;

- (void)drawBorderAndFillForTheme:(ui::ThemeProvider*)themeProvider
                      controlView:(NSView*)controlView
                        innerPath:(NSBezierPath*)innerPath
              showClickedGradient:(BOOL)showClickedGradient
            showHighlightGradient:(BOOL)showHighlightGradient
                       hoverAlpha:(CGFloat)hoverAlpha
                           active:(BOOL)active
                        cellFrame:(NSRect)cellFrame
                  defaultGradient:(NSGradient*)defaultGradient;

// Let the view know when the mouse moves in and out. A timer will update
// the current hoverAlpha_ based on these events.
- (void)setMouseInside:(BOOL)flag animate:(BOOL)animate;

// Gets the path which tightly bounds the outside of the button. This is needed
// to produce images of clear buttons which only include the area inside, since
// the background of the button is drawn by someone else.
- (NSBezierPath*)clipPathForFrame:(NSRect)cellFrame
                           inView:(NSView*)controlView;

// Turn on or off continuous pulsing.  When turning off continuous
// pulsing, leave our pulse state in the correct ending position for
// our isMouseInside_ property.  Public since it's called from the
// bookmark bubble.
- (void)setIsContinuousPulsing:(BOOL)continuous;

// Returns continuous pulse state.
- (BOOL)isContinuousPulsing;

// Safely stop continuous pulsing by turning off all timers.
// May leave the cell in an odd state.
// Needed by an owning control's dealloc routine.
- (void)safelyStopPulsing;

@property(assign, nonatomic) CGFloat hoverAlpha;

// An image that will be drawn after the normal content of the button cell,
// overlaying it.  Never themed.
@property(retain, nonatomic) NSImage* overlayImage;

@end

@interface GradientButtonCell(TestingAPI)
- (BOOL)isMouseInside;
- (BOOL)pulsing;
- (gradient_button_cell::PulseState)pulseState;
- (void)setPulseState:(gradient_button_cell::PulseState)pstate;
@end

#endif  // CHROME_BROWSER_UI_COCOA_GRADIENT_BUTTON_CELL_H_
