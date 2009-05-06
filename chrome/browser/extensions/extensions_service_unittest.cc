// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

struct ExtensionsOrder {
  bool operator()(const Extension* a, const Extension* b) {
    return a->name() < b->name();
  }
};

static std::vector<std::string> GetErrors() {
  const std::vector<std::string>* errors =
      ExtensionErrorReporter::GetInstance()->GetErrors();
  std::vector<std::string> ret_val;

  for (std::vector<std::string>::const_iterator iter = errors->begin();
       iter != errors->end(); ++iter) {
    if (iter->find(".svn") == std::string::npos) {
      ret_val.push_back(*iter);
    }
  }

  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(ret_val.begin(), ret_val.end());

  return ret_val;
}

}  // namespace

// A mock implementation of ExtensionsServiceFrontendInterface for testing the
// backend.
class ExtensionsServiceTestFrontend
    : public ExtensionsServiceFrontendInterface {
 public:

  ~ExtensionsServiceTestFrontend() {
    for (ExtensionList::iterator iter = extensions_.begin();
         iter != extensions_.end(); ++iter) {
      delete *iter;
    }
  }

  ExtensionList* extensions() {
    return &extensions_;
  }

  Extension* installed() {
    return installed_;
  }

  // ExtensionsServiceFrontendInterface
  virtual MessageLoop* GetMessageLoop() {
    return &message_loop_;
  }

  virtual void InstallExtension(const FilePath& extension_path) {
  }

  virtual void LoadExtension(const FilePath& extension_path) {
  }

  virtual void OnExtensionsLoaded(ExtensionList* new_extensions) {
    extensions_.insert(extensions_.end(), new_extensions->begin(),
                       new_extensions->end());
    delete new_extensions;
    // In the tests we rely on extensions being in particular order, which is
    // not always the case (and is not guaranteed by used APIs).
    std::stable_sort(extensions_.begin(), extensions_.end(), ExtensionsOrder());
  }

  virtual void OnExtensionInstalled(Extension* extension, bool is_update) {
    installed_ = extension;
  }

  virtual Extension* GetExtensionByID(std::string id) {
    return NULL;
  }

  void TestInstallExtension(const FilePath& path,
                            ExtensionsServiceBackend* backend,
                            bool should_succeed) {
    ASSERT_TRUE(file_util::PathExists(path));
    backend->InstallExtension(path,
        scoped_refptr<ExtensionsServiceFrontendInterface>(this));
    message_loop_.RunAllPending();
    std::vector<std::string> errors = GetErrors();
    if (should_succeed) {
      EXPECT_TRUE(installed_) << path.value();
      EXPECT_EQ(0u, errors.size()) << path.value();
      for (std::vector<std::string>::iterator err = errors.begin();
        err != errors.end(); ++err) {
        LOG(ERROR) << *err;
      }
    } else {
      EXPECT_FALSE(installed_) << path.value();
      EXPECT_EQ(1u, errors.size()) << path.value();
    }

    installed_ = NULL;
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }


 private:
  MessageLoop message_loop_;
  ExtensionList extensions_;
  Extension* installed_;
};

// make the test a PlatformTest to setup autorelease pools properly on mac
class ExtensionsServiceTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    ExtensionErrorReporter::Init(false);  // no noisy errors
  }

  virtual void SetUp() {
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }
};

// Test loading good extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectorySuccess) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  extensions_path = extensions_path.AppendASCII("good");

  scoped_refptr<ExtensionsServiceBackend> backend(
      new ExtensionsServiceBackend(extensions_path));
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  std::vector<Extension*> extensions;
  backend->LoadExtensionsFromInstallDirectory(
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(3u, frontend->extensions()->size());

  EXPECT_EQ(std::string("00123456789abcdef0123456789abcdef0123456"),
            frontend->extensions()->at(0)->id());
  EXPECT_EQ(std::string("My extension 1"),
            frontend->extensions()->at(0)->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            frontend->extensions()->at(0)->description());

  Extension* extension = frontend->extensions()->at(0);
  const UserScriptList& scripts = extension->content_scripts();
  const std::vector<std::string>& toolstrips = extension->toolstrips();
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(2u, scripts[0].url_patterns().size());
  EXPECT_EQ("http://*.google.com/*",
            scripts[0].url_patterns()[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*",
            scripts[0].url_patterns()[1].GetAsString());
  EXPECT_EQ(2u, scripts[0].js_scripts().size());
  EXPECT_EQ(extension->path().AppendASCII("script1.js").value(),
            scripts[0].js_scripts()[0].path().value());
  EXPECT_EQ(extension->path().AppendASCII("script2.js").value(),
            scripts[0].js_scripts()[1].path().value());
  EXPECT_TRUE(extension->plugins_dir().empty());
  EXPECT_EQ(1u, scripts[1].url_patterns().size());
  EXPECT_EQ("http://*.news.com/*", scripts[1].url_patterns()[0].GetAsString());
  EXPECT_EQ(extension->path().AppendASCII("js_files").AppendASCII("script3.js")
      .value(), scripts[1].js_scripts()[0].path().value());
  const std::vector<URLPattern> permissions = extension->permissions();
  ASSERT_EQ(2u, permissions.size());
  EXPECT_EQ("http://*.google.com/*", permissions[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*", permissions[1].GetAsString());
  ASSERT_EQ(2u, toolstrips.size());
  EXPECT_EQ("toolstrip1.html", toolstrips[0]);
  EXPECT_EQ("toolstrip2.html", toolstrips[1]);

  EXPECT_EQ(std::string("10123456789abcdef0123456789abcdef0123456"),
            frontend->extensions()->at(1)->id());
  EXPECT_EQ(std::string("My extension 2"),
            frontend->extensions()->at(1)->name());
  EXPECT_EQ(std::string(""),
            frontend->extensions()->at(1)->description());
  EXPECT_EQ(frontend->extensions()->at(1)->path().AppendASCII("npapi").value(),
            frontend->extensions()->at(1)->plugins_dir().value());
  EXPECT_EQ(frontend->extensions()->at(1)->GetResourceURL("background.html"),
            frontend->extensions()->at(1)->background_url());
  ASSERT_EQ(0u, frontend->extensions()->at(1)->content_scripts().size());

  EXPECT_EQ(std::string("20123456789abcdef0123456789abcdef0123456"),
            frontend->extensions()->at(2)->id());
  EXPECT_EQ(std::string("My extension 3"),
            frontend->extensions()->at(2)->name());
  EXPECT_EQ(std::string(""),
            frontend->extensions()->at(2)->description());
  ASSERT_EQ(0u, frontend->extensions()->at(2)->content_scripts().size());
};

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectoryFail) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  extensions_path = extensions_path.AppendASCII("bad");

  scoped_refptr<ExtensionsServiceBackend> backend(
      new ExtensionsServiceBackend(extensions_path));
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  std::vector<Extension*> extensions;
  backend->LoadExtensionsFromInstallDirectory(
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();

  EXPECT_EQ(4u, GetErrors().size());
  EXPECT_EQ(0u, frontend->extensions()->size());

  EXPECT_TRUE(MatchPattern(GetErrors()[0],
      std::string("Could not load extension from '*'. * ") +
      JSONReader::kBadRootElementType)) << GetErrors()[0];

  EXPECT_TRUE(MatchPattern(GetErrors()[1],
      std::string("Could not load extension from '*'. ") +
      Extension::kMissingFileError)) << GetErrors()[1];

  EXPECT_TRUE(MatchPattern(GetErrors()[2],
      std::string("Could not load extension from '*'. ") +
      Extension::kInvalidManifestError)) << GetErrors()[2];

  EXPECT_TRUE(MatchPattern(GetErrors()[3],
      "Could not load extension from '*'. Could not read '*' file.")) <<
      GetErrors()[3];
};

// Test installing extensions.
TEST_F(ExtensionsServiceTest, InstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath install_dir;
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("ext_test"),
                                   &install_dir);
  scoped_refptr<ExtensionsServiceBackend> backend(
      new ExtensionsServiceBackend(install_dir));
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  FilePath path = extensions_path.AppendASCII("good.crx");

  // A simple extension that should install without error.
  frontend->TestInstallExtension(path, backend, true);
  // TODO(erikkay): verify the contents of the installed extension.

  // Installing the same extension twice should fail.
  frontend->TestInstallExtension(path, backend, false);

  // 0-length extension file.
  path = extensions_path.AppendASCII("not_an_extension.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Bad magic number.
  path = extensions_path.AppendASCII("bad_magic.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Poorly formed JSON.
  path = extensions_path.AppendASCII("bad_json.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Incorrect zip hash.
  path = extensions_path.AppendASCII("bad_hash.crx");
  frontend->TestInstallExtension(path, backend, false);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

TEST_F(ExtensionsServiceTest, LoadExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  scoped_refptr<ExtensionsServiceBackend> backend(
      new ExtensionsServiceBackend(extensions_path));
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  FilePath ext1 = extensions_path.AppendASCII("good").AppendASCII("extension1")
      .AppendASCII("1");
  backend->LoadSingleExtension(ext1,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, frontend->extensions()->size());

  FilePath no_manifest = extensions_path.AppendASCII("bad")
      .AppendASCII("no_manifest").AppendASCII("1");
  backend->LoadSingleExtension(no_manifest,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();
  EXPECT_EQ(1u, GetErrors().size());
  ASSERT_EQ(1u, frontend->extensions()->size());
}

TEST_F(ExtensionsServiceTest, GenerateID) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  scoped_refptr<ExtensionsServiceBackend> backend(
      new ExtensionsServiceBackend(extensions_path));
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  FilePath no_id_ext = extensions_path.AppendASCII("no_id");
  backend->LoadSingleExtension(no_id_ext,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, frontend->extensions()->size());
  std::string id1 = frontend->extensions()->at(0)->id();
  ASSERT_EQ("0000000000000000000000000000000000000000", id1);
  ASSERT_EQ("chrome-extension://0000000000000000000000000000000000000000/",
            frontend->extensions()->at(0)->url().spec());

  backend->LoadSingleExtension(no_id_ext,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get()));
  frontend->GetMessageLoop()->RunAllPending();
  std::string id2 = frontend->extensions()->at(1)->id();
  ASSERT_EQ("0000000000000000000000000000000000000001", id2);
  ASSERT_EQ("chrome-extension://0000000000000000000000000000000000000001/",
            frontend->extensions()->at(1)->url().spec());
}

// Test that extensions get grouped in the right SiteInstance (and therefore
// process) based on their URLs.
TEST_F(ExtensionsServiceTest, ProcessGrouping) {
  // Extensions in different profiles should always be different SiteInstances.
  // Note: we don't initialize these, since we're not testing that
  // functionality.  This means we can get away with a NULL UserScriptMaster.
  TestingProfile profile1(1);
  scoped_refptr<ExtensionsService> frontend1(
      new ExtensionsService(&profile1, NULL));

  TestingProfile profile2(2);
  scoped_refptr<ExtensionsService> frontend2(
      new ExtensionsService(&profile2, NULL));

  // Extensions with common origins ("scheme://id/") should be grouped in the
  // same SiteInstance.
  GURL ext1_url1("chrome-extensions://ext1_id/index.html");
  GURL ext1_url2("chrome-extensions://ext1_id/toolstrips/toolstrip.html");
  GURL ext2_url1("chrome-extensions://ext2_id/index.html");

  scoped_refptr<SiteInstance> site11 =
      frontend1->GetSiteInstanceForURL(ext1_url1);
  scoped_refptr<SiteInstance> site12 =
      frontend1->GetSiteInstanceForURL(ext1_url2);
  EXPECT_EQ(site11, site12);

  scoped_refptr<SiteInstance> site21 =
      frontend1->GetSiteInstanceForURL(ext2_url1);
  EXPECT_NE(site11, site21);

  scoped_refptr<SiteInstance> other_profile_site =
      frontend2->GetSiteInstanceForURL(ext1_url1);
  EXPECT_NE(site11, other_profile_site);
}
