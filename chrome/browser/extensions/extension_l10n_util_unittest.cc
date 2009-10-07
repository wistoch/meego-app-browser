// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_l10n_util.h"

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(Extensio8nL10nUtil, GetValidLocalesEmptyLocaleFolder) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  std::set<std::string> locales;
  EXPECT_FALSE(extension_l10n_util::GetValidLocales(src_path,
                                                    &locales,
                                                    &error));

  EXPECT_TRUE(locales.empty());
}

TEST(ExtensionL10nUtil, GetValidLocalesWithValidLocaleNoMessagesFile) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));
  ASSERT_TRUE(file_util::CreateDirectory(src_path.AppendASCII("sr")));

  std::string error;
  std::set<std::string> locales;
  EXPECT_FALSE(extension_l10n_util::GetValidLocales(src_path,
                                                    &locales,
                                                    &error));

  EXPECT_TRUE(locales.empty());
}

TEST(ExtensionL10nUtil, GetValidLocalesWithValidLocalesAndMessagesFile) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0")
      .AppendASCII(Extension::kLocaleFolder);

  std::string error;
  std::set<std::string> locales;
  EXPECT_TRUE(extension_l10n_util::GetValidLocales(install_dir,
                                                   &locales,
                                                   &error));
  EXPECT_EQ(2U, locales.size());
  EXPECT_TRUE(locales.find("sr") != locales.end());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsMissingFiles) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_TRUE(NULL == extension_l10n_util::LoadMessageCatalogs(src_path,
                                                               "en-US",
                                                               "sr",
                                                               &error));
  EXPECT_FALSE(error.empty());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsBadJSONFormat) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  FilePath locale = src_path.AppendASCII("en_US");
  ASSERT_TRUE(file_util::CreateDirectory(locale));

  std::string data = "{ \"name\":";
  ASSERT_TRUE(
      file_util::WriteFile(locale.AppendASCII(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  std::string error;
  EXPECT_TRUE(NULL == extension_l10n_util::LoadMessageCatalogs(src_path,
                                                              "en-US",
                                                              "sr",
                                                              &error));
  EXPECT_EQ("Line: 1, column: 10, Syntax error.", error);
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsDuplicateKeys) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  FilePath locale_1 = src_path.AppendASCII("en_US");
  ASSERT_TRUE(file_util::CreateDirectory(locale_1));

  std::string data =
    "{ \"name\": { \"message\": \"something\" }, "
    "\"name\": { \"message\": \"something else\" } }";
  ASSERT_TRUE(
      file_util::WriteFile(locale_1.AppendASCII(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  FilePath locale_2 = src_path.AppendASCII("sr");
  ASSERT_TRUE(file_util::CreateDirectory(locale_2));

  ASSERT_TRUE(
      file_util::WriteFile(locale_2.AppendASCII(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  std::string error;
  // JSON parser hides duplicates. We are going to get only one key/value
  // pair at the end.
  scoped_ptr<ExtensionMessageBundle> message_bundle(
      extension_l10n_util::LoadMessageCatalogs(src_path,
                                               "en-US",
                                               "sr",
                                               &error));
  EXPECT_TRUE(NULL != message_bundle.get());
  EXPECT_TRUE(error.empty());
}

bool PathsAreEqual(const FilePath& path1, const FilePath& path2) {
  FilePath::StringType path1_str = path1.value();
  std::replace(path1_str.begin(), path1_str.end(), '\\', '/');

  FilePath::StringType path2_str = path2.value();
  std::replace(path2_str.begin(), path2_str.end(), '\\', '/');

  if (path1_str == path2_str) {
    return true;
  } else {
    return false;
  }
}

TEST(ExtensionL10nUtil, GetL10nRelativePath) {
  static std::string current_locale = l10n_util::GetApplicationLocale(L"");
  std::replace(current_locale.begin(), current_locale.end(), '-', '_');

  FilePath locale_path;
  locale_path = locale_path
      .AppendASCII(Extension::kLocaleFolder)
      .AppendASCII(current_locale)
      .AppendASCII("foo")
      .AppendASCII("bar.js");

  FilePath result = extension_l10n_util::GetL10nRelativePath(
      FilePath(FILE_PATH_LITERAL("foo/bar.js")));
  EXPECT_TRUE(PathsAreEqual(locale_path, result));
}

}  // namespace
