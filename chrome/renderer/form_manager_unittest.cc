// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/form_manager.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/form_data.h"

using WebKit::WebElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebString;
using WebKit::WebVector;

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

typedef RenderViewTest FormManagerTest;

TEST_F(FormManagerTest, WebFormElementToFormData) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    true,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, ExtractForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, ExtractMultipleForms) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>"
           "<FORM name=\"TestForm2\" action=\"http://zoo.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"second\" value=\"Submit\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[1]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(ASCIIToUTF16("TestForm2"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(2U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("second"),
                      ASCIIToUTF16("Submit"),
                      ASCIIToUTF16("submit"),
                      0),
            fields2[1]);
}

TEST_F(FormManagerTest, GetFormsAutocomplete) {
  // Form is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\""
           " autocomplete=off>"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we did load the forms.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // autocomplete=off and we're requiring autocomplete, so no forms returned.
  forms.clear();
  form_manager.GetForms(FormManager::REQUIRE_AUTOCOMPLETE, &forms);
  ASSERT_EQ(0U, forms.size());

  // The firstname element is not auto-completable due to autocomplete=off.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://abc.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\""
           "   autocomplete=off>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply\" value=\"Send\"/>"
           "</FORM>");

  web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  form_manager.Reset();
  form_manager.ExtractForms(web_frame);

  forms.clear();
  form_manager.GetForms(FormManager::REQUIRE_AUTOCOMPLETE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[1]);
}

TEST_F(FormManagerTest, GetFormsElementsEnabled) {
  // The firstname element is not enabled due to disabled being set.
  LoadHTML("<FORM name=\"TestForm\" action=\"http://xyz.com\" method=\"post\">"
           "  <INPUT disabled type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_ELEMENTS_ENABLED, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://xyz.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("submit"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[1]);
}

TEST_F(FormManagerTest, FindForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form and verify it's the correct form.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, FillForm) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Wyatt"),
                      ASCIIToUTF16("text"),
                      20),
            fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Earp"),
                      ASCIIToUTF16("text"),
                      20),
            fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields2[2]);
}

TEST_F(FormManagerTest, Reset) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // There should be no forms after the call to Reset.
  form_manager.Reset();

  forms.clear();
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(0U, forms.size());
}

TEST_F(FormManagerTest, Labels) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <LABEL for=\"firstname\"> First name: </LABEL>"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\"> Last name: </LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsWithSpans) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <LABEL for=\"firstname\"><span>First name: </span></LABEL>"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\"><span>Last name: </span></LABEL>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

// This test is different from FormManagerTest.Labels in that the label elements
// for= attribute is set to the name of the form control element it is a label
// for instead of the id of the form control element.  This is invalid because
// the for= attribute must be set to the id of the form control element.
TEST_F(FormManagerTest, InvalidLabels) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <LABEL for=\"firstname\"> First name: </LABEL>"
           "    <INPUT type=\"text\" name=\"firstname\" value=\"John\"/>"
           "  <LABEL for=\"lastname\"> Last name: </LABEL>"
           "    <INPUT type=\"text\" name=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

// This test has three form control elements, only one of which has a label
// element associated with it.  The first element is disabled because of the
// autocomplete=off attribute.
TEST_F(FormManagerTest, OneLabelElementFirstControlElementDisabled) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  First name:"
           "    <INPUT type=\"text\" id=\"firstname\" autocomplete=\"off\"/>"
           "  <LABEL for=\"middlename\">Middle name: </LABEL>"
           "    <INPUT type=\"text\" id=\"middlename\"/>"
           "  Last name:"
           "    <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_AUTOCOMPLETE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Middle name:"),
                      ASCIIToUTF16("middlename"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromText) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  First name:"
           "    <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "  Last name:"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromParagraph) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  <P>First name:</P><INPUT type=\"text\" "
           "                           id=\"firstname\" value=\"John\"/>"
           "  <P>Last name:</P>"
           "    <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromTableCell) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "<TABLE>"
           "  <TR>"
           "    <TD>First name:</TD>"
           "    <TD><INPUT type=\"text\" id=\"firstname\" value=\"John\"/></TD>"
           "  </TR>"
           "  <TR>"
           "    <TD>Last name:</TD>"
           "    <TD><INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/></TD>"
           "  </TR>"
           "  <TR>"
           "    <TD></TD>"
           "    <TD>"
           "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "    </TD>"
           "  </TR>"
           "</TABLE>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromTableCellNested) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "<TABLE>"
           "  <TR>"
           "    <TD>"
           "      <FONT>"
           "        First name:"
           "      </FONT>"
           "      <FONT>"
           "        Bogus"
           "      </FONT>"
           "    </TD>"
           "    <TD>"
           "      <FONT>"
           "        <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "      </FONT>"
           "    </TD>"
           "  </TR>"
           "  <TR>"
           "    <TD>"
           "      <FONT>"
           "        Last name:"
           "      </FONT>"
           "    </TD>"
           "    <TD>"
           "      <FONT>"
           "        <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "      </FONT>"
           "    </TD>"
           "  </TR>"
           "  <TR>"
           "    <TD></TD>"
           "    <TD>"
           "      <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "    </TD>"
           "  </TR>"
           "</TABLE>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("First name:Bogus"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredFromDefinitionList) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "<DL>"
           "  <DT>"
           "    <SPAN>"
           "      *"
           "    </SPAN>"
           "    <SPAN>"
           "      First name:"
           "    </SPAN>"
           "    <SPAN>"
           "      Bogus"
           "    </SPAN>"
           "  </DT>"
           "  <DD>"
           "    <FONT>"
           "      <INPUT type=\"text\" id=\"firstname\" value=\"John\"/>"
           "    </FONT>"
           "  </DD>"
           "  <DT>"
           "    <SPAN>"
           "      Last name:"
           "    </SPAN>"
           "  </DT>"
           "  <DD>"
           "    <FONT>"
           "      <INPUT type=\"text\" id=\"lastname\" value=\"Smith\"/>"
           "    </FONT>"
           "  </DD>"
           "  <DT></DT>"
           "  <DD>"
           "    <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "  </DD>"
           "</DL>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("*First name:Bogus"),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("John"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Last name:"),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Smith"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredWithSameName) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Address Line 1:"
           "    <INPUT type=\"text\" name=\"Address\"/>"
           "  Address Line 2:"
           "    <INPUT type=\"text\" name=\"Address\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  const FormData& form = forms[0];
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Address Line 1:"),
                      ASCIIToUTF16("Address"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("Address Line 2:"),
                      ASCIIToUTF16("Address"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);
}

TEST_F(FormManagerTest, LabelsInferredWithImageTags) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Phone:"
           "  <input type=\"text\" name=\"dayphone1\">"
           "  <img/>"
           "  -"
           "  <img/>"
           "  <input type=\"text\" name=\"dayphone2\">"
           "  <img/>"
           "  -"
           "  <img/>"
           "  <input type=\"text\" name=\"dayphone3\">"
           "  ext.:"
           "  <input type=\"text\" name=\"dayphone4\">"
           "  <input type=\"text\" name=\"dummy\">"
           "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");

  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    true,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(6U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Phone:"),
                      ASCIIToUTF16("dayphone1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone2"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone3"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(FormField(ASCIIToUTF16("ext.:"),
                      ASCIIToUTF16("dayphone4"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[3]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("dummy"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[4]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[5]);
}

TEST_F(FormManagerTest, FillFormMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"5\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"5\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Brother"));
  form.fields[1].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields2 = form2.fields;
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Broth"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[0]);
  EXPECT_EQ(ASCIIToUTF16("Broth"), fields2[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonat"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[1]);
  EXPECT_EQ(ASCIIToUTF16("Jonat"), fields2[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
                      fields2[2]);
}

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_F(FormManagerTest, FillFormNegativeMaxLength) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\" maxlength=\"-1\"/>"
           "  <INPUT type=\"text\" id=\"lastname\" maxlength=\"-10\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Brother"));
  form.fields[1].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Brother"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[0]);
  EXPECT_EQ(ASCIIToUTF16("Brother"), fields2[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonathan"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[1]);
  EXPECT_EQ(ASCIIToUTF16("Jonathan"), fields2[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
                      fields2[2]);
}

// This test sends a FormData object to FillForm with more fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormMoreFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  prefix
  //  firstname
  //  hidden
  //  middlename
  //  second
  //  lastname
  //  postfix
  FormData* form = &forms[0];

  FormField field1(string16(),
                   ASCIIToUTF16("prefix"),
                   string16(),
                   ASCIIToUTF16("text"),
                   20);
  form->fields.insert(form->fields.begin(), field1);

  FormField field2(string16(),
                   ASCIIToUTF16("hidden"),
                   string16(),
                   ASCIIToUTF16("text"),
                   20);
  form->fields.insert(form->fields.begin() + 2, field2);

  FormField field3(string16(),
                   ASCIIToUTF16("second"),
                   string16(),
                   ASCIIToUTF16("text"),
                   20);
  form->fields.insert(form->fields.begin() + 4, field3);

  FormField field4(string16(),
                   ASCIIToUTF16("postfix"),
                   string16(),
                   ASCIIToUTF16("text"),
                   20);
  form->fields.insert(form->fields.begin() + 6, field4);

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Alpha"));
  form->fields[1].set_value(ASCIIToUTF16("Brother"));
  form->fields[2].set_value(ASCIIToUTF16("Abracadabra"));
  form->fields[3].set_value(ASCIIToUTF16("Joseph"));
  form->fields[4].set_value(ASCIIToUTF16("Beta"));
  form->fields[5].set_value(ASCIIToUTF16("Jonathan"));
  form->fields[6].set_value(ASCIIToUTF16("Omega"));
  EXPECT_TRUE(form_manager.FillForm(*form));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(4U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Brother"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(ASCIIToUTF16("Brother"), fields[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Joseph"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(ASCIIToUTF16("Joseph"), fields[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonathan"),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(ASCIIToUTF16("Jonathan"), fields[2].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[3]);
}

// This test sends a FormData object to FillForm with fewer fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormFewerFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"prefix\"/>"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"hidden\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"second\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"postfix\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];
  form->fields.erase(form->fields.begin());
  form->fields.erase(form->fields.begin() + 1);
  form->fields.erase(form->fields.begin() + 2);
  form->fields.erase(form->fields.begin() + 3);

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(*form));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(8U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("prefix"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(string16(), fields[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Brother"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(ASCIIToUTF16("Brother"), fields[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("hidden"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(string16(), fields[2].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Joseph"),
                      ASCIIToUTF16("text"),
                      20),
            fields[3]);
  EXPECT_EQ(ASCIIToUTF16("Joseph"), fields[3].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("second"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[4]);
  EXPECT_EQ(string16(), fields[4].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonathan"),
                      ASCIIToUTF16("text"),
                      20),
            fields[5]);
  EXPECT_EQ(ASCIIToUTF16("Jonathan"), fields[5].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("postfix"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[6]);
  EXPECT_EQ(string16(), fields[6].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[7]);
}

// This test sends a FormData object to FillForm with a field changed from
// those in the cached WebFormElement.  In this case, we only fill out the
// fields that match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormChangedFormDataFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));

  // Alter the label and name used for matching.
  form->fields[1].set_label(ASCIIToUTF16("bogus"));
  form->fields[1].set_name(ASCIIToUTF16("bogus"));

  EXPECT_TRUE(form_manager.FillForm(*form));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(4U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Brother"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(ASCIIToUTF16("Brother"), fields[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Joseph"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(string16(), fields[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonathan"),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(ASCIIToUTF16("Jonathan"), fields[2].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[3]);
}

// This test sends a FormData object to FillForm with fewer fields than are in
// the cached WebFormElement.  In this case, we only fill out the fields that
// match between the FormData object and the WebFormElement.
TEST_F(FormManagerTest, FillFormExtraFieldInCache) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"text\" id=\"postfix\"/>"
           "  <INPUT type=\"submit\" name=\"reply-send\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // After the field modification, the fields in |form| will look like:
  //  firstname
  //  middlename
  //  lastname
  FormData* form = &forms[0];
  form->fields.pop_back();

  // Fill the form.
  form->fields[0].set_value(ASCIIToUTF16("Brother"));
  form->fields[1].set_value(ASCIIToUTF16("Joseph"));
  form->fields[2].set_value(ASCIIToUTF16("Jonathan"));
  EXPECT_TRUE(form_manager.FillForm(*form));

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  // TODO(jhawkins): We don't actually compare the value of the field in
  // FormField::operator==()!
  const std::vector<FormField>& fields = form2.fields;
  ASSERT_EQ(5U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Brother"),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(ASCIIToUTF16("Brother"), fields[0].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("middlename"),
                      ASCIIToUTF16("Joseph"),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(ASCIIToUTF16("Joseph"), fields[1].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Jonathan"),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(ASCIIToUTF16("Jonathan"), fields[2].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("postfix"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[3]);
  EXPECT_EQ(string16(), fields[3].value());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[4]);
}

TEST_F(FormManagerTest, FillFormEmptyName) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("firstname");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://buh.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      string16(),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Wyatt"));
  form.fields[1].set_value(ASCIIToUTF16("Earp"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://buh.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("firstname"),
                      ASCIIToUTF16("Wyatt"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("lastname"),
                      ASCIIToUTF16("Earp"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      string16(),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
                      fields2[2]);
}

TEST_F(FormManagerTest, FillFormEmptyFormNames) {
  LoadHTML("<FORM action=\"http://buh.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"firstname\"/>"
           "  <INPUT type=\"text\" id=\"middlename\"/>"
           "  <INPUT type=\"text\" id=\"lastname\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>"
           "<FORM action=\"http://abc.com\" method=\"post\">"
           "  <INPUT type=\"text\" id=\"apple\"/>"
           "  <INPUT type=\"text\" id=\"banana\"/>"
           "  <INPUT type=\"submit\" value=\"Send\"/>"
           "</FORM>");

  WebFrame* web_frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), web_frame);

  FormManager form_manager;
  form_manager.ExtractForms(web_frame);

  // Verify that we have the form.
  std::vector<FormData> forms;
  form_manager.GetForms(FormManager::REQUIRE_NONE, &forms);
  ASSERT_EQ(2U, forms.size());

  // Get the input element we want to find.
  WebElement element = web_frame->document().getElementById("apple");
  WebInputElement input_element = element.to<WebInputElement>();

  // Find the form that contains the input element.
  FormData form;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form));
  EXPECT_EQ(string16(), form.name);
  EXPECT_EQ(GURL(web_frame->url()), form.origin);
  EXPECT_EQ(GURL("http://abc.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("apple"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("banana"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(string16(),
                      string16(),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[2]);

  // Fill the form.
  form.fields[0].set_value(ASCIIToUTF16("Red"));
  form.fields[1].set_value(ASCIIToUTF16("Yellow"));
  EXPECT_TRUE(form_manager.FillForm(form));

  // Find the newly-filled form that contains the input element.
  FormData form2;
  EXPECT_TRUE(form_manager.FindFormWithFormControlElement(
      input_element, FormManager::REQUIRE_NONE, &form2));
  EXPECT_EQ(string16(), form2.name);
  EXPECT_EQ(GURL(web_frame->url()), form2.origin);
  EXPECT_EQ(GURL("http://abc.com"), form2.action);

  const std::vector<FormField>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("apple"),
                      ASCIIToUTF16("Red"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[0]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("banana"),
                      ASCIIToUTF16("Yellow"),
                      ASCIIToUTF16("text"),
                      20),
                      fields2[1]);
  EXPECT_EQ(FormField(string16(),
                      string16(),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
                      fields2[2]);
}

TEST_F(FormManagerTest, ThreePartPhone) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Phone:"
           "  <input type=\"text\" name=\"dayphone1\">"
           "  -"
           "  <input type=\"text\" name=\"dayphone2\">"
           "  -"
           "  <input type=\"text\" name=\"dayphone3\">"
           "  ext.:"
           "  <input type=\"text\" name=\"dayphone4\">"
           "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");


  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    true,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(5U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Phone:"),
                      ASCIIToUTF16("dayphone1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone2"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[1]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone3"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[2]);
  EXPECT_EQ(FormField(ASCIIToUTF16("ext.:"),
                      ASCIIToUTF16("dayphone4"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[3]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[4]);
}


TEST_F(FormManagerTest, SizeFields) {
  LoadHTML("<FORM name=\"TestForm\" action=\"http://cnn.com\" method=\"post\">"
           "  Phone:"
           "  <input type=\"text\" size=\"3\" name=\"dayphone1\">"
           "  -"
           "  <input type=\"text\" size=\"3\" name=\"dayphone2\">"
           "  -"
           "  <input type=\"text\" size=\"4\" name=\"dayphone3\">"
           "  ext.:"
           "  <input type=\"text\" size=\"5\" name=\"dayphone4\">"
           "  <input type=\"text\" name=\"default1\">"
           "  <input type=\"text\" size=\"-1\" name=\"invalid1\">"
           "  <input type=\"submit\" name=\"reply-send\" value=\"Send\">"
           "</FORM>");


  WebFrame* frame = GetMainFrame();
  ASSERT_NE(static_cast<WebFrame*>(NULL), frame);

  WebVector<WebFormElement> forms;
  frame->forms(forms);
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(FormManager::WebFormElementToFormData(forms[0],
                                                    FormManager::REQUIRE_NONE,
                                                    true,
                                                    &form));
  EXPECT_EQ(ASCIIToUTF16("TestForm"), form.name);
  EXPECT_EQ(GURL(frame->url()), form.origin);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormField>& fields = form.fields;
  ASSERT_EQ(7U, fields.size());
  EXPECT_EQ(FormField(ASCIIToUTF16("Phone:"),
                      ASCIIToUTF16("dayphone1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      3),
            fields[0]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone2"),
                      string16(),
                      ASCIIToUTF16("text"),
                      3),
            fields[1]);
  EXPECT_EQ(FormField(ASCIIToUTF16("-"),
                      ASCIIToUTF16("dayphone3"),
                      string16(),
                      ASCIIToUTF16("text"),
                      4),
            fields[2]);
  EXPECT_EQ(FormField(ASCIIToUTF16("ext.:"),
                      ASCIIToUTF16("dayphone4"),
                      string16(),
                      ASCIIToUTF16("text"),
                      5),
            fields[3]);
  // When unspecified |size|, default is 20.
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("default1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      20),
            fields[4]);
  // When invalid |size| same value is returned.
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("invalid1"),
                      string16(),
                      ASCIIToUTF16("text"),
                      -1),
            fields[5]);
  EXPECT_EQ(FormField(string16(),
                      ASCIIToUTF16("reply-send"),
                      ASCIIToUTF16("Send"),
                      ASCIIToUTF16("submit"),
                      0),
            fields[6]);
}

}  // namespace
