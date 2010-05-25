// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "grit/webkit_chromium_resources.h"
#include "grit/webkit_resources.h"

#define MAX_LOADSTRING 100

namespace {

FilePath GetResourceFilePath(const char* ascii_name) {
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.AppendASCII("DumpRenderTree_resources");
  return path.AppendASCII(ascii_name);
}

base::StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size)
      ? base::StringPiece(static_cast<char*>(data_ptr), data_size)
      : base::StringPiece();
}

base::StringPiece NetResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

}  // namespace

namespace webkit_support {

// TODO(tkent): Implement some of the followings for platform-dependent tasks
// such as loading resource.

void BeforeInitialize() {
}

void AfterInitialize() {
}

void BeforeShutdown() {
}

void AfterShutdown() {
}

}  // namespace webkit_support

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  wchar_t localized[MAX_LOADSTRING];
  int length = ::LoadString(::GetModuleHandle(NULL), message_id,
                            localized, MAX_LOADSTRING);
  if (!length && ::GetLastError() == ERROR_RESOURCE_NAME_NOT_FOUND) {
    NOTREACHED();
    return L"No string for this identifier!";
  }
  return string16(localized, length);
}

base::StringPiece GetDataResource(int resource_id) {
  switch (resource_id) {
  case IDR_BROKENIMAGE: {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      FilePath path = GetResourceFilePath("missingImage.gif");
      bool success = file_util::ReadFileToString(path, &broken_image_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return broken_image_data;
  }
  case IDR_TEXTAREA_RESIZER: {
    // Use webkit's text area resizer image.
    static std::string resize_corner_data;
    if (resize_corner_data.empty()) {
      FilePath path = GetResourceFilePath("textAreaResizeCorner.png");
      bool success = file_util::ReadFileToString(path, &resize_corner_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return resize_corner_data;
  }

  case IDR_SEARCH_CANCEL:
  case IDR_SEARCH_CANCEL_PRESSED:
  case IDR_SEARCH_MAGNIFIER:
  case IDR_SEARCH_MAGNIFIER_RESULTS:
  case IDR_MEDIA_PAUSE_BUTTON:
  case IDR_MEDIA_PLAY_BUTTON:
  case IDR_MEDIA_PLAY_BUTTON_DISABLED:
  case IDR_MEDIA_SOUND_FULL_BUTTON:
  case IDR_MEDIA_SOUND_NONE_BUTTON:
  case IDR_MEDIA_SOUND_DISABLED:
  case IDR_MEDIA_SLIDER_THUMB:
  case IDR_MEDIA_VOLUME_SLIDER_THUMB:
  case IDR_DEVTOOLS_INJECT_WEBKIT_JS:
  case IDR_DEVTOOLS_INJECT_DISPATCH_JS:
    return NetResourceProvider(resource_id);

  default:
    break;
  }

  return base::StringPiece();
}

}  // namespace webkit_glue
