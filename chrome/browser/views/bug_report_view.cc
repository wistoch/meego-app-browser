// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bug_report_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_version_info.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "unicode/locid.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "app/x11_util.h"
#else
#include "app/win_util.h"
#endif

using views::ColumnSet;
using views::GridLayout;

// Report a bug data version.
static const int kBugReportVersion = 1;
static const int kScreenImageRadioGroup = 2;


// Number of lines description field can display at one time.
static const int kDescriptionLines = 5;

class BugReportComboBoxModel : public ComboboxModel {
 public:
  BugReportComboBoxModel() {}

  // ComboboxModel interface.
  virtual int GetItemCount() {
    return BugReportUtil::OTHER_PROBLEM + 1;
  }

  virtual std::wstring GetItemAt(int index) {
    return GetItemAtIndex(index);
  }

  static std::wstring GetItemAtIndex(int index) {
    switch (index) {
      case BugReportUtil::PAGE_WONT_LOAD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_WONT_LOAD);
      case BugReportUtil::PAGE_LOOKS_ODD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_LOOKS_ODD);
      case BugReportUtil::PHISHING_PAGE:
        return l10n_util::GetString(IDS_BUGREPORT_PHISHING_PAGE);
      case BugReportUtil::CANT_SIGN_IN:
        return l10n_util::GetString(IDS_BUGREPORT_CANT_SIGN_IN);
      case BugReportUtil::CHROME_MISBEHAVES:
        return l10n_util::GetString(IDS_BUGREPORT_CHROME_MISBEHAVES);
      case BugReportUtil::SOMETHING_MISSING:
        return l10n_util::GetString(IDS_BUGREPORT_SOMETHING_MISSING);
      case BugReportUtil::BROWSER_CRASH:
        return l10n_util::GetString(IDS_BUGREPORT_BROWSER_CRASH);
      case BugReportUtil::OTHER_PROBLEM:
        return l10n_util::GetString(IDS_BUGREPORT_OTHER_PROBLEM);
      default:
        NOTREACHED();
        return std::wstring();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BugReportComboBoxModel);
};

namespace browser {

// Global "display this dialog" function declared in browser_dialogs.h.
void ShowBugReportView(views::Window* parent,
                       Profile* profile,
                       TabContents* tab) {
  BugReportView* view = new BugReportView(profile, tab);

  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins).
  std::vector<unsigned char> *screenshot_png = new std::vector<unsigned char>;

#if defined(OS_LINUX)
  x11_util::GrabWindowSnapshot(parent->GetNativeWindow(), screenshot_png);
#else
  win_util::GrabWindowSnapshot(parent->GetNativeWindow(), screenshot_png);
#endif

  // Get the size of the parent window to capture screenshot dimensions
  gfx::Rect screenshot_size = parent->GetBounds();


  // The BugReportView takes ownership of the png data, and will dispose of
  // it in its destructor.
  view->set_png_data(screenshot_png);
  view->set_screenshot_size(screenshot_size);

  // Create and show the dialog.
  views::Window::CreateChromeWindow(parent->GetNativeWindow(), gfx::Rect(),
                                    view)->Show();
}

}  // namespace browser

// BugReportView - create and submit a bug report from the user.
// This is separate from crash reporting, which is handled by Breakpad.
//
BugReportView::BugReportView(Profile* profile, TabContents* tab)
    : include_page_source_checkbox_(NULL),
      include_page_image_checkbox_(NULL),
      profile_(profile),
      tab_(tab),
      problem_type_(0) {
  DCHECK(profile);
  SetupControl();

  // We want to use the URL of the current committed entry (the current URL may
  // actually be the pending one).
  if (tab->controller().GetActiveEntry()) {
    page_url_text_->SetText(UTF8ToUTF16(
        tab->controller().GetActiveEntry()->url().spec()));
  }

  // Retrieve the application version info.
  scoped_ptr<FileVersionInfo> version_info(
      chrome_app::GetChromeVersionInfo());
  if (version_info.get()) {
    version_ = version_info->product_name() + L" - " +
        version_info->file_version() +
        L" (" + version_info->last_change() + L")";
  }
}

BugReportView::~BugReportView() {
}

void BugReportView::SetupControl() {
  bug_type_model_.reset(new BugReportComboBoxModel);

  // Adds all controls.
  bug_type_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_BUG_TYPE));
  bug_type_combo_ = new views::Combobox(bug_type_model_.get());
  bug_type_combo_->set_listener(this);
  bug_type_combo_->SetAccessibleName(bug_type_label_->GetText());

  page_title_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_PAGE_TITLE));
  page_title_text_ = new views::Label(UTF16ToWideHack(tab_->GetTitle()));
  page_url_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_URL_LABEL));
  // page_url_text_'s text (if any) is filled in after dialog creation.
  page_url_text_ = new views::Textfield;
  page_url_text_->SetController(this);
  page_url_text_->SetAccessibleName(page_url_label_->GetText());

  description_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_DESCRIPTION_LABEL));
#if defined(OS_LINUX)
  // TODO(davemoore) Remove this when gtk textfields support multiline.
  description_text_ = new views::Textfield;
#else
  description_text_ =
      new views::Textfield(views::Textfield::STYLE_MULTILINE);
  description_text_->SetHeightInLines(kDescriptionLines);
  description_text_->SetAccessibleName(description_label_->GetText());
#endif

  include_page_source_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_SOURCE_CHKBOX));
  include_page_source_checkbox_->SetChecked(true);

#if defined(OS_CHROMEOS)
  include_last_screen_image_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_LAST_SCREEN_IMAGE),
      kScreenImageRadioGroup);
  last_screenshot_iv_ = new views::ImageView();

  include_new_screen_image_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_NEW_SCREEN_IMAGE),
      kScreenImageRadioGroup);

  include_system_information_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_SYSTEM_INFORMATION_CHKBOX));
  system_information_url_ = new views::Link(
      l10n_util::GetString(IDS_BUGREPORT_SYSTEM_INFORMATION_URL_TEXT));
  system_information_url_->SetController(this);

  include_last_screen_image_radio_->SetChecked(true);
  include_system_information_checkbox_->SetChecked(true);
#endif
  include_page_image_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_IMAGE_CHKBOX));
  include_page_image_checkbox_->SetChecked(true);

  // Arranges controls by using GridLayout.
  const int column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing * 2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Page Title and text.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_title_label_);
  layout->AddView(page_title_text_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Bug type and combo box.
  layout->StartRow(0, column_set_id);
  layout->AddView(bug_type_label_, 1, 1, GridLayout::LEADING, GridLayout::FILL);
  layout->AddView(bug_type_combo_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Page URL and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_url_label_);
  layout->AddView(page_url_text_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Description label and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(description_label_, 1, 1, GridLayout::LEADING,
                  GridLayout::LEADING);
  layout->AddView(description_text_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Checkboxes.
  // The include page source checkbox is hidden until we can make it work.
  // layout->StartRow(0, column_set_id);
  // layout->SkipColumns(1);
  // layout->AddView(include_page_source_checkbox_);
  // layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
#if defined(OS_CHROMEOS)
    // Radio boxes to select last screen shot or,
    layout->AddView(include_last_screen_image_radio_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    // new screenshot
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
    layout->AddView(include_new_screen_image_radio_);
    layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

    // Checkbox for system information
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
    layout->AddView(include_system_information_checkbox_);

    // TODO(rkc): Add a link once we're pulling system info, to it
#else
  if (include_page_image_checkbox_) {
    layout->StartRow(0, column_set_id);
    layout->SkipColumns(1);
    layout->AddView(include_page_image_checkbox_);
  }
#endif

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
}

gfx::Size BugReportView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_BUGREPORT_DIALOG_WIDTH_CHARS,
      IDS_BUGREPORT_DIALOG_HEIGHT_LINES));
}


void BugReportView::UpdateReportingControls(bool is_phishing_report) {
  // page source, screen/page images, system information
  // are not needed if it's a phishing report

  include_page_source_checkbox_->SetEnabled(!is_phishing_report);
  include_page_source_checkbox_->SetChecked(!is_phishing_report);

#if defined(OS_CHROMEOS)
  include_last_screen_image_radio_->SetEnabled(!is_phishing_report);
  include_new_screen_image_radio_->SetEnabled(!is_phishing_report);

  include_system_information_checkbox_->SetEnabled(!is_phishing_report);
  include_system_information_checkbox_->SetChecked(!is_phishing_report);

  system_information_url_->SetEnabled(!is_phishing_report);
#else
  if (include_page_image_checkbox_) {
    include_page_image_checkbox_->SetEnabled(!is_phishing_report);
    include_page_image_checkbox_->SetChecked(!is_phishing_report);
  }
#endif
}

void BugReportView::ItemChanged(views::Combobox* combobox,
                                int prev_index,
                                int new_index) {
  if (new_index == prev_index)
    return;

  problem_type_ = new_index;
  bool is_phishing_report = new_index == BugReportUtil::PHISHING_PAGE;

  description_text_->SetEnabled(!is_phishing_report);
  description_text_->SetReadOnly(is_phishing_report);
  if (is_phishing_report) {
    old_report_text_ = UTF16ToWide(description_text_->text());
    description_text_->SetText(string16());
  } else if (!old_report_text_.empty()) {
    description_text_->SetText(WideToUTF16Hack(old_report_text_));
    old_report_text_.clear();
  }

  UpdateReportingControls(is_phishing_report);
  GetDialogClientView()->UpdateDialogButtons();
}

void BugReportView::ContentsChanged(views::Textfield* sender,
                                    const string16& new_contents) {
}

bool BugReportView::HandleKeystroke(views::Textfield* sender,
                                    const views::Textfield::Keystroke& key) {
  return false;
}

std::wstring BugReportView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    if (problem_type_ == BugReportUtil::PHISHING_PAGE)
      return l10n_util::GetString(IDS_BUGREPORT_SEND_PHISHING_REPORT);
    else
      return l10n_util::GetString(IDS_BUGREPORT_SEND_REPORT);
  } else {
    return std::wstring();
  }
}

int BugReportView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

bool BugReportView::CanResize() const {
  return false;
}

bool BugReportView::CanMaximize() const {
  return false;
}

bool BugReportView::IsAlwaysOnTop() const {
  return false;
}

bool BugReportView::HasAlwaysOnTopMenu() const {
  return false;
}

bool BugReportView::IsModal() const {
  return true;
}

std::wstring BugReportView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BUGREPORT_TITLE);
}

bool BugReportView::Accept() {
  if (IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    if (problem_type_ == BugReportUtil::PHISHING_PAGE)
      BugReportUtil::ReportPhishing(tab_,
          UTF16ToUTF8(page_url_text_->text()));
    else
      BugReportUtil::SendReport(profile_,
          WideToUTF8(page_title_text_->GetText()),
          problem_type_,
          UTF16ToUTF8(page_url_text_->text()),
          UTF16ToUTF8(description_text_->text()),
#if defined(OS_CHROMEOS)
          include_new_screen_image_radio_->checked() && png_data_.get() ?
#else
          include_page_image_checkbox_->checked() && png_data_.get() ?
#endif
              reinterpret_cast<const char *>(&((*png_data_.get())[0])) : NULL,
          png_data_->size(), screenshot_size_.width(),
          screenshot_size_.height());
  }
  return true;
}

#if defined(OS_CHROMEOS)
void BugReportView::LinkActivated(views::Link* source,
                                    int event_flags) {
  GURL url;
  if (source == system_information_url_) {
    url = GURL(l10n_util::GetStringUTF16(IDS_BUGREPORT_SYSTEM_INFORMATION_URL));
  } else {
    NOTREACHED() << "Unknown link source";
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
}
#endif


views::View* BugReportView::GetContentsView() {
  return this;
}
