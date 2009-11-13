// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_util.h"

#include "base/string_util.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/vfs_backend.h"

namespace webkit_database {

bool DatabaseUtil::CrackVfsFilePath(const string16& vfs_file_path,
                                    string16* origin_identifier,
                                    string16* database_name,
                                    string16* sqlite_suffix) {
  // 'vfs_file_path' is of the form <origin_identifier>/<db_name>#<suffix>.
  // <suffix> is optional.
  DCHECK(!vfs_file_path.empty());
  size_t first_slash_index = vfs_file_path.find('/');
  size_t last_pound_index = vfs_file_path.rfind('#');
  // '/' and '#' must be present in the string. Also, the string cannot start
  // with a '/' (origin_identifier cannot be empty) and '/' must come before '#'
  if ((first_slash_index == string16::npos) ||
      (last_pound_index == string16::npos) ||
      (first_slash_index == 0) ||
      (first_slash_index > last_pound_index)) {
    return false;
  }

  *origin_identifier = vfs_file_path.substr(0, first_slash_index);
  *database_name = vfs_file_path.substr(
      first_slash_index + 1, last_pound_index - first_slash_index - 1);
  *sqlite_suffix = vfs_file_path.substr(
      last_pound_index + 1, vfs_file_path.length() - last_pound_index - 1);
  return true;
}

FilePath DatabaseUtil::GetFullFilePathForVfsFile(
    DatabaseTracker* db_tracker, const string16& vfs_file_path) {
  string16 origin_identifier;
  string16 database_name;
  string16 sqlite_suffix;
  if (!CrackVfsFilePath(vfs_file_path, &origin_identifier,
                        &database_name, &sqlite_suffix)) {
    return FilePath(); // invalid vfs_file_name
  }

  FilePath full_path = db_tracker->GetFullDBFilePath(
      origin_identifier, database_name);
  if (!full_path.empty() && !sqlite_suffix.empty()) {
    full_path = FilePath::FromWStringHack(
        full_path.ToWStringHack() + UTF16ToWide(sqlite_suffix));
  }
  return full_path;
}

}  // namespace webkit_database
