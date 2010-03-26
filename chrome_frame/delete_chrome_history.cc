// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of DeleteChromeHistory
#include "chrome_frame/delete_chrome_history.h"

#include "chrome_frame/chrome_frame_activex.h"
#include "chrome/browser/browsing_data_remover.h"
#include "utils.h"

// Below other header to avoid symbol pollution.
#define INITGUID
#include <deletebrowsinghistory.h>

DeleteChromeHistory::DeleteChromeHistory()
  : remove_mask_(0) {
  DLOG(INFO) << __FUNCTION__;
}

DeleteChromeHistory::~DeleteChromeHistory() {
}


HRESULT DeleteChromeHistory::FinalConstruct() {
  DLOG(INFO) << __FUNCTION__;
  Initialize();
  return S_OK;
}

void DeleteChromeHistory::OnAutomationServerReady() {
  DLOG(INFO) << __FUNCTION__;
  automation_client_->RemoveBrowsingData(remove_mask_);
  loop_.Quit();
}

void DeleteChromeHistory::OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {
  DLOG(WARNING) << __FUNCTION__;
  loop_.Quit();
}

void DeleteChromeHistory::GetProfilePath(const std::wstring& profile_name,
                                         FilePath* profile_path) {
  ChromeFramePlugin::GetProfilePath(kIexploreProfileName, profile_path);
}

STDMETHODIMP DeleteChromeHistory::DeleteBrowsingHistory(DWORD flags) {
  DLOG(INFO) << __FUNCTION__;
  // Usually called inside a quick startup/tear-down routine by RunDLL32. You
  // can simulate the process by calling:
  //    RunDll32.exe InetCpl.cpl,ClearMyTracksByProcess 255
  // Since automation setup isn't synchronous, we can be tearing down while
  // being only partially set-up, causing even synchronous IPCs to be dropped.
  // Since the *Chrome* startup/tear-down occurs synchronously from the
  // perspective of automation, we can add a flag to the chrome.exe invocation
  // in lieu of sending an IPC when it seems appropriate. Since we assume this
  // happens in one-off fashion, don't attempt to pack REMOVE_* arguments.
  // Instead, have the browser process clobber all history.
  if (!InitializeAutomation(GetHostProcessName(false), L"", false, false)) {
    return E_UNEXPECTED;
  }

  if (flags & DELETE_BROWSING_HISTORY_COOKIES)
    remove_mask_ |= BrowsingDataRemover::REMOVE_COOKIES;
  if (flags & DELETE_BROWSING_HISTORY_TIF)
    remove_mask_ |= BrowsingDataRemover::REMOVE_CACHE;
  if (flags & DELETE_BROWSING_HISTORY_FORMDATA)
    remove_mask_ |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (flags & DELETE_BROWSING_HISTORY_PASSWORDS)
    remove_mask_ |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (flags & DELETE_BROWSING_HISTORY_HISTORY)
    remove_mask_ |= BrowsingDataRemover::REMOVE_HISTORY;

  loop_.PostDelayedTask(FROM_HERE,
      new MessageLoop::QuitTask, 1000 * 600);
  loop_.MessageLoop::Run();

  return S_OK;
}


