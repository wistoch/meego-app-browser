// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <string>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(InputMethodUtilTest, FindLocalizedStringTest) {
  EXPECT_TRUE(StringIsSupported("Hiragana"));
  EXPECT_TRUE(StringIsSupported("Latin"));
  EXPECT_TRUE(StringIsSupported("Direct input"));
  EXPECT_FALSE(StringIsSupported("####THIS_STRING_IS_NOT_SUPPORTED####"));
}

TEST(InputMethodUtilTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("ja", NormalizeLanguageCode("ja"));
  EXPECT_EQ("ja", NormalizeLanguageCode("jpn"));
  EXPECT_EQ("t", NormalizeLanguageCode("t"));
  EXPECT_EQ("zh-CN", NormalizeLanguageCode("zh-CN"));
  EXPECT_EQ("zh-CN", NormalizeLanguageCode("zh_CN"));
  EXPECT_EQ("en-US", NormalizeLanguageCode("EN_us"));
  // See app/l10n_util.cc for es-419.
  EXPECT_EQ("es-419", NormalizeLanguageCode("es_419"));

  // Special three-letter language codes.
  EXPECT_EQ("cs", NormalizeLanguageCode("cze"));
  EXPECT_EQ("de", NormalizeLanguageCode("ger"));
  EXPECT_EQ("el", NormalizeLanguageCode("gre"));
  EXPECT_EQ("hr", NormalizeLanguageCode("scr"));
  EXPECT_EQ("ro", NormalizeLanguageCode("rum"));
  EXPECT_EQ("sk", NormalizeLanguageCode("slo"));
}

TEST(InputMethodUtilTest, IsKeyboardLayout) {
  EXPECT_TRUE(IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(IsKeyboardLayout("anthy"));
}

TEST(InputMethodUtilTest, GetLanguageCodeFromDescriptor) {
  EXPECT_EQ("ja", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("anthy", "Anthy", "us", "ja")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("chewing", "Chewing", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("bopomofo", "Bopomofo(Zhuyin)", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:cangjie", "Cangjie", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:quick", "Quick", "us", "zh")));
  EXPECT_EQ("zh-CN", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("pinyin", "Pinyin", "us", "zh")));
  EXPECT_EQ("en-US", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:us::eng", "USA", "us", "eng")));
  EXPECT_EQ("en-UK", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:uk::eng", "United Kingdom", "us", "eng")));
}

TEST(LanguageConfigModelTest, MaybeRewriteLanguageName) {
  EXPECT_EQ(L"English", MaybeRewriteLanguageName(L"English"));
  EXPECT_EQ(l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS),
            MaybeRewriteLanguageName(L"t"));
}

TEST(LanguageConfigModelTest, GetLanguageDisplayNameFromCode) {
  EXPECT_EQ(L"French", GetLanguageDisplayNameFromCode("fr"));
  // MaybeRewriteLanguageName() should be applied.
  EXPECT_EQ(l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS),
            GetLanguageDisplayNameFromCode("t"));
}

TEST(LanguageConfigModelTest, SortLanguageCodesByNames) {
  std::vector<std::string> language_codes;
  // Check if this function can handle an empty list.
  SortLanguageCodesByNames(&language_codes);

  language_codes.push_back("ja");
  language_codes.push_back("fr");
  language_codes.push_back("t");
  SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(3U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("t",  language_codes[2]);  // Others

  // Add a duplicate entry and see if it works.
  language_codes.push_back("ja");
  SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(4U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("ja", language_codes[2]);  // Japanese
  ASSERT_EQ("t",  language_codes[3]);  // Others
}

TEST(LanguageConfigModelTest, SortInputMethodIdsByNamesInternal) {
  std::map<std::string, std::string> id_to_language_code_map;
  id_to_language_code_map.insert(std::make_pair("mozc", "ja"));
  id_to_language_code_map.insert(std::make_pair("mozc-jp", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:jp::jpn", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:fr::fra", "fr"));
  id_to_language_code_map.insert(std::make_pair("m17n:latn-pre", "t"));

  std::vector<std::string> input_method_ids;
  // Check if this function can handle an empty list.
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);

  input_method_ids.push_back("mozc");           // Japanese
  input_method_ids.push_back("xkb:fr::fra");    // French
  input_method_ids.push_back("m17n:latn-pre");  // Others
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(3U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[2]);  // Others

  // Add a duplicate entry and see if it works.
  // Note that SortInputMethodIdsByNamesInternal uses std::stable_sort.
  input_method_ids.push_back("xkb:jp::jpn");  // also Japanese
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(4U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[3]);  // Others

  input_method_ids.push_back("mozc-jp");  // also Japanese
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(5U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese
  ASSERT_EQ("mozc-jp", input_method_ids[3]);         // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[4]);  // Others
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_DE) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:ch::ger");  // Switzerland - German
  input_method_ids.push_back("xkb:de::ger");  // Germany - German
  ReorderInputMethodIdsForLanguageCode("de", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:de::ger", input_method_ids[0]);
  EXPECT_EQ("xkb:ch::ger", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_FR) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:be::fra");  // Belgium - French
  input_method_ids.push_back("xkb:fr::fra");  // France - French
  ReorderInputMethodIdsForLanguageCode("fr", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:fr::fra", input_method_ids[0]);
  EXPECT_EQ("xkb:be::fra", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_EN_US) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:us:dvorak:eng");  // US - Dvorak - English
  input_method_ids.push_back("xkb:us::eng");  // US - English
  ReorderInputMethodIdsForLanguageCode("en-US", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("xkb:us:dvorak:eng", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_FI) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:fi::fin");  // Finland - Finnish
  ReorderInputMethodIdsForLanguageCode("fi", &input_method_ids);
  // There is no rule for reordering for Finnish.
  ASSERT_EQ(1U, input_method_ids.size());
  EXPECT_EQ("xkb:fi::fin", input_method_ids[0]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_Noop) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:fr::fra");  // France - French
  input_method_ids.push_back("xkb:be::fra");  // Belgium - French
  // If the list is already sorted, nothing should happen.
  ReorderInputMethodIdsForLanguageCode("fr", &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:fr::fra", input_method_ids[0]);
  EXPECT_EQ("xkb:be::fra", input_method_ids[1]);
}

}  // namespace input_method
}  // namespace chromeos
