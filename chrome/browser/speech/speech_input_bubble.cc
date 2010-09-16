// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/speech/speech_input_bubble.h"
#include "gfx/canvas_skia.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

SpeechInputBubble::FactoryMethod SpeechInputBubble::factory_ = NULL;
const int SpeechInputBubble::kBubbleTargetOffsetX = 5;

SkBitmap* SpeechInputBubbleBase::mic_empty_ = NULL;
SkBitmap* SpeechInputBubbleBase::mic_full_ = NULL;
SkBitmap* SpeechInputBubbleBase::mic_mask_ = NULL;

SpeechInputBubble* SpeechInputBubble::Create(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect) {
  if (factory_)
    return (*factory_)(tab_contents, delegate, element_rect);

  // Has the tab already closed before bubble create request was processed?
  if (!tab_contents)
    return NULL;

  return CreateNativeBubble(tab_contents, delegate, element_rect);
}

SpeechInputBubbleBase::SpeechInputBubbleBase()
    : display_mode_(DISPLAY_MODE_RECORDING) {
  if (!mic_empty_) {  // Static variables.
    mic_empty_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_EMPTY);
    mic_full_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_FULL);
    mic_mask_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_MASK);
  }

  // Instance variables.
  mic_image_.reset(new SkBitmap());
  mic_image_->setConfig(SkBitmap::kARGB_8888_Config, mic_empty_->width(),
                        mic_empty_->height());
  mic_image_->allocPixels();

  buffer_image_.reset(new SkBitmap());
  buffer_image_->setConfig(SkBitmap::kARGB_8888_Config, mic_empty_->width(),
                           mic_empty_->height());
  buffer_image_->allocPixels();
}

SpeechInputBubbleBase::~SpeechInputBubbleBase() {
  // This destructor is added to make sure members such as the scoped_ptr
  // get destroyed here and the derived classes don't have to care about such
  // member variables which they don't use.
}

void SpeechInputBubbleBase::SetRecordingMode() {
  display_mode_ = DISPLAY_MODE_RECORDING;
  UpdateLayout();
}

void SpeechInputBubbleBase::SetRecognizingMode() {
  display_mode_ = DISPLAY_MODE_RECOGNIZING;
  UpdateLayout();
}

void SpeechInputBubbleBase::SetMessage(const string16& text) {
  message_text_ = text;
  display_mode_ = DISPLAY_MODE_MESSAGE;
  UpdateLayout();
}

void SpeechInputBubbleBase::SetInputVolume(float volume) {
  mic_image_->eraseARGB(0, 0, 0, 0);
  buffer_image_->eraseARGB(0, 0, 0, 0);

  int width = mic_image_->width();
  int height = mic_image_->height();
  SkCanvas canvas(*mic_image_);
  SkCanvas buffer_canvas(*buffer_image_);

  // The 'full volume' mic image is drawn clipped to the current volume level,
  // and a gradient mask is applied over it with the 'multiply' compositing
  // operator to show soft edges at the top.
  buffer_canvas.save();
  SkScalar clip_top = ((1.0f - volume) * height * 3) / 2.0f - height / 2.0f;
  buffer_canvas.clipRect(SkRect::MakeLTRB(0, clip_top,
      SkIntToScalar(width), SkIntToScalar(height)));
  buffer_canvas.drawBitmap(*mic_full_, 0, 0);
  buffer_canvas.restore();
  SkPaint multiply_paint;
  multiply_paint.setXfermode(SkXfermode::Create(SkXfermode::kMultiply_Mode));
  buffer_canvas.drawBitmap(*mic_mask_, 0, clip_top, &multiply_paint);

  // Draw the empty volume image first and the current volume image on top.
  canvas.drawBitmap(*mic_empty_, 0, 0);
  canvas.drawBitmap(*buffer_image_.get(), 0, 0);

  SetImage(*mic_image_.get());
}
