// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBKIT_CLIENT_H_
#define WEBKIT_SUPPORT_TEST_WEBKIT_CLIENT_H_

#include "webkit/glue/webfilesystem_impl.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/support/weburl_loader_mock_factory.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_database_system.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_webmimeregistry_impl.h"

// An implementation of WebKitClient for tests.
class TestWebKitClient : public webkit_glue::WebKitClientImpl {
 public:
  explicit TestWebKitClient(bool unit_test_mode);
  virtual ~TestWebKitClient();

  virtual WebKit::WebMimeRegistry* mimeRegistry();
  WebKit::WebClipboard* clipboard();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual WebKit::WebCookieJar* cookieJar();
  virtual bool sandboxEnabled();
  virtual WebKit::WebKitClient::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name);
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual WebKit::WebURLLoader* createURLLoader();
  virtual WebKit::WebData loadResource(const char* name);
  virtual WebKit::WebString defaultLocale();
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota);

  void dispatchStorageEvent(const WebKit::WebString& key,
      const WebKit::WebString& old_value, const WebKit::WebString& new_value,
      const WebKit::WebString& origin, const WebKit::WebURL& url,
      bool is_local_storage);

#if defined(OS_WIN)
  void SetThemeEngine(WebKit::WebThemeEngine* engine);
  virtual WebKit::WebThemeEngine *themeEngine();
#endif

  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();
  virtual WebKit::WebGraphicsContext3D* createGraphicsContext3D();

  WebURLLoaderMockFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

 private:
  TestShellWebMimeRegistryImpl mime_registry_;
  MockWebClipboardImpl mock_clipboard_;
  webkit_glue::WebFileSystemImpl file_system_;
  ScopedTempDir appcache_dir_;
  SimpleAppCacheSystem appcache_system_;
  SimpleDatabaseSystem database_system_;
  SimpleWebCookieJarImpl cookie_jar_;
  WebURLLoaderMockFactory url_loader_factory_;
  bool unit_test_mode_;

#if defined(OS_WIN)
  WebKit::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitClient);
};

#endif  // WEBKIT_SUPPORT_TEST_WEBKIT_CLIENT_H_
