// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// The name of the object property used to store an entry widget pointer on
// another widget.
const char kButtonDataKey[] = "label-entry";

// How far we indent dialog widgets, in pixels.
const int kAutoFillDialogIndent = 5;

// The resource id for the 'Learn more' link button.
const gint kAutoFillDialogLearnMoreLink = 1;

// All of these widgets are GtkEntrys except for default_profile, which is a
// GtkCheckButton.
typedef struct _AddressWidgets {
  GtkWidget* label;
  GtkWidget* default_profile;
  GtkWidget* first_name;
  GtkWidget* middle_name;
  GtkWidget* last_name;
  GtkWidget* email;
  GtkWidget* company_name;
  GtkWidget* address_line1;
  GtkWidget* address_line2;
  GtkWidget* city;
  GtkWidget* state;
  GtkWidget* zipcode;
  GtkWidget* country;
  GtkWidget* phone1;
  GtkWidget* phone2;
  GtkWidget* phone3;
  GtkWidget* fax1;
  GtkWidget* fax2;
  GtkWidget* fax3;
} AddressWidgets;

// All of these widgets are GtkEntrys except for default_profile, which is a
// GtkCheckButton, and billing/shipping_address are GtkComboBoxes.
typedef struct _CreditCardWidgets {
  GtkWidget* label;
  GtkWidget* default_creditcard;
  GtkWidget* name_on_card;
  GtkWidget* card_number;
  GtkWidget* expiration_month;
  GtkWidget* expiration_year;
  GtkWidget* verification_code;
  GtkWidget* billing_address;
  GtkWidget* shipping_address;
  GtkWidget* phone1;
  GtkWidget* phone2;
  GtkWidget* phone3;
  string16 original_card_number;
} CreditCardWidgets;

// Adds an alignment around |widget| which indents the widget by |offset|.
GtkWidget* IndentWidget(GtkWidget* widget, int offset) {
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            offset, 0);
  gtk_container_add(GTK_CONTAINER(alignment), widget);
  return alignment;
}

// Makes sure we use the gtk theme colors by loading the base color of an entry
// widget.
void SetWhiteBackground(GtkWidget* widget) {
  GtkWidget* entry = gtk_entry_new();
  gtk_widget_ensure_style(entry);
  GtkStyle* style = gtk_widget_get_style(entry);
  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL,
                       &style->base[GTK_STATE_NORMAL]);
  gtk_widget_destroy(entry);
}

string16 GetEntryText(GtkWidget* entry) {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(entry)));
}

void SetEntryText(GtkWidget* entry, const string16& text) {
  gtk_entry_set_text(GTK_ENTRY(entry), UTF16ToUTF8(text).c_str());
}

void SetButtonData(GtkWidget* widget, GtkWidget* entry) {
  g_object_set_data(G_OBJECT(widget), kButtonDataKey, entry);
}

GtkWidget* GetButtonData(GtkWidget* widget) {
  return static_cast<GtkWidget*>(
      g_object_get_data(G_OBJECT(widget), kButtonDataKey));
}

////////////////////////////////////////////////////////////////////////////////
// Form Table helpers.
//
// The following functions can be used to create a form with labeled widgets.
//

// Creates a form table with dimensions |rows| x |cols|.
GtkWidget* InitFormTable(int rows, int cols) {
  // We have two table rows per form table row.
  GtkWidget* table = gtk_table_new(rows * 2, cols, false);
  gtk_table_set_row_spacings(GTK_TABLE(table), gtk_util::kControlSpacing);
  gtk_table_set_col_spacings(GTK_TABLE(table), gtk_util::kFormControlSpacing);

  // Leave no space between the label and the widget.
  for (int i = 0; i < rows; i++)
    gtk_table_set_row_spacing(GTK_TABLE(table), i * 2, 0);

  return table;
}

// Sets the label of the form widget at |row|,|col|.  The label is |len| columns
// long.
void FormTableSetLabel(
    GtkWidget* table, int row, int col, int len, int label_id) {
  // We have two table rows per form table row.
  row *= 2;

  std::string text;
  if (label_id)
    text = l10n_util::GetStringUTF8(label_id);
  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_table_attach(GTK_TABLE(table), label,
                   col, col + len,  // Left col, right col.
                   row, row + 1,  // Top row, bottom row.
                   GTK_FILL, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Sets the form widget at |row|,|col|.  The widget fills up |len| columns.  If
// |expand| is true, the widget will expand to fill all of the extra space in
// the table row.
void FormTableSetWidget(GtkWidget* table,
                        GtkWidget* widget,
                        int row, int col,
                        int len, bool expand) {
  const GtkAttachOptions expand_option =
      static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND);
  GtkAttachOptions xoption = (expand) ?  expand_option : GTK_FILL;

  // We have two table rows per form table row.
  row *= 2;
  gtk_table_attach(GTK_TABLE(table), widget,
                   col, col + len,  // Left col, right col.
                   row + 1, row + 2,  // Top row, bottom row.
                   xoption, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Adds a labeled entry box to the form table at |row|,|col|.  The entry widget
// fills up |len| columns.  The returned widget is owned by |table| and should
// not be destroyed.
GtkWidget* FormTableAddEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, false);

  return entry;
}

// Adds a labeled entry box to the form table that will expand to fill extra
// space in the table row.
GtkWidget* FormTableAddExpandedEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, true);

  return entry;
}

// Adds a sized entry box to the form table.  The entry widget width is set to
// |char_len|.
GtkWidget* FormTableAddSizedEntry(
    GtkWidget* table, int row, int col, int char_len, int label_id) {
  GtkWidget* entry = FormTableAddEntry(table, row, col, 1, label_id);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), char_len);
  return entry;
}

// Like FormTableAddEntry, but connects to the 'changed' signal.  |changed| is a
// callback to handle the 'changed' signal that is emitted when the user edits
// the entry.  |expander| is the expander widget that will be sent to the
// callback as the user data.
GtkWidget* FormTableAddLabelEntry(
    GtkWidget* table, int row, int col, int len, int label_id,
    GtkWidget* expander, GCallback changed) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  g_signal_connect(entry, "changed", changed, expander);
  FormTableSetWidget(table, entry, row, col, len, false);

  return entry;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AutoFillDialog
//
// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog {
 public:
  AutoFillDialog(Profile* profile,
                 AutoFillDialogObserver* observer,
                 const std::vector<AutoFillProfile*>& profiles,
                 const std::vector<CreditCard*>& credit_cards);
  ~AutoFillDialog() {}

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  Calls DeleteSoon on the global singleton dialog
  // object.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'response' signal handler.  Notifies the AutoFillDialogObserver that new
  // data is available if the response is GTK_RESPONSE_APPLY or GTK_RESPONSE_OK.
  // We close the dialog if the response is GTK_RESPONSE_OK or
  // GTK_RESPONSE_CANCEL.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         AutoFillDialog* autofill_dialog);

  // 'clicked' signal handler.  Sets the default profile.
  static void OnDefaultProfileClicked(GtkWidget* button,
                                      AutoFillDialog* dialog);

  // 'clicked' signal handler.  Sets the default credit card.
  static void OnDefaultCreditCardClicked(GtkWidget* button,
                                         AutoFillDialog* dialog);

  // 'clicked' signal handler.  Adds a new address.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Adds a new credit card.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Deletes the associated address.
  static void OnDeleteAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Deletes the associated credit card.
  static void OnDeleteCreditCardClicked(GtkButton* button,
                                        AutoFillDialog* dialog);

  // 'changed' signal handler.  Updates the title of the expander widget with
  // the contents of the label entry widget.
  static void OnLabelChanged(GtkEntry* label, GtkWidget* expander);

  // Opens the 'Learn more' link in a new foreground tab.
  void OnLinkActivated();

  // Initializes the group widgets and returns their container.  |name_id| is
  // the resource ID of the group label.  |button_id| is the resource name of
  // the button label.  |clicked_callback| is a callback that handles the
  // 'clicked' signal emitted when the user presses the 'Add' button.
  GtkWidget* InitGroup(int label_id,
                       int button_id,
                       GCallback clicked_callback);

  // Initializes the expander, frame and table widgets used to hold the address
  // and credit card forms.  |name_id| is the resource id of the label of the
  // expander widget.  The content vbox widget is returned in |content_vbox|.
  // Returns the expander widget.
  GtkWidget* InitGroupContentArea(int name_id, GtkWidget** content_vbox);

  // Returns a GtkExpander that is added to the appropriate vbox.  Each method
  // adds the necessary widgets and layout required to fill out information
  // for either an address or a credit card.  The expander will be expanded by
  // default if |expand| is true.  The "Default Profile/Credit Card" button will
  // be toggled if |is_default| is true.
  GtkWidget* AddNewAddress(bool expand, bool is_default);
  GtkWidget* AddNewCreditCard(bool expand, bool is_default);

  // Adds a new address filled out with information from |profile|.  Sets the
  // "Default Profile" check button if |is_default| is true.
  void AddAddress(const AutoFillProfile& profile, bool is_default);

  // Adds a new credit card filled out with information from |credit_card|.
  void AddCreditCard(const CreditCard& credit_card, bool is_default);

  // The browser profile.  Unowned pointer, may not be NULL.
  Profile* profile_;

  // The list of current AutoFill profiles.
  std::vector<AutoFillProfile> profiles_;

  // The list of current AutoFill credit cards.
  std::vector<CreditCard> credit_cards_;

  // The list of address widgets, used to modify the AutoFill profiles.
  std::vector<AddressWidgets> address_widgets_;

  // The list of credit card widgets, used to modify the stored credit cards.
  std::vector<CreditCardWidgets> credit_card_widgets_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  // The addresses group.
  GtkWidget* addresses_vbox_;

  // The credit cards group.
  GtkWidget* creditcards_vbox_;

  // Our observer.
  AutoFillDialogObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(Profile* profile,
                               AutoFillDialogObserver* observer,
                               const std::vector<AutoFillProfile*>& profiles,
                               const std::vector<CreditCard*>& credit_cards)
    : profile_(profile),
      observer_(observer) {
  DCHECK(profile);
  DCHECK(observer);

  // Copy the profiles.
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter)
    profiles_.push_back(**iter);

  // Copy the credit cards.
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter)
    credit_cards_.push_back(**iter);

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_TITLE).c_str(),
      // AutoFill dialog is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_APPLY,
      GTK_RESPONSE_APPLY,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                       IDS_AUTOFILL_DIALOG_HEIGHT_LINES,
                                       true);

  // Allow browser windows to go in front of the AutoFill dialog in Metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  // Allow the contents to be scrolled.
  GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), scrolled_window);

  // We create an event box so that we can color the frame background white.
  GtkWidget* frame_event_box = gtk_event_box_new();
  SetWhiteBackground(frame_event_box);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
                                        frame_event_box);

  // The frame outline of the content area.
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(frame_event_box), frame);

  // The content vbox.
  GtkWidget* outer_vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(outer_vbox), gtk_util::kContentAreaSpacing);
  gtk_container_add(GTK_CONTAINER(frame), outer_vbox);

  addresses_vbox_ = InitGroup(IDS_AUTOFILL_ADDRESSES_GROUP_NAME,
                              IDS_AUTOFILL_ADD_ADDRESS_BUTTON,
                              G_CALLBACK(OnAddAddressClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), addresses_vbox_);

  string16 default_profile = WideToUTF16Hack(
      profile->GetPrefs()->GetString(prefs::kAutoFillDefaultProfile));
  for (std::vector<AutoFillProfile>::const_iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    AddAddress(*iter, iter->Label() == default_profile);
  }

  creditcards_vbox_ = InitGroup(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME,
                                IDS_AUTOFILL_ADD_CREDITCARD_BUTTON,
                                G_CALLBACK(OnAddCreditCardClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), creditcards_vbox_);

  string16 default_creditcard = WideToUTF16Hack(
      profile->GetPrefs()->GetString(prefs::kAutoFillDefaultCreditCard));
  for (std::vector<CreditCard>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    AddCreditCard(*iter, iter->Label() == default_creditcard);
  }

  GtkWidget* link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_LEARN_MORE).c_str());
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog_), link,
                               kAutoFillDialogLearnMoreLink);

  // Setting the link widget to secondary positions the button on the left side
  // of the action area (vice versa for RTL layout).
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area), link, TRUE);

  gtk_widget_show_all(dialog_);
}

void AutoFillDialog::Show() {
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());
}

// static
void AutoFillDialog::OnDestroy(GtkWidget* widget,
                               AutoFillDialog* autofill_dialog) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, autofill_dialog);
}

static AutoFillProfile AutoFillProfileFromWidgetValues(
    const AddressWidgets& widgets) {
  // TODO(jhawkins): unique id?
  AutoFillProfile profile(GetEntryText(widgets.label), 0);
  profile.SetInfo(AutoFillType(NAME_FIRST),
                  GetEntryText(widgets.first_name));
  profile.SetInfo(AutoFillType(NAME_MIDDLE),
      GetEntryText(widgets.middle_name));
  profile.SetInfo(AutoFillType(NAME_LAST),
      GetEntryText(widgets.last_name));
  profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
      GetEntryText(widgets.email));
  profile.SetInfo(AutoFillType(COMPANY_NAME),
      GetEntryText(widgets.company_name));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
      GetEntryText(widgets.address_line1));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
      GetEntryText(widgets.address_line2));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      GetEntryText(widgets.city));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE),
      GetEntryText(widgets.state));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP),
      GetEntryText(widgets.zipcode));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY),
      GetEntryText(widgets.country));
  profile.SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE),
      GetEntryText(widgets.phone1));
  profile.SetInfo(AutoFillType(PHONE_HOME_CITY_CODE),
      GetEntryText(widgets.phone2));
  profile.SetInfo(AutoFillType(PHONE_HOME_NUMBER),
      GetEntryText(widgets.phone3));
  profile.SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE),
      GetEntryText(widgets.fax1));
  profile.SetInfo(AutoFillType(PHONE_FAX_CITY_CODE),
      GetEntryText(widgets.fax2));
  profile.SetInfo(AutoFillType(PHONE_FAX_NUMBER),
      GetEntryText(widgets.fax3));
  return profile;
}

static CreditCard CreditCardFromWidgetValues(
    const CreditCardWidgets& widgets) {
  // TODO(jhawkins): unique id?
  CreditCard credit_card(GetEntryText(widgets.label), 0);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME),
                      GetEntryText(widgets.name_on_card));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
                      GetEntryText(widgets.expiration_month));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                      GetEntryText(widgets.expiration_year));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_VERIFICATION_CODE),
                      GetEntryText(widgets.verification_code));

  // If the CC number starts with an asterisk, then we know that the user has
  // not modified the credit card number at the least, so use the original CC
  // number in this case.
  string16 cc_number = GetEntryText(widgets.card_number);
  if (!cc_number.empty() && cc_number[0] == '*')
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                        widgets.original_card_number);
  else
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                        GetEntryText(widgets.card_number));
  // TODO(jhawkins): Billing/shipping addresses.
  return credit_card;
}

// static
void AutoFillDialog::OnResponse(GtkDialog* dialog, gint response_id,
                                AutoFillDialog* autofill_dialog) {
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK) {
    autofill_dialog->profiles_.clear();
    for (std::vector<AddressWidgets>::const_iterator iter =
             autofill_dialog->address_widgets_.begin();
         iter != autofill_dialog->address_widgets_.end();
         ++iter) {
      AutoFillProfile profile = AutoFillProfileFromWidgetValues(*iter);

      // Set this profile as the default profile if the check button is toggled.
      if (gtk_toggle_button_get_active(
          GTK_TOGGLE_BUTTON(iter->default_profile))) {
        autofill_dialog->profile_->GetPrefs()->SetString(
            prefs::kAutoFillDefaultProfile, UTF16ToWideHack(profile.Label()));
      }

      autofill_dialog->profiles_.push_back(profile);
    }

    autofill_dialog->credit_cards_.clear();
    for (std::vector<CreditCardWidgets>::const_iterator iter =
             autofill_dialog->credit_card_widgets_.begin();
         iter != autofill_dialog->credit_card_widgets_.end();
         ++iter) {
      CreditCard credit_card = CreditCardFromWidgetValues(*iter);

      // Set this credit card as the default profile if the check button is
      // toggled.
      if (gtk_toggle_button_get_active(
          GTK_TOGGLE_BUTTON(iter->default_creditcard))) {
        autofill_dialog->profile_->GetPrefs()->SetString(
            prefs::kAutoFillDefaultCreditCard,
            UTF16ToWideHack(credit_card.Label()));
      }

      autofill_dialog->credit_cards_.push_back(credit_card);
    }

    autofill_dialog->observer_->OnAutoFillDialogApply(
        &autofill_dialog->profiles_,
        &autofill_dialog->credit_cards_);
  }

  if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }

  if (response_id == kAutoFillDialogLearnMoreLink)
    autofill_dialog->OnLinkActivated();
}

// static
void AutoFillDialog::OnDefaultProfileClicked(GtkWidget* button,
                                             AutoFillDialog* dialog) {
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

  // The default profile defaults to the first profile if none is selected, so
  // set that here.
  if (!checked) {
    if (dialog->address_widgets_.size()) {
      GtkWidget* check_button = dialog->address_widgets_[0].default_profile;

      // Corner case: if the user is trying to untoggle the first profile, set
      // the second profile as the default profile if there is one.  If there's
      // only one profile, the user won't be able to uncheck the default profile
      // button.
      // TODO(jhawkins): Verify that this is the appropriate behavior.
      if (check_button == button && dialog->address_widgets_.size() > 1U)
        check_button = dialog->address_widgets_[1].default_profile;

      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), TRUE);
    }

    return;
  }

  for (std::vector<AddressWidgets>::iterator iter =
           dialog->address_widgets_.begin();
       iter != dialog->address_widgets_.end(); ++iter) {
    GtkWidget* check_button = iter->default_profile;

    // Don't reset the button that was just pressed.
    if (check_button == button)
      continue;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button)))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), FALSE);
  }
}

// static
void AutoFillDialog::OnDefaultCreditCardClicked(GtkWidget* button,
                                                AutoFillDialog* dialog) {
  bool checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

  // The default profile defaults to the first profile if none is selected, so
  // set that here.
  if (!checked) {
    if (dialog->credit_card_widgets_.size()) {
      GtkWidget* check_button =
          dialog->credit_card_widgets_[0].default_creditcard;

      // Corner case: If the user is trying to untoggle the first profile, set
      // the second profile as the default profile if there is one.  If there's
      // only one profile, the user won't be able to uncheck the default profile
      // button.
      // TODO(jhawkins): Verify that this is the appropriate behavior.
      if (check_button == button && dialog->credit_card_widgets_.size() > 1U)
        check_button = dialog->credit_card_widgets_[1].default_creditcard;

      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), TRUE);
    }

    return;
  }

  for (std::vector<CreditCardWidgets>::iterator iter =
           dialog->credit_card_widgets_.begin();
       iter != dialog->credit_card_widgets_.end(); ++iter) {
    GtkWidget* check_button = iter->default_creditcard;

    // Don't reset the button that was just pressed.
    if (check_button == button)
      continue;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button)))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), FALSE);
  }
}

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  // If this is the only address, make it the default profile.
  GtkWidget* new_address =
      dialog->AddNewAddress(true, !dialog->address_widgets_.size());
  gtk_box_pack_start(GTK_BOX(dialog->addresses_vbox_), new_address,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_address);
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  // If this is the only credit card, make it the default credit card.
  GtkWidget* new_creditcard =
      dialog->AddNewCreditCard(true, !dialog->credit_card_widgets_.size());
  gtk_box_pack_start(GTK_BOX(dialog->creditcards_vbox_), new_creditcard,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_creditcard);
}

// static
void AutoFillDialog::OnDeleteAddressClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* entry = GetButtonData(GTK_WIDGET(button));
  string16 label = GetEntryText(entry);

  // TODO(jhawkins): Base this on ID.

  // Remove the profile.
  for (std::vector<AutoFillProfile>::iterator iter = dialog->profiles_.begin();
       iter != dialog->profiles_.end();
       ++iter) {
    if (iter->Label() == label) {
      dialog->profiles_.erase(iter);
      break;
    }
  }

  // Remove the set of address widgets.
  for (std::vector<AddressWidgets>::iterator iter =
           dialog->address_widgets_.begin();
       iter != dialog->address_widgets_.end();
       ++iter) {
    if (iter->label == entry) {
      dialog->address_widgets_.erase(iter);
      break;
    }
  }

  // Get back to the expander widget.
  GtkWidget* expander = gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                GTK_TYPE_EXPANDER);
  DCHECK(expander);

  // Destroying the widget will also remove it from the parent container.
  gtk_widget_destroy(expander);
}

// static
void AutoFillDialog::OnDeleteCreditCardClicked(GtkButton* button,
                                               AutoFillDialog* dialog) {
  GtkWidget* entry = GetButtonData(GTK_WIDGET(button));
  string16 label = GetEntryText(entry);

  // TODO(jhawkins): Base this on ID.

  // Remove the credit card.
  for (std::vector<CreditCard>::iterator iter = dialog->credit_cards_.begin();
       iter != dialog->credit_cards_.end();
       ++iter) {
    if (iter->Label() == label) {
      dialog->credit_cards_.erase(iter);
      break;
    }
  }

  // Remove the set of credit widgets.
  for (std::vector<CreditCardWidgets>::iterator iter =
           dialog->credit_card_widgets_.begin();
       iter != dialog->credit_card_widgets_.end();
       ++iter) {
    if (iter->label == entry) {
      dialog->credit_card_widgets_.erase(iter);
      break;
    }
  }

  // Get back to the expander widget.
  GtkWidget* expander = gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                GTK_TYPE_EXPANDER);
  DCHECK(expander);

  // Destroying the widget will also remove it from the parent container.
  gtk_widget_destroy(expander);
}

// static
void AutoFillDialog::OnLabelChanged(GtkEntry* label, GtkWidget* expander) {
  gtk_expander_set_label(GTK_EXPANDER(expander), gtk_entry_get_text(label));
}

void AutoFillDialog::OnLinkActivated() {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(GURL(kAutoFillLearnMoreUrl), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::TYPED);
}

GtkWidget* AutoFillDialog::InitGroup(int name_id,
                                     int button_id,
                                     GCallback clicked_callback) {
  GtkWidget* vbox = gtk_vbox_new(false, gtk_util::kControlSpacing);

  // Group label.
  GtkWidget* label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(name_id));
  gtk_box_pack_start(GTK_BOX(vbox),
                     IndentWidget(label, kAutoFillDialogIndent),
                     FALSE, FALSE, 0);

  // Separator.
  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  // Add profile button.
  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(button_id).c_str());
  g_signal_connect(button, "clicked", clicked_callback, this);
  gtk_box_pack_end_defaults(GTK_BOX(vbox),
                            IndentWidget(button, kAutoFillDialogIndent));

  return vbox;
}

GtkWidget* AutoFillDialog::InitGroupContentArea(int name_id,
                                                GtkWidget** content_vbox) {
  GtkWidget* expander = gtk_expander_new(
      l10n_util::GetStringUTF8(name_id).c_str());

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(expander), frame);

  GtkWidget* vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(vbox), gtk_util::kControlSpacing);
  GtkWidget* vbox_alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(vbox_alignment),
                            gtk_util::kControlSpacing,
                            gtk_util::kControlSpacing,
                            gtk_util::kGroupIndent,
                            0);
  gtk_container_add(GTK_CONTAINER(vbox_alignment), vbox);
  gtk_container_add(GTK_CONTAINER(frame), vbox_alignment);

  *content_vbox = vbox;
  return expander;
}

GtkWidget* AutoFillDialog::AddNewAddress(bool expand, bool is_default) {
  AddressWidgets widgets = {0};
  GtkWidget* vbox;
  GtkWidget* address = InitGroupContentArea(IDS_AUTOFILL_NEW_ADDRESS, &vbox);

  gtk_expander_set_expanded(GTK_EXPANDER(address), expand);

  GtkWidget* table = InitFormTable(5, 3);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), table);

  widgets.label = FormTableAddLabelEntry(table, 0, 0, 1,
                                         IDS_AUTOFILL_DIALOG_LABEL,
                                         address, G_CALLBACK(OnLabelChanged));
  widgets.first_name = FormTableAddEntry(table, 1, 0, 1,
                                         IDS_AUTOFILL_DIALOG_FIRST_NAME);
  widgets.middle_name = FormTableAddEntry(table, 1, 1, 1,
                                          IDS_AUTOFILL_DIALOG_MIDDLE_NAME);
  widgets.last_name = FormTableAddEntry(table, 1, 2, 1,
                                        IDS_AUTOFILL_DIALOG_LAST_NAME);
  widgets.email = FormTableAddEntry(table, 2, 0, 1,
                                    IDS_AUTOFILL_DIALOG_EMAIL);
  widgets.company_name = FormTableAddEntry(table, 2, 1, 1,
                                           IDS_AUTOFILL_DIALOG_COMPANY_NAME);
  widgets.address_line1 = FormTableAddEntry(table, 3, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1);
  widgets.address_line2 = FormTableAddEntry(table, 4, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2);

  GtkWidget* default_check = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  widgets.default_profile = default_check;
  FormTableSetWidget(table, default_check, 0, 1, 1, false);
  g_signal_connect(default_check, "clicked",
                   G_CALLBACK(OnDefaultProfileClicked), this);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(default_check),
                               is_default);

  GtkWidget* address_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), address_table);

  widgets.city = FormTableAddEntry(address_table, 0, 0, 1,
                                   IDS_AUTOFILL_DIALOG_CITY);
  widgets.state = FormTableAddEntry(address_table, 0, 1, 1,
                                    IDS_AUTOFILL_DIALOG_STATE);
  widgets.zipcode = FormTableAddSizedEntry(address_table, 0, 2, 7,
                                           IDS_AUTOFILL_DIALOG_ZIP_CODE);
  widgets.country = FormTableAddSizedEntry(address_table, 0, 3, 10,
                                           IDS_AUTOFILL_DIALOG_COUNTRY);

  GtkWidget* phone_table = InitFormTable(1, 8);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  widgets.phone1 = FormTableAddSizedEntry(phone_table, 0, 0, 4,
                                          IDS_AUTOFILL_DIALOG_PHONE);
  widgets.phone2 = FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  widgets.phone3 = FormTableAddEntry(phone_table, 0, 2, 2, 0);
  widgets.fax1 = FormTableAddSizedEntry(phone_table, 0, 4, 4,
                                        IDS_AUTOFILL_DIALOG_FAX);
  widgets.fax2 = FormTableAddSizedEntry(phone_table, 0, 5, 4, 0);
  widgets.fax3 = FormTableAddEntry(phone_table, 0, 6, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  g_signal_connect(button, "clicked", G_CALLBACK(OnDeleteAddressClicked), this);
  SetButtonData(button, widgets.label);
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  address_widgets_.push_back(widgets);
  return address;
}

GtkWidget* AutoFillDialog::AddNewCreditCard(bool expand, bool is_default) {
  CreditCardWidgets widgets = {0};
  GtkWidget* vbox;
  GtkWidget* credit_card = InitGroupContentArea(IDS_AUTOFILL_NEW_CREDITCARD,
                                                &vbox);

  gtk_expander_set_expanded(GTK_EXPANDER(credit_card), expand);

  GtkWidget* label_table = InitFormTable(1, 2);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_table);

  widgets.label = FormTableAddLabelEntry(label_table, 0, 0, 1,
                                         IDS_AUTOFILL_DIALOG_LABEL, credit_card,
                                         G_CALLBACK(OnLabelChanged));

  // TODO(jhawkins): If there's not a default profile, automatically check this
  // check button.
  widgets.default_creditcard = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  FormTableSetWidget(label_table, widgets.default_creditcard, 0, 1, 1, true);
  g_signal_connect(widgets.default_creditcard, "clicked",
                   G_CALLBACK(OnDefaultCreditCardClicked), this);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.default_creditcard),
                               is_default);

  GtkWidget* name_cc_table = InitFormTable(2, 6);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), name_cc_table);

  widgets.name_on_card = FormTableAddExpandedEntry(
      name_cc_table, 0, 0, 3, IDS_AUTOFILL_DIALOG_NAME_ON_CARD);
  widgets.card_number = FormTableAddExpandedEntry(
      name_cc_table, 1, 0, 3, IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER);
  widgets.expiration_month = FormTableAddSizedEntry(name_cc_table, 1, 3, 2, 0);
  widgets.expiration_year = FormTableAddSizedEntry(name_cc_table, 1, 4, 4, 0);
  widgets.verification_code = FormTableAddSizedEntry(
      name_cc_table, 1, 5, 5, IDS_AUTOFILL_DIALOG_CVC);

  FormTableSetLabel(name_cc_table, 1, 3, 2,
                    IDS_AUTOFILL_DIALOG_EXPIRATION_DATE);

  gtk_table_set_col_spacing(GTK_TABLE(name_cc_table), 3, 2);

  GtkWidget* addresses_table = InitFormTable(2, 5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), addresses_table);

  FormTableSetLabel(addresses_table, 0, 0, 3,
                    IDS_AUTOFILL_DIALOG_BILLING_ADDRESS);

  GtkWidget* billing = gtk_combo_box_new_text();
  widgets.billing_address = billing;
  std::string combo_text = l10n_util::GetStringUTF8(
      IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS);
  gtk_combo_box_append_text(GTK_COMBO_BOX(billing), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(billing), 0);
  FormTableSetWidget(addresses_table, billing, 0, 0, 2, false);

  FormTableSetLabel(addresses_table, 1, 0, 3,
                    IDS_AUTOFILL_DIALOG_SHIPPING_ADDRESS);

  GtkWidget* shipping = gtk_combo_box_new_text();
  widgets.shipping_address = shipping;
  combo_text = l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);
  gtk_combo_box_append_text(GTK_COMBO_BOX(shipping), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(shipping), 0);
  FormTableSetWidget(addresses_table, shipping, 1, 0, 2, false);

  GtkWidget* phone_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  widgets.phone1 = FormTableAddSizedEntry(
      phone_table, 0, 0, 4, IDS_AUTOFILL_DIALOG_PHONE);
  widgets.phone2 = FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  widgets.phone3 = FormTableAddEntry(phone_table, 0, 2, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  g_signal_connect(button, "clicked",
                   G_CALLBACK(OnDeleteCreditCardClicked), this);
  SetButtonData(button, widgets.label);
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  credit_card_widgets_.push_back(widgets);
  return credit_card;
}

void AutoFillDialog::AddAddress(const AutoFillProfile& profile,
                                bool is_default) {
  GtkWidget* address = AddNewAddress(false, is_default);
  gtk_expander_set_label(GTK_EXPANDER(address),
                         UTF16ToUTF8(profile.Label()).c_str());

  // We just pushed the widgets to the back of the vector.
  const AddressWidgets& widgets = address_widgets_.back();
  SetEntryText(widgets.label, profile.Label());
  SetEntryText(widgets.first_name,
               profile.GetFieldText(AutoFillType(NAME_FIRST)));
  SetEntryText(widgets.middle_name,
               profile.GetFieldText(AutoFillType(NAME_MIDDLE)));
  SetEntryText(widgets.last_name,
               profile.GetFieldText(AutoFillType(NAME_LAST)));
  SetEntryText(widgets.email,
               profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)));
  SetEntryText(widgets.company_name,
               profile.GetFieldText(AutoFillType(COMPANY_NAME)));
  SetEntryText(widgets.address_line1,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)));
  SetEntryText(widgets.address_line2,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)));
  SetEntryText(widgets.city,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)));
  SetEntryText(widgets.state,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)));
  SetEntryText(widgets.zipcode,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)));
  SetEntryText(widgets.country,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)));
  SetEntryText(widgets.phone1,
               profile.GetFieldText(AutoFillType(PHONE_HOME_COUNTRY_CODE)));
  SetEntryText(widgets.phone2,
               profile.GetFieldText(AutoFillType(PHONE_HOME_CITY_CODE)));
  SetEntryText(widgets.phone3,
               profile.GetFieldText(AutoFillType(PHONE_HOME_NUMBER)));
  SetEntryText(widgets.fax1,
               profile.GetFieldText(AutoFillType(PHONE_FAX_COUNTRY_CODE)));
  SetEntryText(widgets.fax2,
               profile.GetFieldText(AutoFillType(PHONE_FAX_CITY_CODE)));
  SetEntryText(widgets.fax3,
               profile.GetFieldText(AutoFillType(PHONE_FAX_NUMBER)));

  gtk_box_pack_start(GTK_BOX(addresses_vbox_), address, FALSE, FALSE, 0);
  gtk_widget_show_all(address);
}

void AutoFillDialog::AddCreditCard(const CreditCard& credit_card,
                                   bool is_default) {
  GtkWidget* credit_card_widget = AddNewCreditCard(false, is_default);
  gtk_expander_set_label(GTK_EXPANDER(credit_card_widget),
                         UTF16ToUTF8(credit_card.Label()).c_str());

  // We just pushed the widgets to the back of the vector.
  const CreditCardWidgets& widgets = credit_card_widgets_.back();
  SetEntryText(widgets.label, credit_card.Label());
  SetEntryText(widgets.name_on_card,
               credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NAME)));
  // Set obfuscated number if not empty.
  credit_card_widgets_.back().original_card_number =
      credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER));
  string16 credit_card_number;
  if (!widgets.original_card_number.empty())
    credit_card_number = credit_card.ObfuscatedNumber();
  // TODO(jhawkins): Credit Card type?  Shouldn't be necessary.
  SetEntryText(widgets.card_number, credit_card_number);
  SetEntryText(widgets.expiration_month,
               credit_card.GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)));
  SetEntryText(
      widgets.expiration_year,
      credit_card.GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
  SetEntryText(
      widgets.verification_code,
      credit_card.GetFieldText(AutoFillType(CREDIT_CARD_VERIFICATION_CODE)));

  // TODO(jhawkins): Set the GtkComboBox widgets.

  gtk_box_pack_start(GTK_BOX(creditcards_vbox_), credit_card_widget,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(credit_card_widget);
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(gfx::NativeWindow parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile) {
  // It's possible we haven't shown the InfoBar yet, but if the user is in the
  // AutoFill dialog, she doesn't need to be asked to enable or disable
  // AutoFill.
  profile->GetPrefs()->SetBoolean(prefs::kAutoFillInfoBarShown, true);

  if (!dialog) {
    dialog = new AutoFillDialog(
        profile,
        observer,
        profile->GetPersonalDataManager()->profiles(),
        profile->GetPersonalDataManager()->credit_cards());
  }
  dialog->Show();
}
