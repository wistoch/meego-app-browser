// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tab API implementation.

#ifndef CEEE_IE_BROKER_TAB_API_MODULE_H_
#define CEEE_IE_BROKER_TAB_API_MODULE_H_

#include <string>

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/common_api_module.h"

#include "toolband.h"  //NOLINT

namespace tab_api {

class TabApiResult;
typedef ApiResultCreator<TabApiResult> TabApiResultCreator;

// Registers all Tab API Invocations with the given dispatcher.
void RegisterInvocations(ApiDispatcher* dispatcher);

class TabApiResult : public common_api::CommonApiResult {
 public:
  explicit TabApiResult(int request_id)
      : common_api::CommonApiResult(request_id) {
  }

  // Retrieves the frame window to use from the arguments provided, or the
  // current frame window if none was specified in arguments.
  // Will set @p specified to true if the window was specified and @p specified
  // isn't NULL. It can be NULL if caller doesn't need to know.
  virtual HWND GetSpecifiedOrCurrentFrameWindow(const Value& args,
                                                bool* specified);

  // Creates a value object with the information for a tab, as specified
  // by the API definition.
  // @param tab_id The ID of the tab we want the create a value for.
  // @param index The index of the tab if already known, -1 otherwise.
  // @return true for success, false for failure and will also call
  //    ApiDispatcher::InvocationResult::PostError on failures.
  virtual bool CreateTabValue(int tab_id, long index);

  // Check if saved_dict has a specified frame window and if so compares it
  // to the frame window that is contained in the given input_dict or the
  // grand parent of the tab window found in the input_dict and returned in
  // tab_window.
  static bool IsTabFromSameOrUnspecifiedFrameWindow(
      const DictionaryValue& input_dict,
      const Value* saved_window_value,
      HWND* tab_window,
      ApiDispatcher* dispatcher);
};

class GetTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class GetSelectedTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
};

class GetAllTabsInWindowResult : public TabApiResult {
 public:
  explicit GetAllTabsInWindowResult(int request_id) : TabApiResult(request_id) {
  }

  // Populates the result with all tabs in the given JSON encoded list.
  virtual bool Execute(BSTR tab_handles);
};

class GetAllTabsInWindow
    : public ApiResultCreator<GetAllTabsInWindowResult> {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class UpdateTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class RemoveTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
};

class CreateTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  // We need to wait for the new tab to complete the result response.
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
  // And we also have a PermanentEventHandler too...
  // To properly convert and complete the event arguments.
  static bool EventHandler(const std::string& input_args,
                           std::string* converted_args,
                           ApiDispatcher* dispatcher);
};

class MoveTab : public TabApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class TabsInsertCode : public ApiResultCreator<> {
 protected:
  // Will return NULL after calling PostError on the result upon failure.
  ApiDispatcher::InvocationResult* ExecuteImpl(const ListValue& args,
                                               int request_id,
                                               CeeeTabCodeType type,
                                               int* tab_id,
                                               HRESULT* hr);
};

class TabsExecuteScript : public TabsInsertCode {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class TabsInsertCSS : public TabsInsertCode {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

// Helper class to handle the data cleanup.
class TabInfo : public CeeeTabInfo {
 public:
  TabInfo();
  ~TabInfo();
  // Useful for reuse in unit tests.
  void Clear();
};

}  // namespace tab_api

#endif  // CEEE_IE_BROKER_TAB_API_MODULE_H_
