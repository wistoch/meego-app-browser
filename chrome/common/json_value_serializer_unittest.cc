// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/file_util.h"
#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(JSONValueSerializerTest, Roundtrip) {
  const std::string original_serialization =
    "{\"bool\":true,\"int\":42,\"list\":[1,2],\"null\":null,\"real\":3.14}";
  Value* root = NULL;
  JSONStringValueSerializer serializer(original_serialization);
  ASSERT_TRUE(serializer.Deserialize(&root));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root);

  Value* null_value = NULL;
  ASSERT_TRUE(root_dict->Get(L"null", &null_value));
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->IsType(Value::TYPE_NULL));

  bool bool_value = false;
  ASSERT_TRUE(root_dict->GetBoolean(L"bool", &bool_value));
  ASSERT_TRUE(bool_value);

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger(L"int", &int_value));
  ASSERT_EQ(42, int_value);

  double real_value = 0.0;
  ASSERT_TRUE(root_dict->GetReal(L"real", &real_value));
  ASSERT_DOUBLE_EQ(3.14, real_value);

  // We shouldn't be able to write using this serializer, since it was
  // initialized with a const string.
  ASSERT_FALSE(serializer.Serialize(*root_dict));

  std::string test_serialization = "";
  JSONStringValueSerializer mutable_serializer(&test_serialization);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  ASSERT_EQ(original_serialization, test_serialization);

  mutable_serializer.set_pretty_print(true);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  const std::string pretty_serialization =
    "{\r\n"
    "   \"bool\": true,\r\n"
    "   \"int\": 42,\r\n"
    "   \"list\": [ 1, 2 ],\r\n"
    "   \"null\": null,\r\n"
    "   \"real\": 3.14\r\n"
    "}\r\n";
  ASSERT_EQ(pretty_serialization, test_serialization);

  delete root;
}

TEST(JSONValueSerializerTest, StringEscape) {
  std::wstring all_chars;
  for (int i = 1; i < 256; ++i) {
    all_chars += static_cast<wchar_t>(i);
  }
  // Generated in in Firefox using the following js (with an extra backslash for
  // double quote):
  // var s = '';
  // for (var i = 1; i < 256; ++i) { s += String.fromCharCode(i); }
  // uneval(s).replace(/\\/g, "\\\\");
  std::string all_chars_expected =
      "\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\b\\t\\n\\v\\f\\r\\x0E\\x0F\\x10"
      "\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\x1C\\x1D\\x1E"
      "\\x1F !\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\"
      "\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7F\\x80\\x81\\x82\\x83\\x84\\x85"
      "\\x86\\x87\\x88\\x89\\x8A\\x8B\\x8C\\x8D\\x8E\\x8F\\x90\\x91\\x92\\x93"
      "\\x94\\x95\\x96\\x97\\x98\\x99\\x9A\\x9B\\x9C\\x9D\\x9E\\x9F\\xA0\\xA1"
      "\\xA2\\xA3\\xA4\\xA5\\xA6\\xA7\\xA8\\xA9\\xAA\\xAB\\xAC\\xAD\\xAE\\xAF"
      "\\xB0\\xB1\\xB2\\xB3\\xB4\\xB5\\xB6\\xB7\\xB8\\xB9\\xBA\\xBB\\xBC\\xBD"
      "\\xBE\\xBF\\xC0\\xC1\\xC2\\xC3\\xC4\\xC5\\xC6\\xC7\\xC8\\xC9\\xCA\\xCB"
      "\\xCC\\xCD\\xCE\\xCF\\xD0\\xD1\\xD2\\xD3\\xD4\\xD5\\xD6\\xD7\\xD8\\xD9"
      "\\xDA\\xDB\\xDC\\xDD\\xDE\\xDF\\xE0\\xE1\\xE2\\xE3\\xE4\\xE5\\xE6\\xE7"
      "\\xE8\\xE9\\xEA\\xEB\\xEC\\xED\\xEE\\xEF\\xF0\\xF1\\xF2\\xF3\\xF4\\xF5"
      "\\xF6\\xF7\\xF8\\xF9\\xFA\\xFB\\xFC\\xFD\\xFE\\xFF";

  std::string expected_output = "{\"all_chars\":\"" + all_chars_expected +
                                 "\"}";
  // Test JSONWriter interface
  std::string output_js;
  DictionaryValue valueRoot;
  valueRoot.SetString(L"all_chars", all_chars);
  JSONWriter::Write(&valueRoot, false, &output_js);
  ASSERT_EQ(expected_output, output_js);

  // Test JSONValueSerializer interface (uses JSONWriter).
  JSONStringValueSerializer serializer(&output_js);
  ASSERT_TRUE(serializer.Serialize(valueRoot));
  ASSERT_EQ(expected_output, output_js);
}

TEST(JSONValueSerializerTest, UnicodeStrings) {
  // unicode string json -> escaped ascii text
  DictionaryValue root;
  std::wstring test(L"\x7F51\x9875");
  root.SetString(L"web", test);

  std::string expected = "{\"web\":\"\\u7F51\\u9875\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(expected, actual);

  // escaped ascii text -> json
  Value* deserial_root = NULL;
  JSONStringValueSerializer deserializer(expected);
  ASSERT_TRUE(deserializer.Deserialize(&deserial_root));
  DictionaryValue* dict_root = static_cast<DictionaryValue*>(deserial_root);
  std::wstring web_value;
  ASSERT_TRUE(dict_root->GetString(L"web", &web_value));
  ASSERT_EQ(test, web_value);
  delete deserial_root;
}

TEST(JSONValueSerializerTest, HexStrings) {
  // hex string json -> escaped ascii text
  DictionaryValue root;
  std::wstring test(L"\x01\x02");
  root.SetString(L"test", test);

  std::string expected = "{\"test\":\"\\x01\\x02\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(expected, actual);

  // escaped ascii text -> json
  Value* deserial_root = NULL;
  JSONStringValueSerializer deserializer(expected);
  ASSERT_TRUE(deserializer.Deserialize(&deserial_root));
  DictionaryValue* dict_root = static_cast<DictionaryValue*>(deserial_root);
  std::wstring test_value;
  ASSERT_TRUE(dict_root->GetString(L"test", &test_value));
  ASSERT_EQ(test, test_value);
  delete deserial_root;

  // Test converting escaped regular chars
  deserial_root = NULL;
  std::string escaped_chars = "{\"test\":\"\\x67\\x6f\"}";
  JSONStringValueSerializer deserializer2(escaped_chars);
  ASSERT_TRUE(deserializer2.Deserialize(&deserial_root));
  dict_root = static_cast<DictionaryValue*>(deserial_root);
  ASSERT_TRUE(dict_root->GetString(L"test", &test_value));
  ASSERT_EQ(std::wstring(L"go"), test_value);
  delete deserial_root;
}

namespace {

void ValidateJsonList(const std::string& json) {
  Value* root = NULL;
  ASSERT_TRUE(JSONReader::Read(json, &root));
  ASSERT_TRUE(root && root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root);
  ASSERT_EQ(1, list->GetSize());
  Value* elt = NULL;
  ASSERT_TRUE(list->Get(0, &elt));
  int value = 0;
  ASSERT_TRUE(elt && elt->GetAsInteger(&value));
  ASSERT_EQ(1, value);
  delete root;
}

}  // namespace

TEST(JSONValueSerializerTest, JSONReaderComments) {
  ValidateJsonList("[ // 2, 3, ignore me ] \n1 ]");
  ValidateJsonList("[ /* 2, \n3, ignore me ]*/ \n1 ]");
  ValidateJsonList("//header\n[ // 2, \n// 3, \n1 ]// footer");
  ValidateJsonList("/*\n[ // 2, \n// 3, \n1 ]*/[1]");
  ValidateJsonList("[ 1 /* one */ ] /* end */");
  ValidateJsonList("[ 1 //// ,2\r\n ]");

  Value* root = NULL;
  // It's ok to have a comment in a string.
  ASSERT_TRUE(JSONReader::Read("[\"// ok\\n /* foo */ \"]", &root));
  ASSERT_TRUE(root && root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root);
  ASSERT_EQ(1, list->GetSize());
  Value* elt = NULL;
  ASSERT_TRUE(list->Get(0, &elt));
  std::wstring value;
  ASSERT_TRUE(elt && elt->GetAsString(&value));
  ASSERT_EQ(L"// ok\n /* foo */ ", value);
  delete root;

  root = NULL;
  // You can't nest comments.
  ASSERT_FALSE(JSONReader::Read("/* /* inner */ outer */ [ 1 ]", &root));

  // Not a open comment token.
  ASSERT_FALSE(JSONReader::Read("/ * * / [1]", &root));
}

namespace {
  class JSONFileValueSerializerTest : public testing::Test {
  protected:
    virtual void SetUp() {
      // Name a subdirectory of the temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"JSONFileValueSerializerTest");

      // Create a fresh, empty copy of this directory.
      file_util::Delete(test_dir_, true);
      CreateDirectory(test_dir_.c_str(), NULL);
    }
    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    std::wstring test_dir_;
  };
}  // anonymous namespace

TEST_F(JSONFileValueSerializerTest, Roundtrip) {
  std::wstring original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  file_util::AppendToPath(&original_file_path, L"serializer_test.js");

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  Value* root;
  ASSERT_TRUE(deserializer.Deserialize(&root));

  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root);

  Value* null_value = NULL;
  ASSERT_TRUE(root_dict->Get(L"null", &null_value));
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->IsType(Value::TYPE_NULL));

  bool bool_value = false;
  ASSERT_TRUE(root_dict->GetBoolean(L"bool", &bool_value));
  ASSERT_TRUE(bool_value);

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger(L"int", &int_value));
  ASSERT_EQ(42, int_value);

  std::wstring string_value;
  ASSERT_TRUE(root_dict->GetString(L"string", &string_value));
  ASSERT_EQ(L"hello", string_value);

  // Now try writing.
  std::wstring written_file_path = test_dir_;
  file_util::AppendToPath(&written_file_path, L"test_output.js");

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::ContentsEqual(original_file_path, written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));

  delete root;
}

TEST_F(JSONFileValueSerializerTest, RoundtripNested) {
  std::wstring original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  file_util::AppendToPath(&original_file_path, L"serializer_nested_test.js");

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  Value* root;
  ASSERT_TRUE(deserializer.Deserialize(&root));

  // Now try writing.
  std::wstring written_file_path = test_dir_;
  file_util::AppendToPath(&written_file_path, L"test_output.js");

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::ContentsEqual(original_file_path, written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));

  delete root;
}

TEST_F(JSONFileValueSerializerTest, NoWhitespace) {
  std::wstring source_file_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_file_path));
  file_util::AppendToPath(&source_file_path,
                          L"serializer_test_nowhitespace.js");
  ASSERT_TRUE(file_util::PathExists(source_file_path));
  JSONFileValueSerializer serializer(source_file_path);
  Value* root;
  ASSERT_TRUE(serializer.Deserialize(&root));
  ASSERT_TRUE(root);
  delete root;
}
