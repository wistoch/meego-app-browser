// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/keyboard_codes.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/event.h"
#include "views/widget/widget.h"

namespace views {

#define EXPECT_STR_EQ(ascii, utf16) \
  EXPECT_EQ(ASCIIToWide(ascii), UTF16ToWide(utf16))

// TODO(oshima): Move tests that are independent of TextfieldViews to
// textfield_unittests.cc once we move the test utility functions
// from chrome/browser/automation/ to app/test/.
class NativeTextfieldViewsTest : public ::testing::Test,
                                 public Textfield::Controller {
 public:
  NativeTextfieldViewsTest()
      : widget_(NULL),
        textfield_(NULL),
        textfield_view_(NULL),
        model_(NULL),
        message_loop_(MessageLoop::TYPE_UI) {
  }

  // ::testing::Test overrides.
  virtual void SetUp() {
    NativeTextfieldViews::SetEnableTextfieldViews(true);
  }

  virtual void TearDown() {
    NativeTextfieldViews::SetEnableTextfieldViews(false);
    if (widget_)
      widget_->Close();
  }

  // Textfield::Controller implementation:
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents){
    last_contents_ = new_contents;
  }

  virtual bool HandleKeystroke(Textfield* sender,
                               const Textfield::Keystroke& keystroke) {
    // TODO(oshima): figure out how to test the keystroke.
    return false;
  }

  void InitTextfield(Textfield::StyleFlags style) {
    ASSERT_FALSE(textfield_);
    textfield_ = new Textfield(style);
    textfield_->SetController(this);
    widget_ = Widget::CreatePopupWidget(
        Widget::NotTransparent,
        Widget::AcceptEvents,
        Widget::DeleteOnDestroy,
        Widget::DontMirrorOriginInRTL);
    widget_->Init(NULL, gfx::Rect());
    widget_->SetContentsView(textfield_);
    textfield_view_
        = static_cast<NativeTextfieldViews*>(textfield_->native_wrapper());
    DCHECK(textfield_view_);
    model_ = textfield_view_->model_.get();
  }

  bool SendKeyEventToTextfieldViews(app::KeyboardCode key_code,
                                    bool shift,
                                    bool control) {
    int flags = (shift ? KeyEvent::EF_SHIFT_DOWN : 0) |
        (control ? KeyEvent::EF_CONTROL_DOWN : 0);
    KeyEvent event(KeyEvent::ET_KEY_PRESSED, key_code, flags, 1, 0);
    return textfield_view_->OnKeyPressed(event);
  }

  bool SendKeyEventToTextfieldViews(app::KeyboardCode key_code) {
    return SendKeyEventToTextfieldViews(key_code, false, false);
  }

 protected:
  // We need widget to populate wrapper class.
  Widget* widget_;

  Textfield* textfield_;
  NativeTextfieldViews* textfield_view_;
  TextfieldViewsModel* model_;

  // A fake message loop for view's drawing events.
  MessageLoop message_loop_;

  // The string from Controller::ContentsChanged callback.
  string16 last_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViewsTest);
};

TEST_F(NativeTextfieldViewsTest, ModelChangesTeset) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", last_contents_);
  last_contents_.clear();

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", last_contents_);
  last_contents_.clear();

  // Cases where the callback should not be called.
  textfield_->SetText(ASCIIToUTF16("this is a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_EQ(string16(), last_contents_);

  textfield_->AppendText(string16());
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_EQ(string16(), last_contents_);

  EXPECT_EQ(string16(), textfield_->GetSelectedText());
  textfield_->SelectAll();
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_EQ(string16(), last_contents_);
}

TEST_F(NativeTextfieldViewsTest, KeyTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  SendKeyEventToTextfieldViews(app::VKEY_C, true, false);
  EXPECT_STR_EQ("C", textfield_->text());
  EXPECT_STR_EQ("C", last_contents_);
  last_contents_.clear();

  SendKeyEventToTextfieldViews(app::VKEY_R, false, false);
  EXPECT_STR_EQ("Cr", textfield_->text());
  EXPECT_STR_EQ("Cr", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT,
                              true /* shift */, false /* control */);
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT, true, false);
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(app::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(app::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEventToTextfieldViews(app::VKEY_Z, true, false);
  SendKeyEventToTextfieldViews(app::VKEY_E, true, false);
  SendKeyEventToTextfieldViews(app::VKEY_R, true, false);
  SendKeyEventToTextfieldViews(app::VKEY_O, true, false);
  SendKeyEventToTextfieldViews(app::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->text());

  SendKeyEventToTextfieldViews(app::VKEY_END, true, false);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(app::VKEY_HOME, true, false);
  EXPECT_STR_EQ("ZERO ", textfield_->GetSelectedText());
}

TEST_F(NativeTextfieldViewsTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  char test_str[] = "this is a test";
  for (size_t i = 0; i < sizeof(test_str); i++) {
    // This is ugly and should be replaced by a utility standard function.
    // See comment in NativeTextfieldViews::GetPrintableChar.
    char c = test_str[i];
    app::KeyboardCode code =
        c == ' ' ? app::VKEY_SPACE :
        static_cast<app::KeyboardCode>(app::VKEY_A + c - 'a');
    SendKeyEventToTextfieldViews(code);
  }
  EXPECT_STR_EQ(test_str, textfield_->text());

  // Move the cursor around.
  for (int i = 0; i < 6; i++) {
    SendKeyEventToTextfieldViews(app::VKEY_LEFT);
  }
  SendKeyEventToTextfieldViews(app::VKEY_RIGHT);

  // Delete using backspace and check resulting string.
  SendKeyEventToTextfieldViews(app::VKEY_BACK);
  EXPECT_STR_EQ("this is  test", textfield_->text());

  // Delete using delete key and check resulting string.
  for (int i = 0; i < 5; i++) {
    SendKeyEventToTextfieldViews(app::VKEY_DELETE);
  }
  EXPECT_STR_EQ("this is ", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll();
  SendKeyEventToTextfieldViews(app::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, PasswordTest) {
  InitTextfield(Textfield::STYLE_PASSWORD);
  textfield_->SetText(ASCIIToUTF16("my password"));
  // Just to make sure the text() and callback returns
  // the actual text instead of "*".
  EXPECT_STR_EQ("my password", textfield_->text());
  EXPECT_STR_EQ("my password", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, TestOnKeyPressReturnValue) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  EXPECT_TRUE(SendKeyEventToTextfieldViews(app::VKEY_A));
  // F24, up/down key won't be handled.
  EXPECT_FALSE(SendKeyEventToTextfieldViews(app::VKEY_F24));
  EXPECT_FALSE(SendKeyEventToTextfieldViews(app::VKEY_UP));
  EXPECT_FALSE(SendKeyEventToTextfieldViews(app::VKEY_DOWN));
}

}  // namespace views
