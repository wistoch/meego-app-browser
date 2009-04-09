// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/go_button.h"

#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// GoButton, public:

GoButton::GoButton(LocationBarView* location_bar, Browser* browser)
    : ToggleImageButton(this),
      location_bar_(location_bar),
      browser_(browser),
      intended_mode_(MODE_GO),
      visible_mode_(MODE_GO),
      button_delay_(NULL),
      stop_timer_(this) {
  DCHECK(location_bar_);
  set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                           views::Event::EF_MIDDLE_BUTTON_DOWN);
}

GoButton::~GoButton() {
  stop_timer_.RevokeAll();
}

void GoButton::ChangeMode(Mode mode) {
  stop_timer_.RevokeAll();

  SetToggled(mode == MODE_STOP);
  intended_mode_ = mode;
  visible_mode_ = mode;
}

void GoButton::ScheduleChangeMode(Mode mode) {
  if (mode == MODE_STOP) {
    // If we still have a timer running, we can't yet change to a stop sign,
    // so we'll queue up the change for when the timer expires or for when
    // the mouse exits the button.
    if (!stop_timer_.empty() && state() == BS_HOT) {
      intended_mode_ = MODE_STOP;
    } else {
      ChangeMode(MODE_STOP);
    }
  } else {
    // If we want to change the button to a go button, but the user's mouse
    // is hovering, don't change the mode just yet - this prevents the
    // stop button changing to a go under the user's mouse cursor.
    if (visible_mode_ == MODE_STOP && state() == BS_HOT) {
      intended_mode_ = MODE_GO;
    } else {
      ChangeMode(MODE_GO);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, views::ButtonListener implementation:

void GoButton::ButtonPressed(views::Button* button) {
  if (visible_mode_ == MODE_STOP) {
    browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_GO);
  } else if (visible_mode_ == MODE_GO && stop_timer_.empty()) {
    // If the go button is visible and not within the double click timer, go.
    browser_->Go(event_utils::DispositionFromEventFlags(mouse_event_flags()));

    // Figure out the system double-click time.
    if (button_delay_ == NULL)
      button_delay_ = GetDoubleClickTime();

    // Stop any existing timers.
    stop_timer_.RevokeAll();

    // Start a timer - while this timer is running, the go button
    // cannot be changed to a stop button. We do not set intended_mode_
    // to MODE_STOP here as we want to wait for the browser to tell
    // us that it has started loading (and this may occur only after
    // some delay).
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        stop_timer_.NewRunnableMethod(&GoButton::OnButtonTimer),
        button_delay_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, View overrides:

void GoButton::OnMouseExited(const views::MouseEvent& e) {
  if (visible_mode_ != intended_mode_)
    ChangeMode(intended_mode_);

  if (state() != BS_DISABLED)
    SetState(BS_NORMAL);
}

bool GoButton::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (visible_mode_ == MODE_STOP) {
    tooltip->assign(l10n_util::GetString(IDS_TOOLTIP_STOP));
    return true;
  }

  std::wstring current_text(location_bar_->location_entry()->GetText());
  if (current_text.empty())
    return false;

  // Need to make sure the text direction is adjusted based on the locale so
  // that pure LTR strings are displayed appropriately on RTL locales. For
  // example, not adjusting the string will cause the URL
  // "http://www.google.com/" to be displayed in the tooltip as
  // "/http://www.google.com".
  //
  // Note that we mark the URL's text as LTR (instead of examining the
  // characters and guessing the text directionality) since URLs are always
  // treated as left-to-right text, even when they contain RTL characters.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&current_text);

  // TODO(pkasting): http://b/868940 Use the right strings at the right times by
  // asking the autocomplete system what to do.  Don't hardcode "Google" as the
  // search provider name.
  tooltip->assign(true ?
      l10n_util::GetStringF(IDS_TOOLTIP_GO_SITE, current_text) :
      l10n_util::GetStringF(IDS_TOOLTIP_GO_SEARCH, L"Google", current_text));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// GoButton, private:

void GoButton::OnButtonTimer() {
  if (intended_mode_ != visible_mode_)
    ChangeMode(intended_mode_);

  stop_timer_.RevokeAll();
}
