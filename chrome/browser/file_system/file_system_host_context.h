// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "webkit/fileapi/file_system_quota.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

// This is owned by profile and shared by all the FileSystemDispatcherHost
// that shared by the same profile.
class FileSystemHostContext
    : public base::RefCountedThreadSafe<FileSystemHostContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  FileSystemHostContext(const FilePath& data_path, bool is_incognito);
  const FilePath& base_path() const { return base_path_; }
  bool is_incognito() const { return is_incognito_; }

  // Returns the root path and name for the file system specified by given
  // |origin_url| and |type|.  Returns true if the file system is available
  // for the profile and |root_path| and |name| are filled successfully.
  bool GetFileSystemRootPath(const GURL& origin_url,
                             fileapi::FileSystemType type,
                             FilePath* root_path,
                             std::string* name) const;

  // Check if the given |path| is in the FileSystem base directory.
  bool CheckValidFileSystemPath(const FilePath& path) const;

  // Retrieves the origin URL for the given |path| and populates
  // |origin_url|.  It returns false when the given |path| is not a
  // valid filesystem path.
  bool GetOriginFromPath(const FilePath& path, GURL* origin_url);

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Quota related methods.
  bool CheckOriginQuota(const GURL& url, int64 growth);
  void SetOriginQuotaUnlimited(const GURL& url);
  void ResetOriginQuotaUnlimited(const GURL& url);

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  static const char kPersistentName[];
  static const char kTemporaryName[];

 private:
  // Returns the storage identifier string for the given |url|.
  static std::string GetStorageIdentifierFromURL(const GURL& url);

  const FilePath base_path_;
  const bool is_incognito_;
  bool allow_file_access_from_files_;

  scoped_ptr<fileapi::FileSystemQuota> quota_manager_;

  FRIEND_TEST_ALL_PREFIXES(FileSystemHostContextTest, GetOriginFromPath);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemHostContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
