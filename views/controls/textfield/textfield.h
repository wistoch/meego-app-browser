// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_

#if defined (OS_LINUX)
#include <gdk/gdk.h>
#endif

#include <string>

#include "app/gfx/font.h"
#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "base/string16.h"
#include "views/view.h"
#include "third_party/skia/include/core/SkColor.h"

#ifdef UNIT_TEST
#include "gfx/native_widget_types.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#endif

namespace views {

class NativeTextfieldWrapper;

// This class implements a ChromeView that wraps a native text (edit) field.
class Textfield : public View {
 public:
  // The button's class name.
  static const char kViewClassName[];

  // Keystroke provides a platform-dependent way to send keystroke events.
  // Cross-platform code can use IsKeystrokeEnter/Escape to check for these
  // two common key events.
  // TODO(brettw) this should be cleaned up to be more cross-platform.
  class Keystroke {
   public:
#if defined(OS_WIN)
    const Keystroke(unsigned int m,
              wchar_t k,
              int r,
              unsigned int f)
        : message_(m),
          key_(k),
          repeat_count_(r),
          flags_(f) {
    }
    unsigned int message() const { return message_; }
    wchar_t key() const { return key_; }
    int repeat_count() const { return repeat_count_; }
    unsigned int flags() const { return flags_; }
#else
    explicit Keystroke(GdkEventKey* event)
        : event_(*event) {
    }
    const GdkEventKey* event() const { return &event_; }
#endif
    base::KeyboardCode GetKeyboardCode() const;
    bool IsControlHeld() const;
    bool IsShiftHeld() const;

   private:
#if defined(OS_WIN)
    unsigned int message_;
    wchar_t key_;
    int repeat_count_;
    unsigned int flags_;
#else
    GdkEventKey event_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Keystroke);
  };

  // This defines the callback interface for other code to be notified of
  // changes in the state of a text field.
  class Controller {
   public:
    // This method is called whenever the text in the field changes.
    virtual void ContentsChanged(Textfield* sender,
                                 const string16& new_contents) = 0;

    // This method is called to get notified about keystrokes in the edit.
    // This method returns true if the message was handled and should not be
    // processed further. If it returns false the processing continues.
    virtual bool HandleKeystroke(Textfield* sender,
                                 const Textfield::Keystroke& keystroke) = 0;
  };

  enum StyleFlags {
    STYLE_DEFAULT = 0,
    STYLE_PASSWORD = 1<<0,
    STYLE_MULTILINE = 1<<1,
    STYLE_LOWERCASE = 1<<2
  };

  Textfield();
  explicit Textfield(StyleFlags style);
  virtual ~Textfield();


  // Controller accessors
  void SetController(Controller* controller);
  Controller* GetController() const;

  // Gets/Sets whether or not the Textfield is read-only.
  bool read_only() const { return read_only_; }
  void SetReadOnly(bool read_only);

  // Gets/Sets whether or not this Textfield is a password field.
  bool IsPassword() const;
  void SetPassword(bool password);

  // Whether the text field is multi-line or not, must be set when the text
  // field is created, using StyleFlags.
  bool IsMultiLine() const;

  // Gets/Sets the text currently displayed in the Textfield.
  const string16& text() const { return text_; }
  void SetText(const string16& text);

  // Appends the given string to the previously-existing text in the field.
  void AppendText(const string16& text);

  // Returns the text that is currently selected.
  string16 GetSelectedText() const;

  // Causes the edit field to be fully selected.
  void SelectAll();

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection() const;

  // Accessor for |style_|.
  StyleFlags style() const { return style_; }

  // Gets/Sets the text color to be used when painting the Textfield.
  // Call |UseDefaultTextColor| to return to the system default colors.
  SkColor text_color() const { return text_color_; }
  void SetTextColor(SkColor color);

  // Gets/Sets whether the default text color should be used when painting the
  // Textfield.
  bool use_default_text_color() const {
    return use_default_text_color_;
  }
  void UseDefaultTextColor();

  // Gets/Sets the background color to be used when painting the Textfield.
  // Call |UseDefaultBackgroundColor| to return to the system default colors.
  SkColor background_color() const { return background_color_; }
  void SetBackgroundColor(SkColor color);

  // Gets/Sets whether the default background color should be used when painting
  // the Textfield.
  bool use_default_background_color() const {
    return use_default_background_color_;
  }
  void UseDefaultBackgroundColor();

  // Gets/Sets the font used when rendering the text within the Textfield.
  gfx::Font font() const { return font_; }
  void SetFont(const gfx::Font& font);

  // Sets the left and right margin (in pixels) within the text box. On Windows
  // this is accomplished by packing the left and right margin into a single
  // 32 bit number, so the left and right margins are effectively 16 bits.
  void SetHorizontalMargins(int left, int right);

  // Should only be called on a multi-line text field. Sets how many lines of
  // text can be displayed at once by this text field.
  void SetHeightInLines(int num_lines);

  // Sets the default width of the text control. See default_width_in_chars_.
  void set_default_width_in_chars(int default_width) {
    default_width_in_chars_ = default_width;
  }

  // Removes the border from the edit box, giving it a 2D look.
  bool draw_border() const { return draw_border_; }
  void RemoveBorder();

  // Updates all properties on the textfield. This is invoked internally.
  // Users of Textfield never need to invoke this directly.
  void UpdateAllProperties();

  // Invoked by the edit control when the value changes. This method set
  // the text_ member variable to the value contained in edit control.
  // This is important because the edit control can be replaced if it has
  // been deleted during a window close.
  void SyncText();

#ifdef UNIT_TEST
  gfx::NativeView GetTestingHandle() const {
    return native_wrapper_ ? native_wrapper_->GetTestingHandle() : NULL;
  }
#endif

  // Overridden from View:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual bool IsFocusable() const;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse);
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e);
  virtual void SetEnabled(bool enabled);
  virtual void PaintFocusBorder(gfx::Canvas* canvas);

  // Accessibility accessors, overridden from View:
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleState(AccessibilityTypes::State* state);
  virtual void SetAccessibleName(const std::wstring& name);

 protected:
  virtual void Focus();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual std::string GetClassName() const;

  // The object that actually implements the native text field.
  NativeTextfieldWrapper* native_wrapper_;

 private:
  // This is the current listener for events from this Textfield.
  Controller* controller_;

  // The mask of style options for this Textfield.
  StyleFlags style_;

  // The font used to render the text in the Textfield.
  gfx::Font font_;

  // The text displayed in the Textfield.
  string16 text_;

  // True if this Textfield cannot accept input and is read-only.
  bool read_only_;

  // The default number of average characters for the width of this text field.
  // This will be reported as the "desired size". Defaults to 0.
  int default_width_in_chars_;

  // Whether the border is drawn.
  bool draw_border_;

  // The text color to be used when painting the Textfield, provided
  // |use_default_text_color_| is set to false.
  SkColor text_color_;

  // When true, the system text color for Textfields is used when painting this
  // Textfield. When false, the value of |text_color_| determines the
  // Textfield's text color.
  bool use_default_text_color_;

  // The background color to be used when painting the Textfield, provided
  // |use_default_background_color_| is set to false.
  SkColor background_color_;

  // When true, the system background color for Textfields is used when painting
  // this Textfield. When false, the value of |background_color_| determines the
  // Textfield's background color.
  bool use_default_background_color_;

  // The number of lines of text this Textfield displays at once.
  int num_lines_;

  // TODO(beng): remove this once NativeTextfieldWin subclasses
  //             NativeControlWin.
  bool initialized_;

  // The storage string for the accessibility name associated with this control.
  std::wstring accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(Textfield);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
