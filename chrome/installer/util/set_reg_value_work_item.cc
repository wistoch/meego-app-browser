// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/set_reg_value_work_item.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/logging_installer.h"

SetRegValueWorkItem::~SetRegValueWorkItem() {
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         const std::wstring& value_name,
                                         const std::wstring& value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      value_data_str_(value_data),
      value_data_dword_(0),
      overwrite_(overwrite),
      status_(SET_VALUE),
      is_str_type_(true),
      previous_value_dword_(0) {
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         const std::wstring& value_name,
                                         DWORD value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      value_data_dword_(value_data),
      overwrite_(overwrite),
      status_(SET_VALUE),
      is_str_type_(false),
      previous_value_dword_(0) {
}

bool SetRegValueWorkItem::Do() {
  bool success = true;

  if (status_ != SET_VALUE) {
    // we already did something.
    LOG(ERROR) << "multiple calls to Do()";
    success = false;
  }

  base::win::RegKey key;
  if (success && !key.Open(predefined_root_, key_path_.c_str(),
                           KEY_READ | KEY_SET_VALUE)) {
    LOG(ERROR) << "can not open " << key_path_;
    status_ = VALUE_UNCHANGED;
    success = false;
  }

  if (success) {
    if (key.ValueExists(value_name_.c_str())) {
      if (overwrite_) {
        // Read previous value for rollback and write new value
        if (is_str_type_) {
          std::wstring data;
          if (key.ReadValue(value_name_.c_str(), &data)) {
            previous_value_str_.assign(data);
          }
          success = key.WriteValue(value_name_.c_str(),
                                   value_data_str_.c_str());
        } else {
          DWORD data;
          if (key.ReadValueDW(value_name_.c_str(), &data)) {
            previous_value_dword_ = data;
          }
          success = key.WriteValue(value_name_.c_str(), value_data_dword_);
        }
        if (success) {
          VLOG(1) << "overwritten value for " << value_name_;
          status_ = VALUE_OVERWRITTEN;
        } else {
          LOG(ERROR) << "failed to overwrite value for " << value_name_;
          status_ = VALUE_UNCHANGED;
        }
      } else {
        VLOG(1) << value_name_ << " exists. not changed ";
        status_ = VALUE_UNCHANGED;
      }
    } else {
      if (is_str_type_) {
        success = key.WriteValue(value_name_.c_str(), value_data_str_.c_str());
      } else {
        success = key.WriteValue(value_name_.c_str(), value_data_dword_);
      }
      if (success) {
        VLOG(1) << "created value for " << value_name_;
        status_ = NEW_VALUE_CREATED;
      } else {
        LOG(ERROR) << "failed to create value for " << value_name_;
        status_ = VALUE_UNCHANGED;
      }
    }
  }

  LOG_IF(ERROR, !success && !log_message_.empty()) << log_message_;

  if (ignore_failure_) {
    success = true;
  }

  return success;
}

void SetRegValueWorkItem::Rollback() {
  if (!ignore_failure_) {
    if (status_ == SET_VALUE || status_ == VALUE_ROLL_BACK)
      return;

    if (status_ == VALUE_UNCHANGED) {
      status_ = VALUE_ROLL_BACK;
      VLOG(1) << "rollback: setting unchanged, nothing to do";
      return;
    }

    base::win::RegKey key;
    if (!key.Open(predefined_root_, key_path_.c_str(), KEY_SET_VALUE)) {
      status_ = VALUE_ROLL_BACK;
      VLOG(1) << "rollback: can not open " << key_path_;
      return;
    }

    std::wstring result_str(L" failed");
    if (status_ == NEW_VALUE_CREATED) {
      if (key.DeleteValue(value_name_.c_str()))
        result_str.assign(L" succeeded");
      VLOG(1) << "rollback: deleting " << value_name_ << result_str;
    } else if (status_ == VALUE_OVERWRITTEN) {
      // try restore the previous value
      bool success = true;
      if (is_str_type_) {
        success = key.WriteValue(value_name_.c_str(),
                                 previous_value_str_.c_str());
      } else {
        success = key.WriteValue(value_name_.c_str(), previous_value_dword_);
      }
      if (success)
        result_str.assign(L" succeeded");
      VLOG(1) << "rollback: restoring " << value_name_ << result_str;
    } else {
      NOTREACHED();
    }

    status_ = VALUE_ROLL_BACK;
    return;
  }
}
