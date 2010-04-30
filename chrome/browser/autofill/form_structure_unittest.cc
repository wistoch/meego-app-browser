// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/form_structure.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using WebKit::WebInputElement;

namespace {

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << UTF16ToUTF8(form.name)
     << " "
     << UTF16ToUTF8(form.method)
     << " "
     << form.origin.spec()
     << " "
     << form.action.spec()
     << " ";

  for (std::vector<webkit_glue::FormField>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    os << *iter
       << " ";
  }

  return os;
}

TEST(FormStructureTest, FieldCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  FormStructure form_structure(form);

  // All fields are counted.
  EXPECT_EQ(3U, form_structure.field_count());
}

TEST(FormStructureTest, AutoFillCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("state"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("select-one")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  FormStructure form_structure(form);

  // Only text and select fields are counted.
  EXPECT_EQ(2U, form_structure.autofill_count());
}

TEST(FormStructureTest, ConvertToFormData) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("state"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("select")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  FormStructure form_structure(form);

  FormData converted = form_structure.ConvertToFormData();
  EXPECT_EQ(form, converted);
}

TEST(FormStructureTest, HasAutoFillableValues) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  // Non text/select fields are not used when saving auto fill data.
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit1"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit2"),
                                               ASCIIToUTF16("dummy value"),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->HasAutoFillableValues());

  // Empty text/select fields are also not used when saving auto fill data.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("state"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("select-one")));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->HasAutoFillableValues());

  // Non-empty fields can be saved in auto fill profile.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               ASCIIToUTF16("John"),
                                               ASCIIToUTF16("text")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->HasAutoFillableValues());
}

TEST(FormStructureTest, IsAutoFillable) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be auto-fillable.
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // We now have three text fields.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // The method must be 'post'.
  form.method = ASCIIToUTF16("get");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // The target cannot include http(s)://*/search...
  form.method = ASCIIToUTF16("post");
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());
}

TEST(FormStructureTest, HeuristicsContactInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                               ASCIIToUTF16("zipcode"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9UL, form_structure->field_count());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(9U, form_structure->field_count());

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsSample8) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your First Name:"),
                             ASCIIToUTF16("bill.first"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your Last Name:"),
                             ASCIIToUTF16("bill.last"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 1:"),
                             ASCIIToUTF16("bill.street1"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 2:"),
                             ASCIIToUTF16("bill.street2"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City:"),
                             ASCIIToUTF16("bill.city"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("State (U.S.):"),
                             ASCIIToUTF16("bill.state"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Zip/Postal Code:"),
                             ASCIIToUTF16("BillTo.PostalCode"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Country:"),
                             ASCIIToUTF16("bill.country"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Phone Number:"),
                             ASCIIToUTF16("BillTo.Phone"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("Submit"),
                             string16(),
                             ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(10U, form_structure->field_count());

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Country.
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsSample6) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("E-mail address"),
                             ASCIIToUTF16("email"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Full name"),
                             ASCIIToUTF16("name"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Company"),
                             ASCIIToUTF16("company"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address"),
                             ASCIIToUTF16("address"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City"),
                             ASCIIToUTF16("city"),
                             string16(),
                             ASCIIToUTF16("text")));
  // TODO(jhawkins): Add state select control.
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Zip Code"),
                             ASCIIToUTF16("Home.PostalCode"),
                             string16(),
                             ASCIIToUTF16("text")));
  // TODO(jhawkins): Phone number.
  form.fields.push_back(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("Submit"),
                             ASCIIToUTF16("continue"),
                             ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(7U, form_structure->field_count());

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(0)->heuristic_type());
  // Full name.
  EXPECT_EQ(NAME_FULL, form_structure->field(1)->heuristic_type());
  // Company
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

// Tests a sequence of FormFields where only labels are supplied to heuristics
// for matching.  This works because FormField labels are matched in the case
// that input element ids (or |name| fields) are missing.
TEST(FormStructureTest, HeuristicsLabelsOnly) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9UL, form_structure->field_count());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(9U, form_structure->field_count());

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsCreditCardInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name on card"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                               ASCIIToUTF16("verification"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Expect the correct number of fields.
  ASSERT_EQ(6UL, form_structure->field_count());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(6U, form_structure->field_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(1)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(2)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(3)->heuristic_type());
  // Credit card cvc.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(4)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsCreditCardInfoWithUnknownCardField) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name on card"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Type"),
                                               ASCIIToUTF16("card_type"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                               ASCIIToUTF16("verification"),
                                               string16(),
                                               ASCIIToUTF16("text")));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Expect the correct number of fields.
  ASSERT_EQ(7UL, form_structure->field_count());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(7UL, form_structure->field_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Credit card type.  This is an unknown type but related to the credit card.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // Credit card cvc.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

TEST(FormStructureTest, ThreeAddressLines) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line2"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text")));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line3"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text")));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();
  ASSERT_EQ(3U, form_structure->field_count());

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
}

}  // namespace
