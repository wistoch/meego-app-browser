// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_URL_REQUEST_INFO_DEV_H_
#define PPAPI_CPP_DEV_URL_REQUEST_INFO_DEV_H_

#include "ppapi/c/dev/ppb_url_request_info_dev.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class FileRef_Dev;

class URLRequestInfo_Dev : public Resource {
 public:
  URLRequestInfo_Dev();
  URLRequestInfo_Dev(const URLRequestInfo_Dev& other);

  URLRequestInfo_Dev& operator=(const URLRequestInfo_Dev& other);
  void swap(URLRequestInfo_Dev& other);

  // PPB_URLRequestInfo_Dev methods:
  bool SetProperty(PP_URLRequestProperty_Dev property, const Var& value);
  bool AppendDataToBody(const char* data, uint32_t len);
  bool AppendFileToBody(const FileRef_Dev& file_ref,
                        PP_Time expected_last_modified_time = 0);
  bool AppendFileRangeToBody(const FileRef_Dev& file_ref,
                             int64_t start_offset,
                             int64_t length,
                             PP_Time expected_last_modified_time = 0);

  // Convenient helpers for setting properties:
  bool SetURL(const Var& url_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_URL, url_string);
  }
  bool SetMethod(const Var& method_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_METHOD, method_string);
  }
  bool SetHeaders(const Var& headers_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_HEADERS, headers_string);
  }
  bool SetStreamToFile(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, enable);
  }
  bool SetFollowRedirects(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, enable);
  }
  bool SetRecordUploadProgress(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, enable);
  }
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_URL_REQUEST_INFO_DEV_H_
