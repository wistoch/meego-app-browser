// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"

namespace platform_util {

// TODO(estade): It would be nice to be able to select the file in the file
// manager, but that probably requires extending xdg-open. For now just
// show the folder.
void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  if (!file_util::DirectoryExists(dir))
    return;

  std::vector<std::string> argv;
  argv.push_back("xdg-open");
  argv.push_back(dir.value());
  base::file_handle_mapping_vector no_files;
  base::LaunchApp(argv, no_files, false, NULL);
}

}  // namespace platform_util
