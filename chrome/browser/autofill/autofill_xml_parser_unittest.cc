// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/files/talk/xmllite/xmlparser.h"

namespace {

TEST(AutoFillQueryXmlParserTest, BasicQuery) {
  // An XML string representing a basic query response.
  std::string xml = "<autofillqueryresponse>"
                    "<field autofilltype=\"0\" />"
                    "<field autofilltype=\"1\" />"
                    "<field autofilltype=\"3\" />"
                    "<field autofilltype=\"2\" />"
                    "</autofillqueryresponse>";

  // Create a vector of AutoFillFieldTypes, to assign the parsed field types to.
  std::vector<AutoFillFieldType> field_types;
  UploadRequired upload_required = USE_UPLOAD_RATES;

  // Create a parser.
  AutoFillQueryXmlParser parse_handler(&field_types, &upload_required);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler.succeeded());
  EXPECT_EQ(upload_required, USE_UPLOAD_RATES);
  ASSERT_EQ(4U, field_types.size());
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);
  EXPECT_EQ(UNKNOWN_TYPE, field_types[1]);
  EXPECT_EQ(NAME_FIRST, field_types[2]);
  EXPECT_EQ(EMPTY_TYPE, field_types[3]);
}

// Test parsing the upload required attribute.
TEST(AutoFillQueryXmlParserTest, TestUploadRequired) {
  std::vector<AutoFillFieldType> field_types;
  UploadRequired upload_required = USE_UPLOAD_RATES;

  std::string xml = "<autofillqueryresponse uploadrequired=\"true\">"
                    "<field autofilltype=\"0\" />"
                    "</autofillqueryresponse>";

  scoped_ptr<AutoFillQueryXmlParser> parse_handler(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(UPLOAD_REQUIRED, upload_required);
  ASSERT_EQ(1U, field_types.size());
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);

  field_types.clear();
  xml = "<autofillqueryresponse uploadrequired=\"false\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(upload_required, UPLOAD_NOT_REQUIRED);
  ASSERT_EQ(1U, field_types.size());
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);

  field_types.clear();
  xml = "<autofillqueryresponse uploadrequired=\"bad_value\">"
        "<field autofilltype=\"0\" />"
        "</autofillqueryresponse>";

  parse_handler.reset(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(upload_required, USE_UPLOAD_RATES);
  ASSERT_EQ(1U, field_types.size());
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);
}

// Test badly formed XML queries.
TEST(AutoFillQueryXmlParserTest, ParseErrors) {
  std::vector<AutoFillFieldType> field_types;
  UploadRequired upload_required = USE_UPLOAD_RATES;

  // Test no AutoFill type.
  std::string xml = "<autofillqueryresponse>"
                    "<field/>"
                    "</autofillqueryresponse>";

  scoped_ptr<AutoFillQueryXmlParser> parse_handler(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  scoped_ptr<buzz::XmlParser> parser(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_FALSE(parse_handler->succeeded());
  EXPECT_EQ(upload_required, USE_UPLOAD_RATES);
  ASSERT_EQ(0U, field_types.size());

  // Test an incorrect AutoFill type.
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"307\"/>"
        "</autofillqueryresponse>";

  parse_handler.reset(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_TRUE(parse_handler->succeeded());
  EXPECT_EQ(upload_required, USE_UPLOAD_RATES);
  ASSERT_EQ(1U, field_types.size());
  // AutoFillType was out of range and should be set to NO_SERVER_DATA.
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);

  // Test an incorrect AutoFill type.
  field_types.clear();
  xml = "<autofillqueryresponse>"
        "<field autofilltype=\"No Type\"/>"
        "</autofillqueryresponse>";

  // Parse fails but an entry is still added to field_types.
  parse_handler.reset(
      new AutoFillQueryXmlParser(&field_types, &upload_required));
  parser.reset(new buzz::XmlParser(parse_handler.get()));
  parser->Parse(xml.c_str(), xml.length(), true);
  EXPECT_FALSE(parse_handler->succeeded());
  EXPECT_EQ(upload_required, USE_UPLOAD_RATES);
  ASSERT_EQ(1U, field_types.size());
  EXPECT_EQ(NO_SERVER_DATA, field_types[0]);
}

}  // namespace
