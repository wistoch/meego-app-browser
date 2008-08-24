// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_plugin_util.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

//
// ScopableCPRequest
//

ScopableCPRequest::ScopableCPRequest(const char* u, const char* m,
                                     CPBrowsingContext c) {
  pdata = NULL;
  data = NULL;
  url = _strdup(u);
  method = _strdup(m);
  context = c;
}

ScopableCPRequest::~ScopableCPRequest() {
  pdata = NULL;
  data = NULL;
  free(const_cast<char*>(url));
  free(const_cast<char*>(method));
}

//
// PluginHelper
//

// static
void PluginHelper::DestroyAllHelpersForPlugin(ChromePluginLib* plugin) {
  NotificationService::current()->Notify(
      NOTIFY_CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin),
      NotificationService::NoDetails());
}

PluginHelper::PluginHelper(ChromePluginLib* plugin) : plugin_(plugin) {
#ifndef NDEBUG
  message_loop_ = MessageLoop::current();
#endif
  NotificationService::current()->AddObserver(
      this, NOTIFY_CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin_));
}

PluginHelper::~PluginHelper() {
#ifndef NDEBUG
  DCHECK(MessageLoop::current() == message_loop_);
#endif
  NotificationService::current()->RemoveObserver(
      this, NOTIFY_CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin_));
}

void PluginHelper::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
#ifndef NDEBUG
  DCHECK(MessageLoop::current() == message_loop_);
#endif
  DCHECK(type == NOTIFY_CHROME_PLUGIN_UNLOADED);
  DCHECK(plugin_ == Source<ChromePluginLib>(source).ptr());

  delete this;
}

//
// PluginResponseUtils
//

uint32 PluginResponseUtils::CPLoadFlagsToNetFlags(uint32 flags) {
  uint32 net_flags = 0;
#define HANDLE_FLAG(name) \
  if (flags & CPREQUEST##name) \
  net_flags |= net::name

  HANDLE_FLAG(LOAD_VALIDATE_CACHE);
  HANDLE_FLAG(LOAD_BYPASS_CACHE);
  HANDLE_FLAG(LOAD_PREFERRING_CACHE);
  HANDLE_FLAG(LOAD_ONLY_FROM_CACHE);
  HANDLE_FLAG(LOAD_DISABLE_CACHE);
  HANDLE_FLAG(LOAD_DISABLE_INTERCEPT);

  net_flags |= net::LOAD_ENABLE_UPLOAD_PROGRESS;

  return net_flags;
}

int PluginResponseUtils::GetResponseInfo(
    const net::HttpResponseHeaders* response_headers,
    CPResponseInfoType type, void* buf, uint32 buf_size) {
  if (!response_headers)
    return CPERR_FAILURE;

  switch (type) {
  case CPRESPONSEINFO_HTTP_STATUS:
    if (buf && buf_size) {
      int status = response_headers->response_code();
      memcpy(buf, &status, std::min(buf_size, sizeof(int)));
    }
    break;
  case CPRESPONSEINFO_HTTP_RAW_HEADERS: {
    const std::string& headers = response_headers->raw_headers();
    if (buf_size < headers.size()+1)
      return static_cast<int>(headers.size()+1);
    if (buf)
      memcpy(buf, headers.c_str(), headers.size()+1);
    break;
    }
  default:
    return CPERR_INVALID_VERSION;
  }

  return CPERR_SUCCESS;
}

CPError CPB_GetCommandLineArgumentsCommon(const char* url,
                                          std::string* arguments) {
  CommandLine cmd;
  std::wstring arguments_w;

  // Use the same UserDataDir for new launches that we currently have set.
  std::wstring user_data_dir = cmd.GetSwitchValue(switches::kUserDataDir);
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    wchar_t user_data_dir_full[MAX_PATH];
    if (_wfullpath(user_data_dir_full, user_data_dir.c_str(), MAX_PATH) &&
        file_util::PathExists(user_data_dir_full)) {
      CommandLine::AppendSwitchWithValue(
          &arguments_w, switches::kUserDataDir, user_data_dir_full);
    }
  }

  // Use '--app=url' instead of just 'url' to launch the browser with minimal
  // chrome.
  // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
  std::wstring url_w = UTF8ToWide(url);
  CommandLine::AppendSwitchWithValue(&arguments_w, switches::kApp, url_w);

  *arguments = WideToUTF8(arguments_w);

  return CPERR_SUCCESS;
}

//
// Host functions shared by browser and plugin processes
//

void* STDCALL CPB_Alloc(uint32 size) {
  return malloc(size);
}

void STDCALL CPB_Free(void* memory) {
  free(memory);
}

