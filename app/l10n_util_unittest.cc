// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <cstdlib>
#endif

#include "app/app_paths.h"
#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#if !defined(OS_MACOSX)
#include "app/test/data/resource.h"
#endif
#include "base/basictypes.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "unicode/locid.h"

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const std::wstring& string) : string_(string) {}
  const std::wstring& string() const { return string_; }

 private:
  std::wstring string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

}  // namespace

class L10nUtilTest : public PlatformTest {
};

#if defined(OS_WIN)
// TODO(beng): disabled until app strings move to app.
TEST_F(L10nUtilTest, DISABLED_GetString) {
  std::wstring s = l10n_util::GetString(IDS_SIMPLE);
  EXPECT_EQ(std::wstring(L"Hello World!"), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS, L"chrome", L"10");
  EXPECT_EQ(std::wstring(L"Hello, chrome. Your number is 10."), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(std::wstring(L"You owe me $20."), s);
}
#endif  // defined(OS_WIN)

TEST_F(L10nUtilTest, TruncateString) {
  string16 string = ASCIIToUTF16("foooooey    bxxxar baz");

  // Make sure it doesn't modify the string if length > string length.
  EXPECT_EQ(string, l10n_util::TruncateString(string, 100));

  // Test no characters.
  EXPECT_EQ(L"", UTF16ToWide(l10n_util::TruncateString(string, 0)));

  // Test 1 character.
  EXPECT_EQ(L"\x2026", UTF16ToWide(l10n_util::TruncateString(string, 1)));

  // Test adds ... at right spot when there is enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026",
            UTF16ToWide(l10n_util::TruncateString(string, 14)));

  // Test adds ... at right spot when there is not enough space in first word.
  EXPECT_EQ(L"f\x2026", UTF16ToWide(l10n_util::TruncateString(string, 2)));

  // Test adds ... at right spot when there is not enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026",
            UTF16ToWide(l10n_util::TruncateString(string, 11)));

  // Test completely truncates string if break is on initial whitespace.
  EXPECT_EQ(L"\x2026",
            UTF16ToWide(l10n_util::TruncateString(ASCIIToUTF16("   "), 2)));
}

void SetICUDefaultLocale(const std::string& locale_string) {
  icu::Locale locale(locale_string.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

#if !defined(OS_MACOSX)
// We are disabling this test on MacOS because GetApplicationLocale() as an
// API isn't something that we'll easily be able to unit test in this manner.
// The meaning of that API, on the Mac, is "the locale used by Cocoa's main
// nib file", which clearly can't be stubbed by a test app that doesn't use
// Cocoa.

void SetDefaultLocaleForTest(const std::string& tag, base::Environment* env) {
#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
  env->SetVar("LANGUAGE", tag);
#else
  SetICUDefaultLocale(tag);
#endif
}

TEST_F(L10nUtilTest, GetAppLocale) {
  scoped_ptr<base::Environment> env;
  // Use a temporary locale dir so we don't have to actually build the locale
  // dlls for this test.
  FilePath orig_locale_dir;
  PathService::Get(app::DIR_LOCALES, &orig_locale_dir);
  FilePath new_locale_dir;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("l10n_util_test"),
      &new_locale_dir));
  PathService::Override(app::DIR_LOCALES, new_locale_dir);
  // Make fake locale files.
  std::string filenames[] = {
    "en-US",
    "en-GB",
    "fr",
    "es-419",
    "es",
    "zh-TW",
    "zh-CN",
    "he",
    "fil",
    "nb",
    "am",
  };

#if defined(OS_WIN)
  static const char kLocaleFileExtension[] = ".dll";
#elif defined(OS_POSIX)
  static const char kLocaleFileExtension[] = ".pak";
#endif
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    FilePath filename = new_locale_dir.AppendASCII(
        filenames[i] + kLocaleFileExtension);
    file_util::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  icu::Locale locale = icu::Locale::getDefault();

#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
  env.reset(base::Environment::Create());

  // Test the support of LANGUAGE environment variable.
  SetICUDefaultLocale("en-US");
  env->SetVar("LANGUAGE", "xx:fr_CA");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(""));

  env->SetVar("LANGUAGE", "xx:yy:en_gb.utf-8@quot");
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(""));

  env->SetVar("LANGUAGE", "xx:zh-hk");
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(""));

  // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
  // when LANGUAGE is specified. If no language specified in LANGUAGE is valid,
  // then just fallback to the default language, which is en-US for us.
  SetICUDefaultLocale("fr-FR");
  env->SetVar("LANGUAGE", "xx:yy");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));

  env->SetVar("LANGUAGE", "/fr:zh_CN");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(""));

  // Test prioritization of the different environment variables.
  env->SetVar("LANGUAGE", "fr");
  env->SetVar("LC_ALL", "es");
  env->SetVar("LC_MESSAGES", "he");
  env->SetVar("LANG", "nb");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(""));
  env->UnSetVar("LANGUAGE");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(""));
  env->UnSetVar("LC_ALL");
  EXPECT_EQ("he", l10n_util::GetApplicationLocale(""));
  env->UnSetVar("LC_MESSAGES");
  EXPECT_EQ("nb", l10n_util::GetApplicationLocale(""));
  env->UnSetVar("LANG");
#endif  // defined(OS_POSIX) && !defined(OS_CHROMEOS)

  SetDefaultLocaleForTest("en-US", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("xx", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));

#if defined(OS_CHROMEOS)
  // ChromeOS honors preferred locale first in GetApplicationLocale(),
  // defaulting to en-US, while other targets first honor other signals.
  SetICUDefaultLocale("en-GB");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));

  SetICUDefaultLocale("en-US");
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-GB"));

#else  // defined(OS_CHROMEOS)
  SetDefaultLocaleForTest("en-GB", env.get());
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("fr-CA", env.get());
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("es-MX", env.get());
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("es-AR", env.get());
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("es-ES", env.get());
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("es", env.get());
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("zh-HK", env.get());
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("zh-MK", env.get());
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(""));

  SetDefaultLocaleForTest("zh-SG", env.get());
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(""));
#endif  // defined (OS_CHROMEOS)

#if defined(OS_WIN)
  // We don't allow user prefs for locale on linux/mac.
  SetICUDefaultLocale("en-US");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr"));
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr-CA"));

  SetICUDefaultLocale("en-US");
  // Aliases iw, no, tl to he, nb, fil.
  EXPECT_EQ("he", l10n_util::GetApplicationLocale("iw"));
  EXPECT_EQ("nb", l10n_util::GetApplicationLocale("no"));
  EXPECT_EQ("fil", l10n_util::GetApplicationLocale("tl"));
  // es-419 and es-XX (where XX is not Spain) should be
  // mapped to es-419 (Latin American Spanish).
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-419"));
  EXPECT_EQ("es", l10n_util::GetApplicationLocale("es-ES"));
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-AR"));

  SetICUDefaultLocale("es-AR");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale("es"));

  SetICUDefaultLocale("zh-HK");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale("zh-CN"));

  SetICUDefaultLocale("he");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en"));

  // Amharic should be blocked unless OS is Vista or newer.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    SetICUDefaultLocale("am");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("am"));
  } else {
    SetICUDefaultLocale("am");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale(""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale("am"));
  }
#endif  // defined(OS_WIN)

  // Clean up.
  PathService::Override(app::DIR_LOCALES, orig_locale_dir);
  file_util::Delete(new_locale_dir, true);
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
}
#endif  // !defined(OS_MACOSX)

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<StringWrapper*> strings;
  strings.push_back(new StringWrapper(L"C"));
  strings.push_back(new StringWrapper(L"d"));
  strings.push_back(new StringWrapper(L"b"));
  strings.push_back(new StringWrapper(L"a"));
  l10n_util::SortStringsUsingMethod(L"en-US", &strings, &StringWrapper::string);
  ASSERT_TRUE(L"a" == strings[0]->string());
  ASSERT_TRUE(L"b" == strings[1]->string());
  ASSERT_TRUE(L"C" == strings[2]->string());
  ASSERT_TRUE(L"d" == strings[3]->string());
  STLDeleteElements(&strings);
}

// Test upper and lower case string conversion.
TEST_F(L10nUtilTest, UpperLower) {
  string16 mixed(ASCIIToUTF16("Text with UPPer & lowER casE."));
  const string16 expected_lower(ASCIIToUTF16("text with upper & lower case."));
  const string16 expected_upper(ASCIIToUTF16("TEXT WITH UPPER & LOWER CASE."));

  string16 result = l10n_util::ToLower(mixed);
  EXPECT_EQ(result, expected_lower);

  result = l10n_util::ToUpper(mixed);
  EXPECT_EQ(result, expected_upper);
}

TEST_F(L10nUtilTest, LocaleDisplayName) {
  // TODO(jungshik): Make this test more extensive.
  // Test zh-CN and zh-TW are treated as zh-Hans and zh-Hant.
  string16 result = l10n_util::GetDisplayNameForLocale("zh-CN", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Chinese (Simplified Han)"));

  result = l10n_util::GetDisplayNameForLocale("zh-TW", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Chinese (Traditional Han)"));

  result = l10n_util::GetDisplayNameForLocale("pt-BR", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Portuguese (Brazil)"));

  result = l10n_util::GetDisplayNameForLocale("es-419", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Spanish (Latin America and the Caribbean)"));
}

TEST_F(L10nUtilTest, GetParentLocales) {
  std::vector<std::string> locales;
  const std::string top_locale("sr_Cyrl_RS");
  l10n_util::GetParentLocales(top_locale, &locales);

  ASSERT_EQ(3U, locales.size());
  EXPECT_EQ("sr_Cyrl_RS", locales[0]);
  EXPECT_EQ("sr_Cyrl", locales[1]);
  EXPECT_EQ("sr", locales[2]);
}

TEST_F(L10nUtilTest, IsValidLocaleSyntax) {
  // Test valid locales.
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("fr"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("de"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("pt"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("fil"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("haw"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en-US"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en_US"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en_GB"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("pt-BR"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh_CN"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh_Hans"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh_Hans_CN"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh_Hant"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zh_Hant_TW"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("fr_CA"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("i-klingon"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("es-419"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en_IE_PREEURO"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en_IE_u_cu_IEP"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("en_IE@currency=IEP"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("fr@x=y"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax("zn_CN@foo=bar"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax(
      "fr@collation=phonebook;calendar=islamic-civil"));
  EXPECT_EQ(true, l10n_util::IsValidLocaleSyntax(
      "sr_Latn_RS_REVISED@currency=USD"));

  // Test invalid locales.
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax(""));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("x"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("12"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("456"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("a1"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("enUS"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("zhcn"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en.US"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en#US"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("-en-US"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en-US-"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("123-en-US"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("Latin"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("German"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("pt--BR"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("sl-macedonia"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("@"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en-US@"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en-US@x"));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en-US@x="));
  EXPECT_EQ(false, l10n_util::IsValidLocaleSyntax("en-US@=y"));
}
