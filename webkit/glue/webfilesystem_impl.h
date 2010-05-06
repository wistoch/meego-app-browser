// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBFILESYSTEM_IMPL_H_
#define WEBFILESYSTEM_IMPL_H_

#include "base/platform_file.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"

namespace webkit_glue {

class WebFileSystemImpl : public WebKit::WebFileSystem {
 public:
  WebFileSystemImpl();
  virtual ~WebFileSystemImpl() { }

  // WebFileSystem methods:
  virtual bool fileExists(const WebKit::WebString& path);
  virtual bool deleteFile(const WebKit::WebString& path);
  virtual bool deleteEmptyDirectory(const WebKit::WebString& path);
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual bool getFileModificationTime(
      const WebKit::WebString& path,
      double& result);
  virtual WebKit::WebString directoryName(const WebKit::WebString& path);
  virtual WebKit::WebString pathByAppendingComponent(
      const WebKit::WebString& path, const WebKit::WebString& component);
  virtual bool makeAllDirectories(const WebKit::WebString& path);
  virtual WebKit::WebString getAbsolutePath(const WebKit::WebString& path);
  virtual bool isDirectory(const WebKit::WebString& path);
  virtual WebKit::WebURL filePathToURL(const WebKit::WebString& path);
  virtual base::PlatformFile openFile(const WebKit::WebString& path, int mode);
  virtual void closeFile(base::PlatformFile& handle);
  virtual long long seekFile(base::PlatformFile handle,
                             long long offset,
                             int origin);
  virtual bool truncateFile(base::PlatformFile handle, long long offset);
  virtual int readFromFile(base::PlatformFile handle, char* data, int length);
  virtual int writeToFile(base::PlatformFile handle,
                          const char* data,
                          int length);

  void set_sandbox_enabled(bool sandbox_enabled) {
    sandbox_enabled_ = sandbox_enabled;
  }

 protected:
  bool sandbox_enabled_;
};

}  // namespace webkit_glue

#endif  // WEBFILESYSTEM_IMPL_H_
