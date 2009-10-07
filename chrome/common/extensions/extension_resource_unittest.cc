// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionResourceTest, CreateEmptyResource) {
  ExtensionResource resource;

  EXPECT_TRUE(resource.extension_root().empty());
  EXPECT_TRUE(resource.relative_path().empty());
  EXPECT_TRUE(resource.GetFilePath().empty());
}

const FilePath::StringType ToLower(const FilePath::StringType& in_str) {
  FilePath::StringType str(in_str);
  std::transform(str.begin(), str.end(), str.begin(), tolower);
  return str;
}

TEST(ExtensionResourceTest, CreateWithMissingResourceOnDisk) {
  FilePath root_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &root_path));
  FilePath relative_path;
  relative_path = relative_path.AppendASCII("cira.js");
  ExtensionResource resource(root_path, relative_path);

  EXPECT_EQ(root_path.value(), resource.extension_root().value());
  EXPECT_EQ(relative_path.value(), resource.relative_path().value());
  EXPECT_EQ(root_path.Append(relative_path).value(),
    ToLower(resource.GetFilePath().value()));

  EXPECT_FALSE(resource.GetFilePath().empty());
}

TEST(ExtensionResourceTest, CreateWithBothResourcesOnDisk) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  const char* filename = "res.ico";
  FilePath root_resource = temp.path().AppendASCII(filename);
  std::string data = "some foo";
  ASSERT_TRUE(file_util::WriteFile(root_resource.AppendASCII(filename),
      data.c_str(), data.length()));

  FilePath l10n_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(l10n_path));

  static std::string current_locale = l10n_util::GetApplicationLocale(L"");
  std::replace(current_locale.begin(), current_locale.end(), '-', '_');
  l10n_path = l10n_path.AppendASCII(current_locale);
  ASSERT_TRUE(file_util::CreateDirectory(l10n_path));

  ASSERT_TRUE(file_util::WriteFile(l10n_path.AppendASCII(filename),
      data.c_str(), data.length()));

  FilePath path;
  ExtensionResource resource(temp.path(), FilePath().AppendASCII(filename));
  FilePath resolved_path = resource.GetFilePath();

  EXPECT_EQ(l10n_path.AppendASCII(filename).value(), resolved_path.value());
  EXPECT_EQ(temp.path().value(), resource.extension_root().value());
  EXPECT_EQ(FilePath().AppendASCII(filename).value(),
      resource.relative_path().value());
}
