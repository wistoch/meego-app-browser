// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include <algorithm>

#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "grit/bookmark_manager_resources_map.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"

namespace {

class URLRequestResourceBundleJob : public URLRequestSimpleJob {
 public:
  explicit URLRequestResourceBundleJob(URLRequest* request,
      const FilePath& filename, int resource_id)
          : URLRequestSimpleJob(request),
            filename_(filename),
            resource_id_(resource_id) { }

  // URLRequestSimpleJob method.
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.GetRawDataResource(resource_id_).as_string();
    bool result = net::GetMimeTypeFromFile(filename_, mime_type);
    if (StartsWithASCII(*mime_type, "text/", false)) {
      // All of our HTML files should be UTF-8 and for other resource types
      // (like images), charset doesn't matter.
      DCHECK(IsStringUTF8(*data));
      *charset = "utf-8";
    }
    return result;
  }

 private:
  virtual ~URLRequestResourceBundleJob() { }

  // We need the filename of the resource to determine the mime type.
  FilePath filename_;

  // The resource bundle id to load.
  int resource_id_;
};

}  // namespace

// Factory registered with URLRequest to create URLRequestJobs for extension://
// URLs.
static URLRequestJob* CreateExtensionURLRequestJob(URLRequest* request,
                                                   const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // Don't allow toplevel navigations to extension resources in incognito mode.
  // This is because an extension must run in a single process, and an incognito
  // tab prevents that.
  // TODO(mpcomplete): better error code.
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  if (context->is_off_the_record() &&
      info && info->resource_type() == ResourceType::MAIN_FRAME)
    return new URLRequestErrorJob(request, net::ERR_ADDRESS_UNREACHABLE);

  // chrome-extension://extension-id/resource/path.js
  const std::string& extension_id = request->url().host();
  FilePath directory_path = context->GetPathForExtension(extension_id);
  if (directory_path.value().empty()) {
    LOG(WARNING) << "Failed to GetPathForExtension: " << extension_id;
    return NULL;
  }

  FilePath resources_path;
  if (PathService::Get(chrome::DIR_RESOURCES, &resources_path) &&
      directory_path.DirName() == resources_path) {
    FilePath relative_path = directory_path.BaseName().Append(
        extension_file_util::ExtensionURLToRelativeFilePath(request->url()));
#if defined(OS_WIN)
    // TODO(tc): This is a hack, we should normalize paths another way.
    FilePath::StringType path = relative_path.value();
    std::replace(path.begin(), path.end(), '\\', '/');
    relative_path = FilePath(path);
#endif

    // TODO(tc): Make a map of FilePath -> resource ids so we don't have to
    // covert to FilePaths all the time.  This will be more useful as we add
    // more resources.
    for (size_t i = 0; i < kBookmarkManagerResourcesSize; ++i) {
      FilePath bm_resource_path =
          FilePath().AppendASCII(kBookmarkManagerResources[i].name);
      if (relative_path == bm_resource_path) {
        return new URLRequestResourceBundleJob(request, relative_path,
            kBookmarkManagerResources[i].value);
      }
    }
  }
  // TODO(tc): Move all of these files into resources.pak so we don't break
  // when updating on Linux.
  ExtensionResource resource(extension_id, directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new URLRequestFileJob(request,
                               resource.GetFilePathOnAnyThreadHack());
}

// Factory registered with URLRequest to create URLRequestJobs for
// chrome-user-script:/ URLs.
static URLRequestJob* CreateUserScriptURLRequestJob(URLRequest* request,
                                                    const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // chrome-user-script:/user-script-name.user.js
  FilePath directory_path = context->user_script_dir_path();

  ExtensionResource resource(request->url().host(), directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new URLRequestFileJob(request, resource.GetFilePath());
}

void RegisterExtensionProtocols() {
  URLRequest::RegisterProtocolFactory(chrome::kExtensionScheme,
                                      &CreateExtensionURLRequestJob);
  URLRequest::RegisterProtocolFactory(chrome::kUserScriptScheme,
                                      &CreateUserScriptURLRequestJob);
}
