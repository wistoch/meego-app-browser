// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/notification_registrar.h"

// This is an automation provider containing testing calls.
class TestingAutomationProvider : public AutomationProvider,
                                  public BrowserList::Observer,
                                  public NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);

  // BrowserList::Observer implementation
  // Called immediately after a browser is added to the list
  virtual void OnBrowserAdded(const Browser* browser);
  // Called immediately before a browser is removed from the list
  virtual void OnBrowserRemoving(const Browser* browser);

  // IPC implementations
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

 private:
  class PopupMenuWaiter;

  virtual ~TestingAutomationProvider();

  // IPC Message callbacks.
  void CloseBrowser(int handle, IPC::Message* reply_message);
  void CloseBrowserAsync(int browser_handle);
  void ActivateTab(int handle, int at_index, int* status);
  void AppendTab(int handle, const GURL& url, IPC::Message* reply_message);
  void GetActiveTabIndex(int handle, int* active_tab_index);
  void CloseTab(int tab_handle, bool wait_until_closed,
                IPC::Message* reply_message);
  void GetCookies(const GURL& url, int handle, int* value_size,
                  std::string* value);
  void SetCookie(const GURL& url,
                 const std::string value,
                 int handle,
                 int* response_value);
  void DeleteCookie(const GURL& url, const std::string& cookie_name,
                    int handle, bool* success);
  void ShowCollectedCookiesDialog(int handle, bool* success);
  void NavigateToURL(int handle, const GURL& url, IPC::Message* reply_message);
  void NavigateToURLBlockUntilNavigationsComplete(int handle, const GURL& url,
                                                  int number_of_navigations,
                                                  IPC::Message* reply_message);
  void NavigationAsync(int handle, const GURL& url, bool* status);
  void NavigationAsyncWithDisposition(int handle,
                                      const GURL& url,
                                      WindowOpenDisposition disposition,
                                      bool* status);
  void GoBack(int handle, IPC::Message* reply_message);
  void GoForward(int handle, IPC::Message* reply_message);
  void Reload(int handle, IPC::Message* reply_message);
  void SetAuth(int tab_handle, const std::wstring& username,
               const std::wstring& password, IPC::Message* reply_message);
  void CancelAuth(int tab_handle, IPC::Message* reply_message);
  void NeedsAuth(int tab_handle, bool* needs_auth);
  void GetRedirectsFrom(int tab_handle,
                        const GURL& source_url,
                        IPC::Message* reply_message);
  void GetBrowserWindowCount(int* window_count);
  void GetNormalBrowserWindowCount(int* window_count);
  // Be aware that the browser window returned might be of non TYPE_NORMAL
  // or in incognito mode.
  void GetBrowserWindow(int index, int* handle);
  void FindNormalBrowserWindow(int* handle);
  void GetLastActiveBrowserWindow(int* handle);
  void GetActiveWindow(int* handle);
  void ExecuteBrowserCommandAsync(int handle, int command, bool* success);
  void ExecuteBrowserCommand(int handle, int command,
                             IPC::Message* reply_message);
  void GetBrowserLocale(string16* locale);
  void IsWindowActive(int handle, bool* success, bool* is_active);
  void ActivateWindow(int handle);
  void IsWindowMaximized(int handle, bool* is_maximized, bool* success);
  void TerminateSession(int handle, bool* success);
  void WindowGetViewBounds(int handle, int view_id, bool screen_coordinates,
                           bool* success, gfx::Rect* bounds);
  void GetWindowBounds(int handle, gfx::Rect* bounds, bool* result);
  void SetWindowBounds(int handle, const gfx::Rect& bounds, bool* result);
  void SetWindowVisible(int handle, bool visible, bool* result);
  void WindowSimulateClick(const IPC::Message& message,
                           int handle,
                           const gfx::Point& click,
                           int flags);
  void WindowSimulateMouseMove(const IPC::Message& message,
                               int handle,
                               const gfx::Point& location);
  void WindowSimulateKeyPress(const IPC::Message& message,
                              int handle,
                              int key,
                              int flags);
  void GetTabCount(int handle, int* tab_count);
  void GetType(int handle, int* type_as_int);
  void GetTab(int win_handle, int tab_index, int* tab_handle);
  void GetTabProcessID(int handle, int* process_id);
  void GetTabTitle(int handle, int* title_string_size, std::wstring* title);
  void GetTabIndex(int handle, int* tabstrip_index);
  void GetTabURL(int handle, bool* success, GURL* url);
  void GetShelfVisibility(int handle, bool* visible);
  void IsFullscreen(int handle, bool* is_fullscreen);
  void GetFullscreenBubbleVisibility(int handle, bool* is_visible);
  void GetAutocompleteEditForBrowser(int browser_handle, bool* success,
                                     int* autocomplete_edit_handle);

  // Retrieves the visible text from the autocomplete edit.
  void GetAutocompleteEditText(int autocomplete_edit_handle,
                               bool* success, std::wstring* text);

  // Sets the visible text from the autocomplete edit.
  void SetAutocompleteEditText(int autocomplete_edit_handle,
                               const std::wstring& text,
                               bool* success);

  // Retrieves if a query to an autocomplete provider is in progress.
  void AutocompleteEditIsQueryInProgress(int autocomplete_edit_handle,
                                         bool* success,
                                         bool* query_in_progress);

  // Retrieves the individual autocomplete matches displayed by the popup.
  void AutocompleteEditGetMatches(int autocomplete_edit_handle,
                                  bool* success,
                                  std::vector<AutocompleteMatchData>* matches);

  // Deprecated.
  void ApplyAccelerator(int handle, int id);

  void ExecuteJavascript(int handle,
                         const std::wstring& frame_xpath,
                         const std::wstring& script,
                         IPC::Message* reply_message);

  void GetConstrainedWindowCount(int handle, int* count);

  // This function has been deprecated, please use HandleFindRequest.
  void HandleFindInPageRequest(int handle,
                               const std::wstring& find_request,
                               int forward,
                               int match_case,
                               int* active_ordinal,
                               int* matches_found);

#if defined(TOOLKIT_VIEWS)
  void GetFocusedViewID(int handle, int* view_id);

  // Block until the focused view ID changes to something other than
  // previous_view_id.
  void WaitForFocusedViewIDToChange(int handle,
                                    int previous_view_id,
                                    IPC::Message* reply_message);

  // Start tracking popup menus. Must be called before executing the
  // command that might open the popup menu; then call WaitForPopupMenuToOpen.
  void StartTrackingPopupMenus(int browser_handle, bool* success);

  // Wait until a popup menu has opened.
  void WaitForPopupMenuToOpen(IPC::Message* reply_message);
#endif  // defined(TOOLKIT_VIEWS)

  void HandleInspectElementRequest(int handle,
                                   int x,
                                   int y,
                                   IPC::Message* reply_message);

  // Callback for history redirect queries.
  virtual void OnRedirectQueryComplete(
      HistoryService::Handle request_handle,
      GURL from_url,
      bool success,
      history::RedirectList* redirects);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void OnRemoveProvider();  // Called via PostTask

#if defined(TOOLKIT_VIEWS)
  // Keep track of whether a popup menu has been opened since the last time
  // that StartTrackingPopupMenus has been called.
  bool popup_menu_opened_;

  // A temporary object that receives a notification when a popup menu opens.
  PopupMenuWaiter* popup_menu_waiter_;
#endif  // defined(TOOLKIT_VIEWS)

  // Handle for an in-process redirect query. We expect only one redirect query
  // at a time (we should have only one caller, and it will block while waiting
  // for the results) so there is only one handle. When non-0, indicates a
  // query in progress.
  HistoryService::Handle redirect_query_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestingAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
