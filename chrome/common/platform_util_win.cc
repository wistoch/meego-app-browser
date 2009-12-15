// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include "app/gfx/native_widget_types.h"
#include "app/win_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  // ParseDisplayName will fail if the directory is "C:", it must be "C:\\".
  if (dir.value() == L"" || !file_util::EnsureEndsWithSeparator(&dir))
    return;

  typedef HRESULT (WINAPI *SHOpenFolderAndSelectItemsFuncPtr)(
      PCIDLIST_ABSOLUTE pidl_Folder,
      UINT cidl,
      PCUITEMID_CHILD_ARRAY pidls,
      DWORD flags);

  static SHOpenFolderAndSelectItemsFuncPtr open_folder_and_select_itemsPtr =
    NULL;
  static bool initialize_open_folder_proc = true;
  if (initialize_open_folder_proc) {
    initialize_open_folder_proc = false;
    // The SHOpenFolderAndSelectItems API is exposed by shell32 version 6
    // and does not exist in Win2K. We attempt to retrieve this function export
    // from shell32 and if it does not exist, we just invoke ShellExecute to
    // open the folder thus losing the functionality to select the item in
    // the process.
    HMODULE shell32_base = GetModuleHandle(L"shell32.dll");
    if (!shell32_base) {
      NOTREACHED();
      return;
    }
    open_folder_and_select_itemsPtr =
        reinterpret_cast<SHOpenFolderAndSelectItemsFuncPtr>
            (GetProcAddress(shell32_base, "SHOpenFolderAndSelectItems"));
  }
  if (!open_folder_and_select_itemsPtr) {
    ShellExecute(NULL, _T("open"), dir.value().c_str(), NULL, NULL, SW_SHOW);
    return;
  }

  ScopedComPtr<IShellFolder> desktop;
  HRESULT hr = SHGetDesktopFolder(desktop.Receive());
  if (FAILED(hr))
    return;

  win_util::CoMemReleaser<ITEMIDLIST> dir_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
                                 const_cast<wchar_t *>(dir.value().c_str()),
                                 NULL, &dir_item, NULL);
  if (FAILED(hr))
    return;

  win_util::CoMemReleaser<ITEMIDLIST> file_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
      const_cast<wchar_t *>(full_path.value().c_str()),
      NULL, &file_item, NULL);
  if (FAILED(hr))
    return;

  const ITEMIDLIST* highlight[] = {
    {file_item},
  };
  (*open_folder_and_select_itemsPtr)(dir_item, arraysize(highlight),
                                     highlight, NULL);
}

void OpenItem(const FilePath& full_path) {
  win_util::OpenItemViaShell(full_path);
}

void OpenExternal(const GURL& url) {
  // Quote the input scheme to be sure that the command does not have
  // parameters unexpected by the external program. This url should already
  // have been escaped.
  std::string escaped_url = url.spec();
  escaped_url.insert(0, "\"");
  escaped_url += "\"";

  // According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
  // "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
  // ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
  // support URLS of 2083 chars in length, 2K is safe."
  const size_t kMaxUrlLength = 2048;
  if (escaped_url.length() > kMaxUrlLength) {
    NOTREACHED();
    return;
  }

  RegKey key;
  std::wstring registry_path = ASCIIToWide(url.scheme()) +
                               L"\\shell\\open\\command";
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str());
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size);
    if (size <= 2) {
      // ShellExecute crashes the process when the command is empty.
      // We check for "2" because it always returns the trailing NULL.
      // TODO(nsylvain): we should also add a dialog to warn on errors. See
      // bug 1136923.
      return;
    }
  }

  if (reinterpret_cast<ULONG_PTR>(ShellExecuteA(NULL, "open",
                                                escaped_url.c_str(), NULL, NULL,
                                                SW_SHOWNORMAL)) <= 32) {
    // We fail to execute the call. We could display a message to the user.
    // TODO(nsylvain): we should also add a dialog to warn on errors. See
    // bug 1136923.
    return;
  }
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return GetAncestor(view, GA_ROOT);
}

string16 GetWindowTitle(gfx::NativeWindow window_handle) {
  std::wstring result;
  int length = ::GetWindowTextLength(window_handle) + 1;
  ::GetWindowText(window_handle, WriteInto(&result, length), length);
  return WideToUTF16(result);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return ::GetForegroundWindow() == window;
}

bool IsVisible(gfx::NativeView view) {
  // MSVC complains if we don't include != 0.
  return ::IsWindowVisible(view) != 0;
}

void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  win_util::MessageBox(parent, message, title, MB_OK | MB_SETFOREGROUND);
}


namespace {
// Constants copied from src/tools/channel_changer/channel_changer.cc.

// The Google Update key to read to find out which branch you are on.
const wchar_t* const kChromeClientStateKey =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// The Google Client key to read to find out which branch you are on.
const wchar_t* const kChromeClientsKey =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// The Google Update value that defines which branch you are on.
const wchar_t* const kBranchKey = L"ap";

// The suffix Google Update sometimes adds to the channel name (channel names
// are defined in kBranchStrings), indicating that a full install is needed. We
// strip this out (if present) for the purpose of determining which channel you
// are on.
const wchar_t* const kChannelSuffix = L"-full";

// See DetectBranch() in src/tools/channel_changer/channel_changer.cc.
std::wstring CurrentChromeChannel() {
  std::wstring update_branch = L"stable";  // default if we get confused.

  // See if we can find the Clients key on the HKLM branch.
  HKEY registry_hive = HKEY_LOCAL_MACHINE;
  RegKey google_update_hklm(registry_hive, kChromeClientsKey, KEY_READ);
  if (!google_update_hklm.Valid()) {
    // HKLM failed us, try the same for the HKCU branch.
    registry_hive = HKEY_CURRENT_USER;
    RegKey google_update_hkcu(registry_hive, kChromeClientsKey, KEY_READ);
    if (!google_update_hkcu.Valid()) {
      // Unknown.
      registry_hive = 0;
    }
  }

  if (registry_hive != 0) {
    // Now that we know which hive to use, read the 'ap' key from it.
    RegKey client_state(registry_hive, kChromeClientStateKey, KEY_READ);
    client_state.ReadValue(kBranchKey, &update_branch);

    // We look for '1.1-beta' or '1.1-dev', but Google Update might have added
    // '-full' to the channel name, which we need to strip out to determine what
    // channel you are on.
    std::wstring suffix = kChannelSuffix;
    if (update_branch.length() > suffix.length()) {
      size_t index = update_branch.rfind(suffix);
      if (index != std::wstring::npos &&
          index == update_branch.length() - suffix.length()) {
        update_branch = update_branch.substr(0, index);
      }
    }
  }

  // Map to something pithy for human consumption.
  if ((update_branch == L"2.0-dev") ||(update_branch == L"1.1-dev"))
    update_branch = L"dev";
  else if (update_branch == L"1.1-beta")
    update_branch = L"beta";

  return update_branch;
}

}  // namespace

string16 GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  return CurrentChromeChannel();
#else
  return EmptyString16();
#endif
}

}  // namespace platform_util
