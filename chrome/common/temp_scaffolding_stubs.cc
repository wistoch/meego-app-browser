// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "build/build_config.h"

#include <vector>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/hung_renderer_dialog.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/process_watcher.h"

#if defined(OS_LINUX)
#include "chrome/browser/dock_info.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "views/controls/menu/chrome_menu.h"
#endif

class TabContents;

//--------------------------------------------------------------------------

void AutomationProvider::GetActiveWindow(int* handle) { NOTIMPLEMENTED(); }

void AutomationProvider::ActivateWindow(int handle) { NOTIMPLEMENTED(); }

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) { NOTIMPLEMENTED(); }

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
void AutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle,
    bool* success,
    int* autocomplete_edit_handle) {
  *success = false;
  NOTIMPLEMENTED();
}
#endif

void AutomationProvider::GetBrowserForWindow(int window_handle,
                                             bool* success,
                                             int* browser_handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetSecurityState(int handle, bool* success,
                                          SecurityStyle* security_style,
                                          int* ssl_cert_status,
                                          int* mixed_content_status) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetPageType(int handle, bool* success,
                                     NavigationEntry::PageType* page_type) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::ActionOnSSLBlockingPage(int handle, bool proceed,
                                                 IPC::Message* reply_message) {
  NOTIMPLEMENTED();
}

void AutomationProvider::PrintNow(int tab_handle,
                                  IPC::Message* reply_message) {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
void AutomationProvider::GetAutocompleteEditText(int autocomplete_edit_handle,
                                                 bool* success,
                                                 std::wstring* text) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::SetAutocompleteEditText(int autocomplete_edit_handle,
                                                 const std::wstring& text,
                                                 bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle,
    bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle,
    bool* success,
    bool* query_in_progress) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::OnMessageFromExternalHost(
    int handle, const std::string& message, const std::string& origin,
    const std::string& target) {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_MACOSX)

//--------------------------------------------------------------------------

#if defined(OS_LINUX)
bool ShellIntegration::SetAsDefaultBrowser() {
  // http://code.google.com/p/chromium/issues/detail?id=11972
  return true;
}

bool ShellIntegration::IsDefaultBrowser() {
  // http://code.google.com/p/chromium/issues/detail?id=11972
  return true;
}
#endif

//--------------------------------------------------------------------------


// static
bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        int* preference_details,
                                        std::vector<std::wstring>* new_tabs) {
  // http://code.google.com/p/chromium/issues/detail?id=11971
  // Pretend we processed them correctly.
  return true;
}

// static
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  // http://code.google.com/p/chromium/issues/detail?id=11971
  return 0;
}

bool FirstRun::CreateChromeDesktopShortcut() {
  NOTIMPLEMENTED();
  return false;
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool Upgrade::IsBrowserAlreadyRunning() {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return false;
}

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return true;
}

// static
bool Upgrade::SwapNewChromeExeIfPresent() {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return true;
}

// static
Upgrade::TryResult ShowTryChromeDialog() {
  return Upgrade::TD_NOT_NOW;
}

//--------------------------------------------------------------------------

void InstallJankometer(const CommandLine&) {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

void UninstallJankometer() {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

//--------------------------------------------------------------------------

void RLZTracker::CleanupRlz() {
  // http://code.google.com/p/chromium/issues/detail?id=8152
}

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

//--------------------------------------------------------------------------

void RunRepostFormWarningDialog(NavigationController*) {
}

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop) {
  NOTIMPLEMENTED();
  return NULL;
}

//--------------------------------------------------------------------------

MemoryDetails::MemoryDetails() {
  NOTIMPLEMENTED();
}

void MemoryDetails::StartFetch() {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

DownloadRequestDialogDelegate* DownloadRequestDialogDelegate::Create(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host) {
  NOTIMPLEMENTED();
  return NULL;
}

#if !defined(TOOLKIT_VIEWS)
namespace download_util {

void DragDownload(const DownloadItem* download, SkBitmap* icon) {
  NOTIMPLEMENTED();
}

}  // namespace download_util
#endif

#if defined(OS_MACOSX)
void HungRendererDialog::HideForTabContents(TabContents*) {
  NOTIMPLEMENTED();
}

void HungRendererDialog::ShowForTabContents(TabContents*) {
  NOTIMPLEMENTED();
}
#endif

#if !defined(TOOLKIT_VIEWS)
void BrowserList::AllBrowsersClosed() {
  // TODO(port): Close any dependent windows if necessary when the last browser
  //             window is closed.
}
#endif

//--------------------------------------------------------------------------

#if defined(OS_MACOSX)
void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  NOTIMPLEMENTED();
}
#endif

#if defined(OS_MACOSX)
bool DockInfo::GetNewWindowBounds(gfx::Rect* new_window_bounds,
                                  bool* maximize_new_window) const {
  NOTIMPLEMENTED();
  return true;
}

void DockInfo::AdjustOtherWindowBounds() const {
  NOTIMPLEMENTED();
}
#endif

//------------------------------------------------------------------------------

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
namespace views {

MenuItemView::MenuItemView(MenuDelegate* delegate) {
}

MenuItemView::~MenuItemView() {
}

MenuItemView* MenuItemView::AppendMenuItemInternal(int item_id,
                                                   const std::wstring& label,
                                                   const SkBitmap& icon,
                                                   Type type) {
  NOTIMPLEMENTED();
  return NULL;
}

void MenuItemView::RunMenuAt(gfx::NativeView parent,
               const gfx::Rect& bounds,
               AnchorPosition anchor,
               bool has_mnemonics) {
  NOTIMPLEMENTED();
}

void MenuItemView::RunMenuForDropAt(gfx::NativeView parent,
                      const gfx::Rect& bounds,
                      AnchorPosition anchor) {
  NOTIMPLEMENTED();
}

// Hides and cancels the menu. This does nothing if the menu is not open.
void MenuItemView::Cancel() {
  NOTIMPLEMENTED();
}

SubmenuView* MenuItemView::CreateSubmenu() {
  NOTIMPLEMENTED();
  return NULL;
}

void MenuItemView::SetSelected(bool selected) {
  NOTIMPLEMENTED();
}

void MenuItemView::SetIcon(const SkBitmap& icon, int item_id) {
  NOTIMPLEMENTED();
}

void MenuItemView::SetIcon(const SkBitmap& icon) {
  NOTIMPLEMENTED();
}

void MenuItemView::Paint(gfx::Canvas* canvas) {
  NOTIMPLEMENTED();
}

gfx::Size MenuItemView::GetPreferredSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

MenuController* MenuItemView::GetMenuController() {
  NOTIMPLEMENTED();
  return NULL;
}

MenuDelegate* MenuItemView::GetDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  NOTIMPLEMENTED();
  return NULL;
}

wchar_t MenuItemView::GetMnemonic() {
  return 'a';
}

}  // namespace views

#endif
