// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements a browser-side endpoint for UI automation activity.
// The client-side endpoint is implemented by AutomationProxy.
// The entire lifetime of this object should be contained within that of
// the BrowserProcess, and in particular the NotificationService that's
// hung off of it.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/test/automation/automation_constants.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_channel.h"
#if defined(OS_WIN)
#include "views/event.h"
#endif  // defined(OS_WIN)

struct AutomationMsg_Find_Params;
class PopupMenuWaiter;

namespace IPC {
struct Reposition_Params;
struct ExternalTabSettings;
class ChannelProxy;
}

class AutoFillProfile;
class AutomationAutocompleteEditTracker;
class AutomationBrowserTracker;
class AutomationExtensionTracker;
class AutomationResourceMessageFilter;
class AutomationTabTracker;
class AutomationWindowTracker;
class CreditCard;
class DictionaryValue;
class Extension;
class ExtensionPortContainer;
class ExtensionTestResultNotificationObserver;
class ExternalTabContainer;
class InitialLoadObserver;
class ListValue;
class LoginHandler;
class MetricEventDurationObserver;
class NavigationController;
class NavigationControllerRestoredObserver;
class Profile;
struct AutocompleteMatchData;

namespace gfx {
class Point;
}

class AutomationProvider : public base::RefCounted<AutomationProvider>,
                           public IPC::Channel::Listener,
                           public IPC::Message::Sender {
 public:
  explicit AutomationProvider(Profile* profile);

  Profile* profile() const { return profile_; }

  // Establishes a connection to an automation client, if present.
  // An AutomationProxy should be established (probably in a different process)
  // before calling this.
  void ConnectToChannel(const std::string& channel_id);

  // Sets the number of tabs that we expect; when this number of tabs has
  // loaded, an AutomationMsg_InitialLoadsComplete message is sent.
  void SetExpectedTabCount(size_t expected_tabs);

  // Add a listener for navigation status notification. Currently only
  // navigation completion is observed; when the |number_of_navigations|
  // complete, the completed_response object is sent; if the server requires
  // authentication, we instead send the auth_needed_response object.  A pointer
  // to the added navigation observer is returned. This object should NOT be
  // deleted and should be released by calling the corresponding
  // RemoveNavigationStatusListener method.
  NotificationObserver* AddNavigationStatusListener(
      NavigationController* tab, IPC::Message* reply_message,
      int number_of_navigations, bool include_current_navigation);

  void RemoveNavigationStatusListener(NotificationObserver* obs);

  // Add an observer for the TabStrip. Currently only Tab append is observed. A
  // navigation listener is created on successful notification of tab append. A
  // pointer to the added navigation observer is returned. This object should
  // NOT be deleted and should be released by calling the corresponding
  // RemoveTabStripObserver method.
  NotificationObserver* AddTabStripObserver(Browser* parent,
                                            IPC::Message* reply_message);
  void RemoveTabStripObserver(NotificationObserver* obs);

  // Get the index of a particular NavigationController object
  // in the given parent window.  This method uses
  // TabStrip::GetIndexForNavigationController to get the index.
  int GetIndexForNavigationController(const NavigationController* controller,
                                      const Browser* parent) const;

  // Add or remove a non-owning reference to a tab's LoginHandler.  This is for
  // when a login prompt is shown for HTTP/FTP authentication.
  // TODO(mpcomplete): The login handling is a fairly special purpose feature.
  // Eventually we'll probably want ways to interact with the ChromeView of the
  // login window in a generic manner, such that it can be used for anything,
  // not just logins.
  void AddLoginHandler(NavigationController* tab, LoginHandler* handler);
  void RemoveLoginHandler(NavigationController* tab);

  // Add an extension port container.
  // Takes ownership of the container.
  void AddPortContainer(ExtensionPortContainer* port);
  // Remove and delete the port container.
  void RemovePortContainer(ExtensionPortContainer* port);
  // Get the port container for the given port id.
  ExtensionPortContainer* GetPortContainer(int port_id) const;

  // IPC implementations
  virtual bool Send(IPC::Message* msg);
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  IPC::Message* reply_message_release() {
    IPC::Message* reply_message = reply_message_;
    reply_message_ = NULL;
    return reply_message;
  }

  // Adds the extension passed in to the extension tracker, and returns
  // the associated handle. If the tracker already contains the extension,
  // the handle is simply returned.
  int AddExtension(Extension* extension);

#if defined(OS_WIN)
  // Adds the external tab passed in to the tab tracker.
  bool AddExternalTab(ExternalTabContainer* external_tab);
#endif

  // Get the DictionaryValue equivalent for a download item. Caller owns the
  // DictionaryValue.
  DictionaryValue* GetDictionaryFromDownloadItem(const DownloadItem* download);

 protected:
  friend class base::RefCounted<AutomationProvider>;
  virtual ~AutomationProvider();

  // Helper function to find the browser window that contains a given
  // NavigationController and activate that tab.
  // Returns the Browser if found.
  Browser* FindAndActivateTab(NavigationController* contents);

  // Convert a tab handle into a TabContents. If |tab| is non-NULL a pointer
  // to the tab is also returned. Returns NULL in case of failure or if the tab
  // is not of the TabContents type.
  TabContents* GetTabContentsForHandle(int handle, NavigationController** tab);

  scoped_ptr<AutomationAutocompleteEditTracker> autocomplete_edit_tracker_;
  scoped_ptr<AutomationBrowserTracker> browser_tracker_;
  scoped_ptr<AutomationTabTracker> tab_tracker_;
  scoped_ptr<AutomationWindowTracker> window_tracker_;

  typedef std::map<NavigationController*, LoginHandler*> LoginHandlerMap;
  LoginHandlerMap login_handler_map_;

  Profile* profile_;

  // A pointer to reply message used when we do asynchronous processing in the
  // message handler.
  // TODO(phajdan.jr): Remove |reply_message_|, it is error-prone.
  IPC::Message* reply_message_;

  // Consumer for asynchronous history queries.
  CancelableRequestConsumer consumer_;

 private:
  // IPC Message callbacks.
  void GetShowingAppModalDialog(bool* showing_dialog, int* dialog_button);
  void ClickAppModalDialogButton(int button, bool* success);
  void ShutdownSessionService(int handle, bool* result);
  void WindowSimulateDrag(int handle,
                          std::vector<gfx::Point> drag_path,
                          int flags,
                          bool press_escape_en_route,
                          IPC::Message* reply_message);

#if defined(OS_WIN)
  // TODO(port): Replace HWND.
  void GetTabHWND(int handle, HWND* tab_hwnd);
#endif  // defined(OS_WIN)
  void HandleUnused(const IPC::Message& message, int handle);
  void SetShelfVisibility(int handle, bool visible);
  void SetFilteredInet(const IPC::Message& message, bool enabled);
  void GetFilteredInetHitCount(int* hit_count);
  void SetProxyConfig(const std::string& new_proxy_config);
  void SetContentSetting(int handle,
                         const std::string& host,
                         ContentSettingsType content_type,
                         ContentSetting setting,
                         bool* success);

  // Responds to the FindInPage request, retrieves the search query parameters,
  // launches an observer to listen for results and issues a StartFind request.
  void HandleFindRequest(int handle,
                         const AutomationMsg_Find_Params& params,
                         IPC::Message* reply_message);

  // Responds to requests to open the FindInPage window.
  void HandleOpenFindInPageRequest(const IPC::Message& message,
                                   int handle);

  // Get the visibility state of the Find window.
  void GetFindWindowVisibility(int handle, bool* visible);

  // Responds to requests to find the location of the Find window.
  void HandleFindWindowLocationRequest(int handle, int* x, int* y);

  // Get the visibility state of the Bookmark bar.
  void GetBookmarkBarVisibility(int handle, bool* visible, bool* animating);

  // Get the bookmarks as a JSON string.
  void GetBookmarksAsJSON(int handle, std::string* bookmarks_as_json,
                          bool *success);

  // Wait for the bookmark model to load.
  void WaitForBookmarkModelToLoad(int handle, IPC::Message* reply_message);

  // Set |loaded| to true if the bookmark model has loaded, else false.
  void BookmarkModelHasLoaded(int handle, bool* loaded);

  // Editing, modification, and removal of bookmarks.
  // Bookmarks are referenced by id.
  void AddBookmarkGroup(int handle,
                        int64 parent_id, int index, std::wstring title,
                        bool* success);
  void AddBookmarkURL(int handle,
                      int64 parent_id, int index,
                      std::wstring title, const GURL& url,
                      bool* success);
  void ReparentBookmark(int handle,
                        int64 id, int64 new_parent_id, int index,
                        bool* success);
  void SetBookmarkTitle(int handle,
                        int64 id, std::wstring title,
                        bool* success);
  void SetBookmarkURL(int handle,
                      int64 id, const GURL& url,
                      bool* success);
  void RemoveBookmark(int handle,
                      int64 id,
                      bool* success);

  // Set window dimensions.
  // Uses the JSON interface for input/output.
  void SetWindowDimensions(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about infobars in the given TabContents object.
  // This includes info about the type of infobars, the message text,
  // buttons, etc.
  // Caller owns the returned object.
  ListValue* GetInfobarsInfo(TabContents* tc);

  // Wait for infobar count in a given tab to become a certain value.
  // Uses the JSON interface for input/output.
  void WaitForInfobarCount(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Perform actions on an infobar like dismiss, accept, cancel.
  // Uses the JSON interface for input/output.
  void PerformActionOnInfobar(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Get info about the chromium/chrome in use.
  // This includes things like version, executable name, executable path.
  // Uses the JSON interface for input/output.
  void GetBrowserInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Get info about downloads. This includes only ones that have been
  // registered by the history system.
  // Uses the JSON interface for input/output.
  void GetDownloadsInfo(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Wait for all downloads to complete.
  // Uses the JSON interface for input/output.
  void WaitForDownloadsToComplete(Browser* browser,
                                  DictionaryValue* args,
                                  IPC::Message* reply_message);

  // Performs the given action on the specified download.
  // Uses the JSON interface for input/output.
  void PerformActionOnDownload(Browser* browser,
                               DictionaryValue* args,
                               IPC::Message* reply_message);

  // Waits until the given download has been opened to reply.
  // Uses the JSON interface for input/output.
  void WaitForAlwaysOpenDownloadTypeToOpen(Browser* browser,
                                           DictionaryValue* args,
                                           IPC::Message* reply_message);

  // Get info about history.
  // Uses the JSON interface for input/output.
  void GetHistoryInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Add an item to the history service.
  // Uses the JSON interface for input/output.
  void AddHistoryItem(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Get info about preferences.
  // Uses the JSON interface for input/output.
  void GetPrefsInfo(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Set prefs.
  // Uses the JSON interface for input/output.
  void SetPrefs(Browser* browser,
                DictionaryValue* args,
                IPC::Message* reply_message);

  // Return load times of initial tabs.
  // Uses the JSON interface for input/output.
  // Only includes tabs from command line arguments or session restore.
  // See declaration of InitialLoadObserver in automation_provider_observers.h
  // for example response.
  void GetInitialLoadTimes(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about plugins.
  // Uses the JSON interface for input/output.
  void GetPluginsInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Enable a plugin.
  // Uses the JSON interface for input/output.
  void EnablePlugin(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Disable a plugin.
  // Uses the JSON interface for input/output.
  void DisablePlugin(Browser* browser,
                     DictionaryValue* args,
                     IPC::Message* reply_message);

  // Get info about omnibox.
  // Contains data about the matches (url, content, description)
  // in the omnibox popup, the text in the omnibox.
  // Uses the JSON interface for input/output.
  void GetOmniboxInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Set text in the omnibox. This sets focus to the omnibox.
  // Uses the JSON interface for input/output.
  void SetOmniboxText(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Move omnibox popup selection up or down.
  // Uses the JSON interface for input/output.
  void OmniboxMovePopupSelection(Browser* browser,
                                 DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Accept the current string of text in the omnibox.
  // This is equivalent to clicking or hiting enter on a popup selection.
  // Blocks until the page loads.
  // Uses the JSON interface for input/output.
  void OmniboxAcceptInput(Browser* browser,
                          DictionaryValue* args,
                          IPC::Message* reply_message);

  // Save the contents of a tab into a file.
  // Uses the JSON interface for input/output.
  void SaveTabContents(Browser* browser,
                       DictionaryValue* args,
                       IPC::Message* reply_message);

  // Import the given settings from the given browser.
  // Uses the JSON interface for input/output.
  void ImportSettings(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Add a new entry to the password store based on the password information
  // provided. This method can also be used to add a blacklisted site (which
  // will never fill in the password).
  // Uses the JSON interface for input/output.
  void AddSavedPassword(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Removes the password matching the information provided. This method can
  // also be used to remove a blacklisted site.
  // Uses the JSON interface for input/output.
  void RemoveSavedPassword(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Return the saved username/password combinations.
  // Uses the JSON interface for input/output.
  void GetSavedPasswords(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Clear the specified browsing data. This call provides similar
  // functionality to RemoveBrowsingData but is synchronous.
  // Uses the JSON interface for input/output.
  void ClearBrowsingData(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Get info about theme.
  // Uses the JSON interface for input/output.
  void GetThemeInfo(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Get info about all intalled extensions.
  // Uses the JSON interface for input/output.
  void GetExtensionsInfo(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Uninstalls the extension with the given id.
  // Uses the JSON interface for input/output.
  void UninstallExtensionById(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Returns information about translation for a given tab. Includes
  // information about the translate bar if it is showing.
  void GetTranslateInfo(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Takes the specified action on the translate bar.
  // Uses the JSON interface for input/output.
  void SelectTranslateOption(Browser* browser,
                             DictionaryValue* args,
                             IPC::Message* reply_message);

  // Waits until an attempted page translation has completed to reply.
  // Uses the JSON interface for input/output.
  void WaitUntilTranslateComplete(Browser* browser,
                                  DictionaryValue* args,
                                  IPC::Message* reply_message);

  // Get the profiles that are currently saved to the DB.
  // Uses the JSON interface for input/output.
  void GetAutoFillProfile(Browser* browser,
                          DictionaryValue* args,
                          IPC::Message* reply_message);

  // Fill in an AutoFillProfile with the given profile information.
  // Uses the JSON interface for input/output.
  void FillAutoFillProfile(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Translate DictionaryValues of autofill profiles and credit cards to the
  // data structure used in the browser.
  // Args:
  //   profiles/cards: the ListValue of profiles/credit cards to translate.
  //   error_message: a pointer to the return string in case of error.
  static std::vector<AutoFillProfile> GetAutoFillProfilesFromList(
      const ListValue& profiles, std::string* error_message);
  static std::vector<CreditCard> GetCreditCardsFromList(
      const ListValue& cards, std::string* error_message);

  // The opposite of the above: translates from the internal data structure
  // for profiles and credit cards to a ListValue of DictionaryValues. The
  // caller owns the returned object.
  static ListValue* GetListFromAutoFillProfiles(
      std::vector<AutoFillProfile*> autofill_profiles);
  static ListValue* GetListFromCreditCards(
      std::vector<CreditCard*> credit_cards);

  // Return the map from the internal data representation to the string value
  // of auto fill fields and credit card fields.
  static std::map<AutoFillFieldType, std::wstring>
      GetAutoFillFieldToStringMap();
  static std::map<AutoFillFieldType, std::wstring>
      GetCreditCardFieldToStringMap();

  // Generic pattern for pyautolib
  // Uses the JSON interface for input/output.
  void SendJSONRequest(int handle,
                       std::string json_request,
                       IPC::Message* reply_message);

  // Method ptr for json handlers.
  // Uses the JSON interface for input/output.
  typedef void (AutomationProvider::*JsonHandler)(Browser* browser,
                                                  DictionaryValue*,
                                                  IPC::Message*);

  void GetDownloadDirectory(int handle, FilePath* download_directory);

  // Retrieves a Browser from a Window and vice-versa.
  void GetWindowForBrowser(int window_handle, bool* success, int* handle);
  void GetBrowserForWindow(int window_handle, bool* success,
                           int* browser_handle);

  // If |show| is true, call Show() on the new window after creating it.
  void OpenNewBrowserWindow(bool show, IPC::Message* reply_message);
  void OpenNewBrowserWindowOfType(int type,
                                  bool show,
                                  IPC::Message* reply_message);

  void ShowInterstitialPage(int tab_handle,
                            const std::string& html_text,
                            IPC::Message* reply_message);
  void HideInterstitialPage(int tab_handle, bool* success);

  void OnSetPageFontSize(int tab_handle, int font_size);

  // See browsing_data_remover.h for explanation of bitmap fields.
  void RemoveBrowsingData(int remove_mask);

  void InstallExtension(const FilePath& crx_path,
                        IPC::Message* reply_message);

  void LoadExpandedExtension(const FilePath& extension_dir,
                             IPC::Message* reply_message);

  void GetEnabledExtensions(std::vector<FilePath>* result);

  void WaitForExtensionTestResult(IPC::Message* reply_message);

  void InstallExtensionAndGetHandle(const FilePath& crx_path,
                                    bool with_ui,
                                    IPC::Message* reply_message);

  void UninstallExtension(int extension_handle,
                          bool* success);

  void ReloadExtension(int extension_handle,
                       IPC::Message* reply_message);

  void EnableExtension(int extension_handle,
                       IPC::Message* reply_message);

  void DisableExtension(int extension_handle,
                        bool* success);

  void ExecuteExtensionActionInActiveTabAsync(int extension_handle,
                                              int browser_handle,
                                              IPC::Message* reply_message);

  void MoveExtensionBrowserAction(int extension_handle, int index,
                                  bool* success);

  void GetExtensionProperty(int extension_handle,
                            AutomationMsg_ExtensionProperty type,
                            bool* success,
                            std::string* value);


  // See comment in AutomationMsg_WaitForTabToBeRestored.
  void WaitForTabToBeRestored(int tab_handle, IPC::Message* reply_message);

  // Gets the security state for the tab associated to the specified |handle|.
  void GetSecurityState(int handle, bool* success,
                        SecurityStyle* security_style, int* ssl_cert_status,
                        int* insecure_content_status);

  // Gets the page type for the tab associated to the specified |handle|.
  void GetPageType(int handle, bool* success,
                   NavigationEntry::PageType* page_type);

  // Gets the duration in ms of the last event matching |event_name|.
  // |duration_ms| is -1 if the event hasn't occurred yet.
  void GetMetricEventDuration(const std::string& event_name, int* duration_ms);

  // Simulates an action on the SSL blocking page at the tab specified by
  // |handle|. If |proceed| is true, it is equivalent to the user pressing the
  // 'Proceed' button, if false the 'Get me out of there button'.
  // Not that this fails if the tab is not displaying a SSL blocking page.
  void ActionOnSSLBlockingPage(int handle,
                               bool proceed,
                               IPC::Message* reply_message);

  // Brings the browser window to the front and activates it.
  void BringBrowserToFront(int browser_handle, bool* success);

  // Checks to see if a command on the browser's CommandController is enabled.
  void IsMenuCommandEnabled(int browser_handle,
                            int message_num,
                            bool* menu_item_enabled);

  // Prints the current tab immediately.
  void PrintNow(int tab_handle, IPC::Message* reply_message);

  // Asynchronous request for printing the current tab.
  void PrintAsync(int tab_handle);

  // Save the current web page.
  void SavePage(int tab_handle,
                const FilePath& file_name,
                const FilePath& dir_path,
                int type,
                bool* success);

  // Retrieves the number of info-bars currently showing in |count|.
  void GetInfoBarCount(int handle, int* count);

  // Causes a click on the "accept" button of the info-bar at |info_bar_index|.
  // If |wait_for_navigation| is true, it sends the reply after a navigation has
  // occurred.
  void ClickInfoBarAccept(int handle, int info_bar_index,
                          bool wait_for_navigation,
                          IPC::Message* reply_message);

  // Retrieves the last time a navigation occurred for the tab.
  void GetLastNavigationTime(int handle, int64* last_navigation_time);

  // Waits for a new navigation in the tab if none has happened since
  // |last_navigation_time|.
  void WaitForNavigation(int handle,
                         int64 last_navigation_time,
                         IPC::Message* reply_message);

  // Sets the int value for preference with name |name|.
  void SetIntPreference(int handle,
                        const std::string& name,
                        int value,
                        bool* success);

  // Sets the string value for preference with name |name|.
  void SetStringPreference(int handle,
                           const std::string& name,
                           const std::string& value,
                           bool* success);

  // Gets the bool value for preference with name |name|.
  void GetBooleanPreference(int handle,
                            const std::string& name,
                            bool* success,
                            bool* value);

  // Sets the bool value for preference with name |name|.
  void SetBooleanPreference(int handle,
                            const std::string& name,
                            bool value,
                            bool* success);

  // Resets to the default theme.
  void ResetToDefaultTheme();

  // Gets the current used encoding name of the page in the specified tab.
  void GetPageCurrentEncoding(int tab_handle, std::string* current_encoding);

  // Uses the specified encoding to override the encoding of the page in the
  // specified tab.
  void OverrideEncoding(int tab_handle,
                        const std::string& encoding_name,
                        bool* success);

  void SavePackageShouldPromptUser(bool should_prompt);

  // Enables extension automation (for e.g. UITests).
  void SetEnableExtensionAutomation(
      int tab_handle,
      const std::vector<std::string>& functions_enabled);

  void GetWindowTitle(int handle, string16* text);

  // Returns the number of blocked popups in the tab |handle|.
  void GetBlockedPopupCount(int handle, int* count);

  // Selects all contents on the page.
  void SelectAll(int tab_handle);

  // Edit operations on the page.
  void Cut(int tab_handle);
  void Copy(int tab_handle);
  void Paste(int tab_handle);

  void ReloadAsync(int tab_handle);
  void StopAsync(int tab_handle);
  void SaveAsAsync(int tab_handle);

  void WaitForBrowserWindowCountToBecome(int target_count,
                                         IPC::Message* reply_message);

  void WaitForAppModalDialogToBeShown(IPC::Message* reply_message);

  void GoBackBlockUntilNavigationsComplete(int handle,
                                           int number_of_navigations,
                                           IPC::Message* reply_message);

  void GoForwardBlockUntilNavigationsComplete(int handle,
                                              int number_of_navigations,
                                              IPC::Message* reply_message);

#if defined(OS_CHROMEOS)
  // Logs in through the Chrome OS Login Wizard with given |username| and
  // password.  Returns true via |reply_message| on success.
  void LoginWithUserAndPass(const std::string& username,
                            const std::string& password,
                            IPC::Message* reply_message);
#endif

  // Returns the associated view for the tab handle passed in.
  // Returns NULL on failure.
  RenderViewHost* GetViewForTab(int tab_handle);

  // Returns the extension for the given handle. Returns NULL if there is
  // no extension for the handle.
  Extension* GetExtension(int extension_handle);

  // Returns the extension for the given handle, if the handle is valid and
  // the associated extension is enabled. Returns NULL otherwise.
  Extension* GetEnabledExtension(int extension_handle);

  // Returns the extension for the given handle, if the handle is valid and
  // the associated extension is disabled. Returns NULL otherwise.
  Extension* GetDisabledExtension(int extension_handle);

  // Method called by the popup menu tracker when a popup menu is opened.
  void NotifyPopupMenuOpened();

#if defined(OS_WIN)
  // The functions in this block are for use with external tabs, so they are
  // Windows only.

  // The container of an externally hosted tab calls this to reflect any
  // accelerator keys that it did not process. This gives the tab a chance
  // to handle the keys
  void ProcessUnhandledAccelerator(const IPC::Message& message, int handle,
                                   const MSG& msg);

  void SetInitialFocus(const IPC::Message& message, int handle, bool reverse,
                       bool restore_focus_to_view);

  void OnTabReposition(int tab_handle,
                       const IPC::Reposition_Params& params);

  void OnForwardContextMenuCommandToChrome(int tab_handle, int command);

  void CreateExternalTab(const IPC::ExternalTabSettings& settings,
                         gfx::NativeWindow* tab_container_window,
                         gfx::NativeWindow* tab_window,
                         int* tab_handle);

  void ConnectExternalTab(uint64 cookie,
                          bool allow,
                          gfx::NativeWindow parent_window,
                          gfx::NativeWindow* tab_container_window,
                          gfx::NativeWindow* tab_window,
                          int* tab_handle);

  void NavigateInExternalTab(
      int handle, const GURL& url, const GURL& referrer,
      AutomationMsg_NavigationResponseValues* status);
  void NavigateExternalTabAtIndex(
      int handle, int index, AutomationMsg_NavigationResponseValues* status);

  // Handler for a message sent by the automation client.
  void OnMessageFromExternalHost(int handle, const std::string& message,
                                 const std::string& origin,
                                 const std::string& target);

  // Determine if the message from the external host represents a browser
  // event, and if so dispatch it.
  bool InterceptBrowserEventMessageFromExternalHost(const std::string& message,
                                                    const std::string& origin,
                                                    const std::string& target);

  void OnBrowserMoved(int handle);

  void OnRunUnloadHandlers(int handle, gfx::NativeWindow notification_window,
                           int notification_message);

  void OnSetZoomLevel(int handle, int zoom_level);

  ExternalTabContainer* GetExternalTabForHandle(int handle);
#endif  // defined(OS_WIN)

  typedef ObserverList<NotificationObserver> NotificationObserverList;
  typedef std::map<int, ExtensionPortContainer*> PortContainerMap;

  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<InitialLoadObserver> initial_load_observer_;
  scoped_ptr<NotificationObserver> new_tab_ui_load_observer_;
  scoped_ptr<NotificationObserver> find_in_page_observer_;
  scoped_ptr<NotificationObserver> dom_operation_observer_;
  scoped_ptr<NotificationObserver> dom_inspector_observer_;
  scoped_ptr<ExtensionTestResultNotificationObserver>
      extension_test_result_observer_;
  scoped_ptr<MetricEventDurationObserver> metric_event_duration_observer_;
  scoped_ptr<AutomationExtensionTracker> extension_tracker_;
  scoped_ptr<NavigationControllerRestoredObserver> restore_tracker_;
  PortContainerMap port_containers_;
  NotificationObserverList notification_observer_list_;
  scoped_refptr<AutomationResourceMessageFilter>
      automation_resource_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
