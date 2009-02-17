// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionTest : public testing::Test {
};

TEST(ExtensionTest, InitFromValueInvalid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;

  // Start with a valid extension manifest
  std::wstring extensions_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_dir));
  FilePath extensions_path = FilePath::FromWStringHack(extensions_dir)
      .AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("extension1")
      .AppendASCII("1")
      .AppendASCII(Extension::kManifestFilename);

  JSONFileValueSerializer serializer(extensions_path.ToWStringHack());
  scoped_ptr<DictionaryValue> valid_value(
      static_cast<DictionaryValue*>(serializer.Deserialize(&error)));
  ASSERT_TRUE(valid_value.get());
  ASSERT_EQ("", error);
  ASSERT_TRUE(extension.InitFromValue(*valid_value, &error));
  ASSERT_EQ("", error);

  scoped_ptr<DictionaryValue> input_value;

  // Test missing and invalid format versions
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kFormatVersionKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidFormatVersionError, error);

  input_value->SetString(Extension::kFormatVersionKey, "foo");
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidFormatVersionError, error);

  input_value->SetInteger(Extension::kFormatVersionKey, 2);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidFormatVersionError, error);

  // Test missing and invalid ids
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kIdKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidIdError, error);

  input_value->SetInteger(Extension::kIdKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidIdError, error);

  // Test missing and invalid versions
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kVersionKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);

  input_value->SetInteger(Extension::kVersionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);

  // Test missing and invalid names
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kNameKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);

  input_value->SetInteger(Extension::kNameKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);

  // Test invalid description
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(Extension::kDescriptionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidDescriptionError, error);

  // Test invalid user scripts list
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(Extension::kUserScriptsKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_EQ(Extension::kInvalidUserScriptsListError, error);

  // Test invalid user script item
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  ListValue* user_scripts = NULL;
  input_value->GetList(Extension::kUserScriptsKey, &user_scripts);
  ASSERT_FALSE(NULL == user_scripts);
  user_scripts->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidUserScriptError));

  // Test missing and invalid matches array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kUserScriptsKey, &user_scripts);
  DictionaryValue* user_script = NULL;
  user_scripts->GetDictionary(0, &user_script);
  user_script->Remove(Extension::kMatchesKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchesError));

  user_script->Set(Extension::kMatchesKey, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchesError));

  ListValue* matches = new ListValue;
  user_script->Set(Extension::kMatchesKey, matches);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchCountError));

  // Test invalid match element
  matches->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchError));

  // Test missing and invalid files array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kUserScriptsKey, &user_scripts);
  user_scripts->GetDictionary(0, &user_script);
  user_script->Remove(Extension::kFilesKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidFilesError));

  user_script->Set(Extension::kFilesKey, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidFilesError));

  ListValue* files = new ListValue;
  user_script->Set(Extension::kFilesKey, files);
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidFileCountError));

  // Test invalid file element
  files->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidFileError));

  // Test too many file elements (more than one not yet supported)
  files->Set(0, Value::CreateStringValue("foo.js"));
  files->Set(1, Value::CreateStringValue("bar.js"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidFileCountError));
}

TEST(ExtensionTest, InitFromValueValid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;
  DictionaryValue input_value;

  // Test minimal extension
  input_value.SetInteger(Extension::kFormatVersionKey, 1);
  input_value.SetString(Extension::kIdKey, "com.google.myextension");
  input_value.SetString(Extension::kVersionKey, "1.0.0.0");
  input_value.SetString(Extension::kNameKey, "my extension");

  EXPECT_TRUE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ("", error);
  EXPECT_EQ("com.google.myextension", extension.id());
  EXPECT_EQ("1.0.0.0", extension.VersionString());
  EXPECT_EQ("my extension", extension.name());
  EXPECT_EQ("chrome-extension://com.google.myextension/",
            extension.url().spec());
  EXPECT_EQ(path.value(), extension.path().value());
}

TEST(ExtensionTest, GetResourceURLAndPath) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  DictionaryValue input_value;
  input_value.SetInteger(Extension::kFormatVersionKey, 1);
  input_value.SetString(Extension::kIdKey, "com.google.myextension");
  input_value.SetString(Extension::kVersionKey, "1.0.0.0");
  input_value.SetString(Extension::kNameKey, "my extension");
  EXPECT_TRUE(extension.InitFromValue(input_value, NULL));

  EXPECT_EQ(extension.url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension.url(), "bar/baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "bar/../baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "../baz.js").spec());

  EXPECT_EQ(path.Append(FILE_PATH_LITERAL("bar"))
                .Append(FILE_PATH_LITERAL("baz.js")).value(),
            Extension::GetResourcePath(extension.path(), "bar/baz.js").value());
  EXPECT_EQ(path.Append(FILE_PATH_LITERAL("baz.js")).value(),
            Extension::GetResourcePath(extension.path(), "bar/../baz.js")
                .value());
  EXPECT_EQ(FilePath().value(),
            Extension::GetResourcePath(extension.path(), "../baz.js").value());
}
