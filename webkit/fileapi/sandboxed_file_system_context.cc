// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandboxed_file_system_context.h"

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"

namespace fileapi {

SandboxedFileSystemContext::SandboxedFileSystemContext(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path,
    bool is_incognito,
    bool allow_file_access,
    bool unlimited_quota)
    : file_message_loop_(file_message_loop),
      path_manager_(new FileSystemPathManager(
          file_message_loop, profile_path, is_incognito, allow_file_access)),
      quota_manager_(new FileSystemQuotaManager(
          allow_file_access, unlimited_quota)) {
}

SandboxedFileSystemContext::~SandboxedFileSystemContext() {
}

void SandboxedFileSystemContext::Shutdown() {
  path_manager_.reset();
  quota_manager_.reset();
}

void SandboxedFileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(path_manager_.get());
  DCHECK(file_message_loop_->BelongsToCurrentThread());

  std::string storage_identifier =
      FileSystemPathManager::GetStorageIdentifierFromURL(origin_url);
  FilePath path_for_origin = path_manager_->base_path().AppendASCII(
      storage_identifier);

  file_util::Delete(path_for_origin, true /* recursive */);
}

}  // namespace fileapi
