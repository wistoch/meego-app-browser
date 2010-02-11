// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/linked_ptr.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/resource_type.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace {

TEST(ExtensionL10nUtil, GetValidLocalesEmptyLocaleFolder) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
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

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
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
      .Append(Extension::kLocaleFolder);

  std::string error;
  std::set<std::string> locales;
  EXPECT_TRUE(extension_l10n_util::GetValidLocales(install_dir,
                                                   &locales,
                                                   &error));
  EXPECT_EQ(3U, locales.size());
  EXPECT_TRUE(locales.find("sr") != locales.end());
  EXPECT_TRUE(locales.find("en") != locales.end());
  EXPECT_TRUE(locales.find("en_US") != locales.end());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsValidFallback) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0")
      .Append(Extension::kLocaleFolder);

  std::string error;
  std::set<std::string> locales;
  EXPECT_TRUE(extension_l10n_util::GetValidLocales(install_dir,
                                                   &locales,
                                                   &error));

  scoped_ptr<ExtensionMessageBundle> bundle(
      extension_l10n_util::LoadMessageCatalogs(
          install_dir, "sr", "en_US", locales, &error));
  ASSERT_FALSE(NULL == bundle.get());
  EXPECT_TRUE(error.empty());
  EXPECT_EQ("Color", bundle->GetL10nMessage("color"));
  EXPECT_EQ("Not in the US or GB.", bundle->GetL10nMessage("not_in_US_or_GB"));
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsMissingFiles) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::set<std::string> valid_locales;
  valid_locales.insert("sr");
  valid_locales.insert("en");
  std::string error;
  EXPECT_TRUE(NULL == extension_l10n_util::LoadMessageCatalogs(src_path,
                                                               "en",
                                                               "sr",
                                                               valid_locales,
                                                               &error));
  EXPECT_FALSE(error.empty());
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsBadJSONFormat) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  FilePath locale = src_path.AppendASCII("sr");
  ASSERT_TRUE(file_util::CreateDirectory(locale));

  std::string data = "{ \"name\":";
  ASSERT_TRUE(
      file_util::WriteFile(locale.Append(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  std::set<std::string> valid_locales;
  valid_locales.insert("sr");
  valid_locales.insert("en_US");
  std::string error;
  EXPECT_TRUE(NULL == extension_l10n_util::LoadMessageCatalogs(src_path,
                                                              "en_US",
                                                              "sr",
                                                              valid_locales,
                                                              &error));
  EXPECT_EQ("Line: 1, column: 10, Syntax error.", error);
}

TEST(ExtensionL10nUtil, LoadMessageCatalogsDuplicateKeys) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  FilePath locale_1 = src_path.AppendASCII("en");
  ASSERT_TRUE(file_util::CreateDirectory(locale_1));

  std::string data =
    "{ \"name\": { \"message\": \"something\" }, "
    "\"name\": { \"message\": \"something else\" } }";
  ASSERT_TRUE(
      file_util::WriteFile(locale_1.Append(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  FilePath locale_2 = src_path.AppendASCII("sr");
  ASSERT_TRUE(file_util::CreateDirectory(locale_2));

  ASSERT_TRUE(
      file_util::WriteFile(locale_2.Append(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  std::set<std::string> valid_locales;
  valid_locales.insert("sr");
  valid_locales.insert("en");
  std::string error;
  // JSON parser hides duplicates. We are going to get only one key/value
  // pair at the end.
  scoped_ptr<ExtensionMessageBundle> message_bundle(
      extension_l10n_util::LoadMessageCatalogs(src_path,
                                               "en",
                                               "sr",
                                               valid_locales,
                                               &error));
  EXPECT_TRUE(NULL != message_bundle.get());
  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, GetParentLocales) {
  std::vector<std::string> locales;
  const std::string top_locale("sr_Cyrl_RS");
  extension_l10n_util::GetParentLocales(top_locale, &locales);

  ASSERT_EQ(3U, locales.size());
  EXPECT_EQ("sr_Cyrl_RS", locales[0]);
  EXPECT_EQ("sr_Cyrl", locales[1]);
  EXPECT_EQ("sr", locales[2]);
}

// Caller owns the returned object.
ExtensionMessageBundle* CreateManifestBundle() {
  linked_ptr<DictionaryValue> catalog(new DictionaryValue);

  DictionaryValue* name_tree = new DictionaryValue();
  name_tree->SetString(L"message", "name");
  catalog->Set(L"name", name_tree);

  DictionaryValue* description_tree = new DictionaryValue();
  description_tree->SetString(L"message", "description");
  catalog->Set(L"description", description_tree);

  DictionaryValue* action_title_tree = new DictionaryValue();
  action_title_tree->SetString(L"message", "action title");
  catalog->Set(L"title", action_title_tree);

  std::vector<linked_ptr<DictionaryValue> > catalogs;
  catalogs.push_back(catalog);

  std::string error;
  ExtensionMessageBundle* bundle =
    ExtensionMessageBundle::Create(catalogs, &error);
  EXPECT_TRUE(NULL != bundle);
  EXPECT_TRUE(error.empty());

  return bundle;
}

TEST(ExtensionL10nUtil, LocalizeEmptyManifest) {
  DictionaryValue manifest;
  std::string error;
  scoped_ptr<ExtensionMessageBundle> messages(CreateManifestBundle());

  EXPECT_FALSE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));
  EXPECT_EQ(errors::kInvalidName, error);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithoutNameMsgAndEmptyDescription) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "no __MSG");
  std::string error;
  scoped_ptr<ExtensionMessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  std::string result;
  ASSERT_TRUE(manifest.GetString(keys::kName, &result));
  EXPECT_EQ("no __MSG", result);

  EXPECT_FALSE(manifest.HasKey(keys::kDescription));

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameMsgAndEmptyDescription) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "__MSG_name__");
  std::string error;
  scoped_ptr<ExtensionMessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  std::string result;
  ASSERT_TRUE(manifest.GetString(keys::kName, &result));
  EXPECT_EQ("name", result);

  EXPECT_FALSE(manifest.HasKey(keys::kDescription));

  EXPECT_TRUE(error.empty());
}

TEST(ExtensionL10nUtil, LocalizeManifestWithBadNameMsg) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "__MSG_name_is_bad__");
  manifest.SetString(keys::kDescription, "__MSG_description__");
  std::string error;
  scoped_ptr<ExtensionMessageBundle> messages(CreateManifestBundle());

  EXPECT_FALSE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  std::string result;
  ASSERT_TRUE(manifest.GetString(keys::kName, &result));
  EXPECT_EQ("__MSG_name_is_bad__", result);

  ASSERT_TRUE(manifest.GetString(keys::kDescription, &result));
  EXPECT_EQ("__MSG_description__", result);

  EXPECT_EQ("Variable __MSG_name_is_bad__ used but not defined.", error);
}

TEST(ExtensionL10nUtil, LocalizeManifestWithNameDescriptionDefaultTitleMsgs) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "__MSG_name__");
  manifest.SetString(keys::kDescription, "__MSG_description__");
  std::wstring action_title(keys::kBrowserAction);
  action_title.append(L".");
  action_title.append(keys::kPageActionDefaultTitle);
  manifest.SetString(action_title, "__MSG_title__");

  std::string error;
  scoped_ptr<ExtensionMessageBundle> messages(CreateManifestBundle());

  EXPECT_TRUE(
      extension_l10n_util::LocalizeManifest(*messages, &manifest, &error));

  std::string result;
  ASSERT_TRUE(manifest.GetString(keys::kName, &result));
  EXPECT_EQ("name", result);

  ASSERT_TRUE(manifest.GetString(keys::kDescription, &result));
  EXPECT_EQ("description", result);

  ASSERT_TRUE(manifest.GetString(action_title, &result));
  EXPECT_EQ("action title", result);

  EXPECT_TRUE(error.empty());
}

// Try with NULL manifest.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestWithNullManifest) {
  ExtensionInfo info(NULL, "", FilePath(), Extension::LOAD);

  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

// Try with default and current locales missing.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestEmptyManifest) {
  DictionaryValue manifest;
  ExtensionInfo info(&manifest, "", FilePath(), Extension::LOAD);

  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

// Try with missing current_locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestWithDefaultLocale) {
  DictionaryValue manifest;
  manifest.SetString(keys::kDefaultLocale, "en_US");

  ExtensionInfo info(&manifest, "", FilePath(), Extension::LOAD);

  EXPECT_TRUE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

// Try with missing default_locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestWithCurrentLocale) {
  DictionaryValue manifest;
  manifest.SetString(keys::kCurrentLocale,
                     extension_l10n_util::CurrentLocaleOrDefault());

  ExtensionInfo info(&manifest, "", FilePath(), Extension::LOAD);

  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

// Try with all data present, but with same current_locale as system locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestSameCurrentLocale) {
  DictionaryValue manifest;
  manifest.SetString(keys::kDefaultLocale, "en_US");
  manifest.SetString(keys::kCurrentLocale,
                     extension_l10n_util::CurrentLocaleOrDefault());

  ExtensionInfo info(&manifest, "", FilePath(), Extension::LOAD);

  EXPECT_FALSE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

// Try with all data present, but with different current_locale.
TEST(ExtensionL10nUtil, ShouldRelocalizeManifestDifferentCurrentLocale) {
  DictionaryValue manifest;
  manifest.SetString(keys::kDefaultLocale, "en_US");
  manifest.SetString(keys::kCurrentLocale, "sr");

  ExtensionInfo info(&manifest, "", FilePath(), Extension::LOAD);

  EXPECT_TRUE(extension_l10n_util::ShouldRelocalizeManifest(info));
}

class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler() {}

  bool OnRequestRedirected(int request_id, const GURL& url,
                           ResourceResponse* response, bool* defer) {
    return true;
  }

  bool OnResponseStarted(int request_id, ResourceResponse* response) {
    return true;
  }

  bool OnWillRead(
      int request_id, net::IOBuffer** buf, int* buf_size, int min_size) {
    return true;
  }

  bool OnReadCompleted(int request_id, int* bytes_read) { return true; }

  bool OnResponseCompleted(
    int request_id, const URLRequestStatus& status, const std::string& info) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

class ApplyMessageFilterPolicyTest : public testing::Test {
 protected:
  void SetUp() {
    url_.reset(new GURL(
        "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/popup.html"));
    resource_type_ = ResourceType::STYLESHEET;
    resource_handler_.reset(new DummyResourceHandler());
    request_info_.reset(CreateNewResourceRequestInfo());
  }

  ResourceDispatcherHostRequestInfo* CreateNewResourceRequestInfo() {
    return new ResourceDispatcherHostRequestInfo(
        resource_handler_.get(), ChildProcessInfo::RENDER_PROCESS, 0, 0, 0,
        "not important", "not important",
        ResourceType::STYLESHEET, 0U, false, false, -1, -1);
  }

  scoped_ptr<GURL> url_;
  ResourceType::Type resource_type_;
  scoped_ptr<DummyResourceHandler> resource_handler_;
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info_;
};

TEST_F(ApplyMessageFilterPolicyTest, WrongScheme) {
  url_.reset(new GURL("html://behllobkkfkfnphdnhnkndlbkcpglgmj/popup.html"));
  extension_l10n_util::ApplyMessageFilterPolicy(
      *url_, resource_type_, request_info_.get());

  EXPECT_EQ(FilterPolicy::DONT_FILTER, request_info_->filter_policy());
}

TEST_F(ApplyMessageFilterPolicyTest, GoodScheme) {
  extension_l10n_util::ApplyMessageFilterPolicy(
      *url_, resource_type_, request_info_.get());

  EXPECT_EQ(FilterPolicy::FILTER_EXTENSION_MESSAGES,
            request_info_->filter_policy());
}

TEST_F(ApplyMessageFilterPolicyTest, GoodSchemeWithSecurityFilter) {
  request_info_->set_filter_policy(FilterPolicy::FILTER_ALL_EXCEPT_IMAGES);
  extension_l10n_util::ApplyMessageFilterPolicy(
      *url_, resource_type_, request_info_.get());

  EXPECT_EQ(FilterPolicy::FILTER_ALL_EXCEPT_IMAGES,
            request_info_->filter_policy());
}

TEST_F(ApplyMessageFilterPolicyTest, GoodSchemeWrongResourceType) {
  resource_type_ = ResourceType::MAIN_FRAME;
  extension_l10n_util::ApplyMessageFilterPolicy(
      *url_, resource_type_, request_info_.get());

  EXPECT_EQ(FilterPolicy::DONT_FILTER, request_info_->filter_policy());
}

TEST_F(ApplyMessageFilterPolicyTest, WrongSchemeResourceAndFilter) {
  url_.reset(new GURL("html://behllobkkfkfnphdnhnkndlbkcpglgmj/popup.html"));
  resource_type_ = ResourceType::MEDIA;
  request_info_->set_filter_policy(FilterPolicy::FILTER_ALL);
  extension_l10n_util::ApplyMessageFilterPolicy(
      *url_, resource_type_, request_info_.get());

  EXPECT_EQ(FilterPolicy::FILTER_ALL, request_info_->filter_policy());
}

}  // namespace
