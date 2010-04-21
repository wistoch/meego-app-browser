// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/plugins/plugin_list.h"

// Functions needed by webkit_glue.

namespace webkit_glue {

void GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool IsDefaultPluginEnabled() {
  return false;
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

void AppendToLog(const char*, int, const char*) {
}

bool GetApplicationDirectory(FilePath* path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool GetExeDirectory(FilePath* path) {
  return GetApplicationDirectory(path);
}

bool IsProtocolSupportedForMedia(const GURL& url) {
  if (url.SchemeIsFile() ||
      url.SchemeIs("http") ||
      url.SchemeIs("https") ||
      url.SchemeIs("data"))
    return true;
  return false;
}

string16 GetLocalizedString(int message_id) {
  // TODO(tkent): implement this.
  return string16();
}

base::StringPiece GetDataResource(int resource_id) {
  // TODO(tkent): implement this.
  return "";
}

std::string GetProductVersion() {
  return std::string("DumpRenderTree/0.0.0.0");
}

}  // namespace webkit_glue
