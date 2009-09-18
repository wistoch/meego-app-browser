// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/execute_code_in_tab_function.h"

#include "base/thread.h"
#include "base/file_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace keys = extension_tabs_module_constants;

const wchar_t* kCodeKey = L"code";
const wchar_t* kFileKey = L"file";

bool ExecuteCodeInTabFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);

  DictionaryValue* script_info;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &script_info));
  size_t number_of_value = script_info->GetSize();
  if (number_of_value == 0) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  } else if (number_of_value > 1) {
    error_ = keys::kMoreThanOneValuesError;
    return false;
  }

  execute_tab_id_ = -1;
  Browser* browser = NULL;
  TabContents* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  Value* tab_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args->Get(0, &tab_value));
  if (tab_value->IsType(Value::TYPE_NULL)) {
    browser = dispatcher()->GetBrowser();
    if (!browser) {
      error_ = keys::kNoCurrentWindowError;
      return false;
    }
    if (!ExtensionTabUtil::GetDefaultTab(browser, &contents, &execute_tab_id_))
      return false;
  } else {
    EXTENSION_FUNCTION_VALIDATE(tab_value->GetAsInteger(&execute_tab_id_));
    if (!ExtensionTabUtil::GetTabById(execute_tab_id_, profile(), &browser,
                                      NULL, &contents, NULL)) {
      return false;
    }
  }

  DCHECK(browser);
  DCHECK(contents);

  if (!GetExtension()->CanAccessHost(contents->GetURL())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kCannotAccessPageError, contents->GetURL().spec());
    return false;
  }

  if (script_info->HasKey(kCodeKey)) {
    if (!script_info->GetString(kCodeKey, &code_string_))
      return false;
  }

  if (!code_string_.empty()) {
    Execute();
    return true;
  }

  std::string relative_path;
  if (script_info->HasKey(kFileKey)) {
    if (!script_info->GetString(kFileKey, &relative_path))
      return false;
    file_path_ = GetExtension()->GetResourcePath(relative_path);
  }
  if (file_path_.empty()) {
    error_ = keys::kNoCodeOrFileToExecuteError;
    return false;
  }

  ui_loop_ = MessageLoop::current();
  MessageLoop* work_loop = g_browser_process->file_thread()->message_loop();
  work_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ExecuteCodeInTabFunction::LoadFile));

  return true;
}

void ExecuteCodeInTabFunction::LoadFile() {
  DCHECK(ui_loop_);
  std::string content;
  if (!file_util::ReadFileToString(file_path_, &content)) {

#if defined(OS_POSIX)
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kLoadFileError,
        file_path_.value());
#elif defined(OS_WIN)
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kLoadFileError,
        WideToUTF8(file_path_.value()));
#endif  // OS_WIN

    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ExecuteCodeInTabFunction::SendResponse,
                          false));
  } else {
    code_string_ = content;
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ExecuteCodeInTabFunction::Execute));
  }

  return;
}

void ExecuteCodeInTabFunction::Execute() {
  TabContents* contents = NULL;
  Browser* browser = NULL;
  if (!ExtensionTabUtil::GetTabById(execute_tab_id_, profile(), &browser, NULL,
                                    &contents, NULL) && contents && browser) {
    SendResponse(false);
    return;
  }

  bool is_js_code = true;
  std::string function_name = name();
  if (function_name == keys::kInsertCSSFunction) {
    is_js_code = false;
  } else if (function_name != keys::kExecuteScriptFunction) {
    DCHECK(false);
  }
  registrar_.Add(this, NotificationType::TAB_CODE_EXECUTED,
                 NotificationService::AllSources());
  AddRef();  // balanced in Observe()
  contents->ExecuteCode(request_id(), extension_id(), is_js_code,
                        code_string_);
}

void ExecuteCodeInTabFunction::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  std::pair<int, bool>* result_details =
      Details<std::pair<int, bool> >(details).ptr();
  if (result_details->first == request_id()) {
    SendResponse(result_details->second);
    Release();  // balanced in Execute()
  }
}
