// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/tuple.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;

namespace {

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutoFillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}

  AutoFillProfile* GetLabeledProfile(const char* label) {
    for (std::vector<AutoFillProfile *>::iterator it = web_profiles_.begin();
         it != web_profiles_.end(); ++it) {
       if (!(*it)->Label().compare(ASCIIToUTF16(label)))
         return *it;
    }
    return NULL;
  }

 private:
  void CreateTestAutoFillProfiles(ScopedVector<AutoFillProfile>* profiles) {
    AutoFillProfile* profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Home", "Elvis", "Aaron",
                                      "Presley", "theking@gmail.com", "RCA",
                                      "3734 Elvis Presley Blvd.", "Apt. 10",
                                      "Memphis", "Tennessee", "38116", "USA",
                                      "12345678901", "");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Work", "Charles", "Hardin",
                                      "Holley", "buddy@gmail.com", "Decca",
                                      "123 Apple St.", "unit 6", "Lubbock",
                                      "Texas", "79401", "USA", "23456789012",
                                      "");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Empty", "", "", "", "", "", "",
                                      "", "", "", "", "", "", "");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "First", "Elvis Presley",
                                         "Visa", "1234567890123456", "04",
                                         "2012", "456", "Home", "");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Second", "Buddy Holly",
                                         "Mastercard", "0987654321098765", "10",
                                         "2014", "678", "", "");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Empty", "", "", "", "",
                                         "", "", "", "");
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestAutoFillManager : public AutoFillManager {
 public:
  explicit TestAutoFillManager(TabContents* tab_contents)
      : AutoFillManager(tab_contents, NULL) {
    test_personal_data_ = new TestPersonalDataManager();
    set_personal_data_manager(test_personal_data_.get());
  }

  virtual bool IsAutoFillEnabled() const { return true; }

  AutoFillProfile* GetLabeledProfile(const char* label) {
    return test_personal_data_->GetLabeledProfile(label);
  }

 private:
  scoped_refptr<TestPersonalDataManager> test_personal_data_;

  DISALLOW_COPY_AND_ASSIGN(TestAutoFillManager);
};

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         webkit_glue::FormField* field) {
  *field = webkit_glue::FormField(ASCIIToUTF16(label), ASCIIToUTF16(name),
                                  ASCIIToUTF16(value), ASCIIToUTF16(type), 0);
}

void CreateTestFormData(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Email", "email", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("", "ccyear", "", "text", &field);
  form->fields.push_back(field);
}

void CreateTestFormDataBilling(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 1", "billingAddr1", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 2", "billingAddr2", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("City", "billingCity", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("State", "billingState", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Postal Code", "billingZipcode", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Country", "billingCountry", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Email", "email", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("", "ccyear", "", "text", &field);
  form->fields.push_back(field);
}

class AutoFillManagerTest : public RenderViewHostTestHarness {
 public:
  AutoFillManagerTest() {}

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    autofill_manager_.reset(new TestAutoFillManager(contents()));
  }

  bool GetAutoFillSuggestionsMessage(int *page_id,
                                     std::vector<string16>* values,
                                     std::vector<string16>* labels,
                                     int* default_idx) {
    const uint32 kMsgID = ViewMsg_AutoFillSuggestionsReturned::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple4<int, std::vector<string16>, std::vector<string16>, int>
        autofill_param;
    ViewMsg_AutoFillSuggestionsReturned::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (values)
      *values = autofill_param.b;
    if (labels)
      *labels = autofill_param.c;
    if (default_idx)
      *default_idx = autofill_param.d;
    return true;
  }

  bool GetAutoFillFormDataFilledMessage(int *page_id, FormData* results) {
    const uint32 kMsgID = ViewMsg_AutoFillFormDataFilled::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<int, FormData> autofill_param;
    ViewMsg_AutoFillFormDataFilled::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (results)
      *results = autofill_param.b;
    return true;
  }

 protected:
  scoped_ptr<TestAutoFillManager> autofill_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillManagerTest);
};

TEST_F(AutoFillManagerTest, GetProfileSuggestionsEmptyValue) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Work"), labels[1]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetProfileSuggestionsMatchCharacter) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "E", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), labels[0]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsEmptyValue) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("************8765"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("First"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Second"), labels[1]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsMatchCharacter) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  CreateTestFormField("Card Number", "cardnumber", "1", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("First"), labels[0]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsNonCCNumber) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));

  CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));

  CreateTestFormField("", "ccyear", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
}

TEST_F(AutoFillManagerTest, FillCreditCardForm) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(
      autofill_manager_->FillAutoFillFormData(kPageID,
                                              form,
                                              ASCIIToUTF16("cardnumber"),
                                              ASCIIToUTF16("First")));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  ASSERT_EQ(15U, results.fields.size());

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
  CreateTestFormField("City", "city", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[5]));
  CreateTestFormField("State", "state", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[6]));
  CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[7]));
  CreateTestFormField("Country", "country", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[8]));
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[9]));
  CreateTestFormField("Email", "email", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[10]));
  CreateTestFormField(
      "Name on Card", "nameoncard", "Elvis Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[11]));
  CreateTestFormField(
      "Card Number", "cardnumber", "1234567890123456", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[12]));
  CreateTestFormField("Expiration Date", "ccmonth", "04", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[13]));
  CreateTestFormField("", "ccyear", "2012", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[14]));
}

TEST_F(AutoFillManagerTest, FillPhoneNumberTest) {
  FormData form;

  form.name = ASCIIToUTF16("MyPhoneForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/phone_form.html");
  form.action = GURL("http://myform.com/phone_submit.html");

  webkit_glue::FormField field;

  CreateTestFormField("country code", "country code", "", "text", &field);
  field.set_size(1);
  form.fields.push_back(field);
  CreateTestFormField("area code", "area code", "", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);
  CreateTestFormField("phone", "phone prefix", "1", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);
  CreateTestFormField("-", "phone suffix", "", "text", &field);
  field.set_size(4);
  form.fields.push_back(field);
  CreateTestFormField("Phone Extension", "ext", "", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  AutoFillProfile *work_profile = autofill_manager_->GetLabeledProfile("Work");
  EXPECT_TRUE(work_profile != NULL);
  const AutoFillType phone_type(PHONE_HOME_NUMBER);
  string16 saved_phone = work_profile->GetFieldText(phone_type);

  char test_data[] = "1234567890123456";
  for (int i = arraysize(test_data) - 1; i >= 0; --i) {
    test_data[i] = 0;
    work_profile->SetInfo(phone_type, ASCIIToUTF16(test_data));
    // The page ID sent to the AutoFillManager from the RenderView, used to send
    // an IPC message back to the renderer.
    int page_id = 100 - i;
    process()->sink().ClearMessages();
    EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(page_id,
         form,
         ASCIIToUTF16(test_data),
         ASCIIToUTF16("Work")));
    page_id = 0;
    FormData results;
    EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));

    if (i != 7) {
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[2].value());
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[3].value());
    } else {
      // The only size that is parsed and split, right now is 7:
      EXPECT_EQ(ASCIIToUTF16("123"), results.fields[2].value());
      EXPECT_EQ(ASCIIToUTF16("4567"), results.fields[3].value());
    }
  }

  work_profile->SetInfo(phone_type, saved_phone);
}

TEST_F(AutoFillManagerTest, FillCreditCardFormWithBilling) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(
      autofill_manager_->FillAutoFillFormData(kPageID,
                                              form,
                                              ASCIIToUTF16("cardnumber"),
                                              ASCIIToUTF16("First")));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  ASSERT_EQ(15U, results.fields.size());

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  CreateTestFormField(
      "Address Line 1", "billingAddr1", "3734 Elvis Presley Blvd.", "text",
      &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  CreateTestFormField(
      "Address Line 2", "billingAddr2", "Apt. 10", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
  CreateTestFormField("City", "billingCity", "Memphis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[5]));
  CreateTestFormField("State", "billingState", "Tennessee", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[6]));
  CreateTestFormField("Postal Code", "billingZipcode", "38116", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[7]));
  CreateTestFormField("Country", "billingCountry", "USA", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[8]));
  CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[9]));
  CreateTestFormField(
      "Email", "email", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[10]));
  CreateTestFormField(
      "Name on Card", "nameoncard", "Elvis Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[11]));
  CreateTestFormField(
      "Card Number", "cardnumber", "1234567890123456", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[12]));
  CreateTestFormField("Expiration Date", "ccmonth", "04", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[13]));
  CreateTestFormField("", "ccyear", "2012", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[14]));
}

TEST_F(AutoFillManagerTest, FormChangesRemoveField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // Now, after the call to |FormsSeen| we remove the phone number field before
  // filling.
  form.fields.erase(form.fields.begin() + 3);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(
      autofill_manager_->FillAutoFillFormData(kPageID,
                                              form,
                                              ASCIIToUTF16("Elvis"),
                                              ASCIIToUTF16("Home")));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  ASSERT_EQ(4U, results.fields.size());

  CreateTestFormField("First Name", "firstname", "Elvis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  CreateTestFormField("Middle Name", "middlename", "Aaron", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  CreateTestFormField("Last Name", "lastname", "Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  CreateTestFormField(
      "Email", "email", "theking@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
}

TEST_F(AutoFillManagerTest, FormChangesAddField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  // Note: absent phone number.  Adding this below.
  form.fields.push_back(field);
  CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // Now, after the call to |FormsSeen| we add the phone number field before
  // filling.
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(
      autofill_manager_->FillAutoFillFormData(kPageID,
                                              form,
                                              ASCIIToUTF16("Elvis"),
                                              ASCIIToUTF16("Home")));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  ASSERT_EQ(5U, results.fields.size());

  CreateTestFormField("First Name", "firstname", "Elvis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  CreateTestFormField("Middle Name", "middlename", "Aaron", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  CreateTestFormField("Last Name", "lastname", "Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  CreateTestFormField(
      "Email", "email", "theking@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
}

}  // namespace
