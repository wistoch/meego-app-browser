// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/language_library.h"
#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageLibraryTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("ja", LanguageLibrary::NormalizeLanguageCode("ja"));
  EXPECT_EQ("ja", LanguageLibrary::NormalizeLanguageCode("jpn"));
  EXPECT_EQ("t", LanguageLibrary::NormalizeLanguageCode("t"));
  EXPECT_EQ("zh-CN", LanguageLibrary::NormalizeLanguageCode("zh-CN"));
  EXPECT_EQ("zh-CN", LanguageLibrary::NormalizeLanguageCode("zh_CN"));
  EXPECT_EQ("en-US", LanguageLibrary::NormalizeLanguageCode("EN_us"));
  // See app/l10n_util.cc for es-419.
  EXPECT_EQ("es-419", LanguageLibrary::NormalizeLanguageCode("es_419"));

  // Special three-letter language codes.
  EXPECT_EQ("cs", LanguageLibrary::NormalizeLanguageCode("cze"));
  EXPECT_EQ("de", LanguageLibrary::NormalizeLanguageCode("ger"));
  EXPECT_EQ("el", LanguageLibrary::NormalizeLanguageCode("gre"));
  EXPECT_EQ("hr", LanguageLibrary::NormalizeLanguageCode("scr"));
  EXPECT_EQ("ro", LanguageLibrary::NormalizeLanguageCode("rum"));
  EXPECT_EQ("sk", LanguageLibrary::NormalizeLanguageCode("slo"));
}

TEST(LanguageLibraryTest, IsKeyboardLayout) {
  EXPECT_TRUE(LanguageLibrary::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(LanguageLibrary::IsKeyboardLayout("anthy"));
}

TEST(LanguageLibraryTest, GetLanguageCodeFromDescriptor) {
  EXPECT_EQ("ja", LanguageLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("anthy", "Anthy", "us", "ja")));
  EXPECT_EQ("zh-TW", LanguageLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("chewing", "Chewing", "us", "zh")));
  EXPECT_EQ("en-US", LanguageLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:us::eng", "USA", "us", "eng")));
  EXPECT_EQ("en-UK", LanguageLibrary::GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:uk::eng", "United Kingdom", "us", "eng")));
}

TEST(LanguageLibraryTest, GetKeyboardLayoutName) {
  EXPECT_EQ("us", LanguageLibrary::GetKeyboardLayoutName("xkb:us::eng"));
  EXPECT_EQ("gb", LanguageLibrary::GetKeyboardLayoutName("xkb:gb::eng"));
  EXPECT_EQ("us(dvorak)",
            LanguageLibrary::GetKeyboardLayoutName("xkb:us:dvorak:eng"));
  EXPECT_EQ("us", LanguageLibrary::GetKeyboardLayoutName(""));
  EXPECT_EQ("us", LanguageLibrary::GetKeyboardLayoutName("bogus"));
}

}  // namespace chromeos
