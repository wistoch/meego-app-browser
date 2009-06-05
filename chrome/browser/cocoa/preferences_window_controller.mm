// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/preferences_window_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/custom_home_pages_model.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

NSString* const kUserDoneEditingPrefsNotification =
    @"kUserDoneEditingPrefsNotification";

namespace {
std::wstring GetNewTabUIURLString() {
  std::wstring temp = UTF8ToWide(chrome::kChromeUINewTabURL);
  return URLFixerUpper::FixupURL(temp, std::wstring());
}
}  // namespace

//-------------------------------------------------------------------------

@interface PreferencesWindowController(Private)
// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed, or |NULL| if all prefs should be updated.
- (void)prefChanged:(std::wstring*)prefName;
// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const wchar_t*)action;
- (void)registerPrefObservers;
- (void)unregisterPrefObservers;

- (void)customHomePagesChanged;

// KVC setter methods.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)val;
- (void)setHomepageURL:(NSString*)urlString;
- (void)setRestoreOnStartupIndex:(NSInteger)type;
- (void)setShowHomeButton:(BOOL)value;
- (void)setShowPageOptionsButtons:(BOOL)value;
- (void)setDefaultBrowser:(BOOL)value;
- (void)setPasswordManagerEnabledIndex:(NSInteger)value;
- (void)setFormAutofillEnabledIndex:(NSInteger)value;
@end

// A C++ class registered for changes in preferences. Bridges the
// notification back to the PWC.
class PrefObserverBridge : public NotificationObserver {
 public:
  PrefObserverBridge(PreferencesWindowController* controller)
      : controller_(controller) { }
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::PREF_CHANGED)
      [controller_ prefChanged:Details<std::wstring>(details).ptr()];
  }
 private:
  PreferencesWindowController* controller_;  // weak, owns us
};

@implementation PreferencesWindowController

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString *nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"Preferences"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    prefs_ = profile->GetPrefs();
    DCHECK(prefs_);
    observer_.reset(new PrefObserverBridge(self));

    // Set up the model for the custom home page table. The KVO observation
    // tells us when the number of items in the array changes. The normal
    // observation tells us when one of the URLs of an item changes.
    customPagesSource_.reset([[CustomHomePagesModel alloc]
                                initWithProfile:profile_]);
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [customPagesSource_ setURLs:startupPref.urls];
    [customPagesSource_ addObserver:self
                         forKeyPath:@"customHomePages"
                            options:0L
                            context:NULL];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(homepageEntryChanged:)
               name:kHomepageEntryChangedNotification
             object:nil];

    // This needs to be done before awakeFromNib: because the bindings set up
    // in the nib rely on it.
    [self registerPrefObservers];
  }
  return self;
}

- (void)awakeFromNib {
  // TODO(pinkerton): save/restore size based on prefs.
  [[self window] center];

  // Put the advanced view into the scroller and scroll it to the top.
  [advancedScroller_ setDocumentView:advancedView_];
  NSInteger height = [advancedView_ bounds].size.height;
  [advancedView_ scrollPoint:NSMakePoint(0, height)];

  // Ensure the "basics" tab is selected regardless of what is the selected
  // tab in the nib.
  [tabView_ selectFirstTabViewItem:self];
}

- (void)dealloc {
  [customPagesSource_ removeObserver:self forKeyPath:@"customHomePages"];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self unregisterPrefObservers];
  [super dealloc];
}

// Register our interest in the preferences we're displaying so if anything
// else in the UI changes them we will be updated.
- (void)registerPrefObservers {
  if (!prefs_) return;

  // Basics panel
  prefs_->AddPrefObserver(prefs::kURLsToRestoreOnStartup, observer_.get());
  restoreOnStartup_.Init(prefs::kRestoreOnStartup, prefs_, observer_.get());
  newTabPageIsHomePage_.Init(prefs::kHomePageIsNewTabPage,
                             prefs_, observer_.get());
  homepage_.Init(prefs::kHomePage, prefs_, observer_.get());
  showHomeButton_.Init(prefs::kShowHomeButton, prefs_, observer_.get());
  showPageOptionButtons_.Init(prefs::kShowPageOptionsButtons, prefs_,
                              observer_.get());
  // TODO(pinkerton): Register Default search.

  // UserData panel
  askSavePasswords_.Init(prefs::kPasswordManagerEnabled,
                         prefs_, observer_.get());
  formAutofill_.Init(prefs::kFormAutofillEnabled, prefs_, observer_.get());

  // TODO(pinkerton): do other panels...
}

// Clean up what was registered in -registerPrefObservers. We only have to
// clean up the non-PrefMember registrations.
- (void)unregisterPrefObservers {
  if (!prefs_) return;

  // Basics
  prefs_->RemovePrefObserver(prefs::kURLsToRestoreOnStartup, observer_.get());

  // User Data panel
  // Nothing to do here.

  // TODO(pinkerton): do other panels...
}

// Called when a key we're observing via KVO changes.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"customHomePages"]) {
    [self customHomePagesChanged];
    return;
  }
  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

// Record the user performed a certain action and save the preferences.
- (void)recordUserAction:(const wchar_t*)action {
  UserMetrics::RecordComputedAction(action, profile_);
  if (prefs_)
    prefs_->ScheduleSavePersistentPrefs();
}

// Returns the set of keys that |key| depends on for its value so it can be
// re-computed when any of those change as well.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* paths = [super keyPathsForValuesAffectingValueForKey:key];
  if ([key isEqualToString:@"isHomepageURLEnabled"]) {
    paths = [paths setByAddingObject:@"newTabPageIsHomePageIndex"];
  } else if ([key isEqualToString:@"enableRestoreButtons"]) {
    paths = [paths setByAddingObject:@"restoreOnStartupIndex"];
  } else if ([key isEqualToString:@"isDefaultBrowser"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserTextColor"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  } else if ([key isEqualToString:@"defaultBrowserText"]) {
    paths = [paths setByAddingObject:@"defaultBrowser"];
  }
  return paths;
}

//-------------------------------------------------------------------------
// Basics panel

// Sets the home page preferences for kNewTabPageIsHomePage and kHomePage. If a
// blank string is passed in we revert to using NewTab page as the Home page.
// When setting the Home Page to NewTab page, we preserve the old value of
// kHomePage (we don't overwrite it). Note: using SetValue() causes the
// observers not to fire, which is actually a good thing as we could end up in a
// state where setting the homepage to an empty url would automatically reset
// the prefs back to using the NTP, so we'd be never be able to change it.
- (void)setHomepage:(const std::wstring&)homepage {
  if (homepage.empty() || homepage == GetNewTabUIURLString()) {
    newTabPageIsHomePage_.SetValue(true);
  } else {
    newTabPageIsHomePage_.SetValue(false);
    homepage_.SetValue(homepage);
  }
}

// Callback when preferences are changed by someone modifying the prefs backend
// externally. |prefName| is the name of the pref that has changed. Unlike on
// Windows, we don't need to use this method for initializing, that's handled by
// Cocoa Bindings.
// Handles prefs for the "Basics" panel.
- (void)basicsPrefChanged:(std::wstring*)prefName {
  if (*prefName == prefs::kRestoreOnStartup) {
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [self setRestoreOnStartupIndex:startupPref.type];
  }

  // TODO(beng): Note that the kURLsToRestoreOnStartup pref is a mutable list,
  //             and changes to mutable lists aren't broadcast through the
  //             observer system, so this condition will
  //             never match. Once support for broadcasting such updates is
  //             added, this will automagically start to work, and this comment
  //             can be removed.
  if (*prefName == prefs::kURLsToRestoreOnStartup) {
    const SessionStartupPref startupPref =
        SessionStartupPref::GetStartupPref(prefs_);
    [customPagesSource_ setURLs:startupPref.urls];
  }

  if (*prefName == prefs::kHomePageIsNewTabPage) {
    NSInteger useNewTabPage = newTabPageIsHomePage_.GetValue() ? 0 : 1;
    [self setNewTabPageIsHomePageIndex:useNewTabPage];
  }
  if (*prefName == prefs::kHomePage) {
    NSString* value = base::SysWideToNSString(homepage_.GetValue());
    [self setHomepageURL:value];
  }

  if (*prefName == prefs::kShowHomeButton) {
    [self setShowHomeButton:showHomeButton_.GetValue() ? YES : NO];
  }
  if (*prefName == prefs::kShowPageOptionsButtons) {
    [self setShowPageOptionsButtons:showPageOptionButtons_.GetValue() ?
        YES : NO];
  }
}

// Returns the index of the selected cell in the "on startup" matrix based
// on the "restore on startup" pref. The ordering of the cells is in the
// same order as the pref.
- (NSInteger)restoreOnStartupIndex {
  const SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs_);
  return pref.type;
}

// A helper function that takes the startup session type, grabs the URLs to
// restore, and saves it all in prefs.
- (void)saveSessionStartupWithType:(SessionStartupPref::Type)type {
  SessionStartupPref pref;
  pref.type = type;
  pref.urls = [customPagesSource_.get() URLs];
  SessionStartupPref::SetStartupPref(prefs_, pref);
}

// Called when the custom home pages array changes. Force a save to prefs, but
// in order to save it, we have to look up what the current radio button
// setting is (since they're set together). What a pain.
- (void)customHomePagesChanged {
  const SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs_);
  [self saveSessionStartupWithType:pref.type];
}

// Called when an entry in the custom home page array changes URLs. Force
// a save to prefs.
- (void)homepageEntryChanged:(NSNotification*)notify {
  [self customHomePagesChanged];
}

// Sets the pref based on the index of the selected cell in the matrix and
// marks the appropriate user metric.
- (void)setRestoreOnStartupIndex:(NSInteger)type {
  SessionStartupPref::Type startupType =
      static_cast<SessionStartupPref::Type>(type);
  switch (startupType) {
    case SessionStartupPref::DEFAULT:
      [self recordUserAction:L"Options_Startup_Homepage"];
      break;
    case SessionStartupPref::LAST:
      [self recordUserAction:L"Options_Startup_LastSession"];
      break;
    case SessionStartupPref::URLS:
      [self recordUserAction:L"Options_Startup_Custom"];
      break;
    default:
      NOTREACHED();
  }
  [self saveSessionStartupWithType:startupType];
}

// Returns whether or not the +/-/Current buttons should be enabled, based on
// the current pref value for the startup urls.
- (BOOL)enableRestoreButtons {
  return [self restoreOnStartupIndex] == SessionStartupPref::URLS;
}

// Getter for the |customPagesSource| property for bindings.
- (id)customPagesSource {
  return customPagesSource_.get();
}

// Called when the selection in the table changes. If a flag is set indicating
// that we're waiting for a special select message, edit the cell. Otherwise
// just ignore it, we don't normally care.
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
  if (pendingSelectForEdit_) {
    NSTableView* table = [aNotification object];
    NSUInteger selectedRow = [table selectedRow];
    [table editColumn:0 row:selectedRow withEvent:nil select:YES];
    pendingSelectForEdit_ = NO;
  }
}

// Called when the user hits the (+) button for adding a new homepage to the
// list. This will also attempt to make the new item editable so the user can
// just start typing.
- (IBAction)addHomepage:(id)sender {
  [customPagesArrayController_ add:sender];

  // When the new item is added to the model, the array controller will select
  // it. We'll watch for that notification (because we are the table view's
  // delegate) and then make the cell editable. Note that this can't be
  // accomplished simply by subclassing the array controller's add method (I
  // did try). The update of the table is asynchronous with the controller
  // updating the model.
  pendingSelectForEdit_ = YES;
}

// Called when the user hits the (-) button for removing the selected items in
// the homepage table. The controller does all the work.
- (IBAction)removeSelectedHomepages:(id)sender {
  [customPagesArrayController_ remove:sender];
}

// Add all entries for all open browsers with our profile.
- (IBAction)useCurrentPagesAsHomepage:(id)sender {
  std::vector<GURL> urls;
  for (BrowserList::const_iterator browserIter = BrowserList::begin();
       browserIter != BrowserList::end(); ++browserIter) {
    Browser* browser = *browserIter;
    if (browser->profile() != profile_)
      continue;  // Only want entries for open profile.

    for (int tabIndex = 0; tabIndex < browser->tab_count(); ++tabIndex) {
      TabContents* tab = browser->GetTabContentsAt(tabIndex);
      if (tab->ShouldDisplayURL()) {
        const GURL url = browser->GetTabContentsAt(tabIndex)->GetURL();
        if (!url.is_empty())
          urls.push_back(url);
      }
    }
  }
  [customPagesSource_ setURLs:urls];
}

enum { kHomepageNewTabPage, kHomepageURL };

// Returns the index of the selected cell in the "home page" marix based on
// the "new tab is home page" pref. Sadly, the ordering is reversed from the
// pref value.
- (NSInteger)newTabPageIsHomePageIndex {
  return newTabPageIsHomePage_.GetValue() ?
      kHomepageNewTabPage : kHomepageURL;
}

// Sets the pref based on the given index into the matrix and marks the
// appropriate user metric.
- (void)setNewTabPageIsHomePageIndex:(NSInteger)index {
  bool useNewTabPage = index == kHomepageNewTabPage ? true : false;
  if (useNewTabPage)
    [self recordUserAction:L"Options_Homepage_UseNewTab"];
  else
    [self recordUserAction:L"Options_Homepage_UseURL"];
  newTabPageIsHomePage_.SetValue(useNewTabPage);
}

// Returns whether or not the homepage URL text field should be enabled
// based on if the new tab page is the home page.
- (BOOL)isHomepageURLEnabled {
  return newTabPageIsHomePage_.GetValue() ? NO : YES;
}

// Returns the homepage URL.
- (NSString*)homepageURL {
  NSString* value = base::SysWideToNSString(homepage_.GetValue());
  return value;
}

// Sets the homepage URL to |urlString| with some fixing up.
- (void)setHomepageURL:(NSString*)urlString {
  // If the text field contains a valid URL, sync it to prefs. We run it
  // through the fixer upper to allow input like "google.com" to be converted
  // to something valid ("http://google.com").
  if (!urlString)
    urlString = [NSString stringWithFormat:@"%s", chrome::kChromeUINewTabURL];
  std::wstring temp = base::SysNSStringToWide(urlString);
  std::wstring fixedString = URLFixerUpper::FixupURL(temp, std::wstring());
  if (GURL(WideToUTF8(fixedString)).is_valid())
    [self setHomepage:fixedString];
}

// Returns whether the home button should be checked based on the preference.
- (BOOL)showHomeButton {
  return showHomeButton_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the home button should be displayed
// based on |value|.
- (void)setShowHomeButton:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_Homepage_ShowHomeButton"];
  else
    [self recordUserAction:L"Options_Homepage_HideHomeButton"];
  showHomeButton_.SetValue(value ? true : false);
}

// Returns whether the page and options button should be checked based on the
// preference.
- (BOOL)showPageOptionsButtons {
  return showPageOptionButtons_.GetValue() ? YES : NO;
}

// Sets the backend pref for whether or not the page and options buttons should
// be displayed based on |value|.
- (void)setShowPageOptionsButtons:(BOOL)value {
  if (value)
    [self recordUserAction:L"Options_Homepage_ShowPageOptionsButtons"];
  else
    [self recordUserAction:L"Options_Homepage_HidePageOptionsButtons"];
  showPageOptionButtons_.SetValue(value ? true : false);
}

// Called when the user clicks the button to make Chromium the default
// browser. Registers http and https.
- (IBAction)makeDefaultBrowser:(id)sender {
  ShellIntegration::SetAsDefaultBrowser();
  [self recordUserAction:L"Options_SetAsDefaultBrowser"];
  // If the user made Chrome the default browser, then he/she arguably wants
  // to be notified when that changes.
  prefs_->SetBoolean(prefs::kCheckDefaultBrowser, true);

  // Tickle KVO so that the UI updates.
  [self setDefaultBrowser:YES];
}

// A stub setter so that we can trick KVO into thinking the UI needs
// to be updated.
- (void)setDefaultBrowser:(BOOL)ignore {
  // Do nothing.
}

// Returns if Chromium is the default browser.
- (BOOL)isDefaultBrowser {
  return ShellIntegration::IsDefaultBrowser() ? YES : NO;
}

// Returns the text color of the "chromium is your default browser" text (green
// for yes, red for no).
- (NSColor*)defaultBrowserTextColor {
  return [self isDefaultBrowser] ?
    [NSColor colorWithCalibratedRed:0.0 green:135.0/255.0 blue:0 alpha:1.0] :
    [NSColor colorWithCalibratedRed:135.0/255.0 green:0 blue:0 alpha:1.0];
}

// Returns the text for the "chromium is your default browser" string dependent
// on if Chromium actually is or not.
- (NSString*)defaultBrowserText {
  BOOL isDefault = [self isDefaultBrowser];
  int stringId = isDefault ? IDS_OPTIONS_DEFAULTBROWSER_DEFAULT :
      IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  std::wstring text =
      l10n_util::GetStringF(stringId, l10n_util::GetString(IDS_PRODUCT_NAME));
  return base::SysWideToNSString(text);
}

//-------------------------------------------------------------------------
// User Data panel

// Since passwords and forms are radio groups, 'enabled' is index 0 and
// 'disabled' is index 1. Yay.
const int kEnabledIndex = 0;
const int kDisabledIndex = 1;

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed, or |NULL| if all prefs should be updated.
// Handles prefs for the "Minor Tweaks" panel.
- (void)userDataPrefChanged:(std::wstring*)prefName {
  if (*prefName == prefs::kPasswordManagerEnabled) {
    [self setPasswordManagerEnabledIndex:askSavePasswords_.GetValue() ?
        kEnabledIndex : kDisabledIndex];
  }
  if (*prefName == prefs::kFormAutofillEnabled) {
    [self setFormAutofillEnabledIndex:formAutofill_.GetValue() ?
        kEnabledIndex : kDisabledIndex];
  }
}

// Called to launch the Keychain Access app to show the user's stored
// passwords.
- (IBAction)showSavedPasswords:(id)sender {
  NSString* const kKeychainBundleId = @"com.apple.keychainaccess";
  [self recordUserAction:L"Options_ShowPasswordsExceptions"];
  [[NSWorkspace sharedWorkspace]
      launchAppWithBundleIdentifier:kKeychainBundleId
                            options:0L
     additionalEventParamDescriptor:nil
                   launchIdentifier:nil];
}

// Called to import data from other browsers (Safari, Firefox, etc).
- (IBAction)importData:(id)sender {
  NOTIMPLEMENTED();
}

// Called to clear user's browsing data. This puts up an application-modal
// dialog to guide the user through clearing the data.
- (IBAction)clearData:(id)sender {
  scoped_nsobject<ClearBrowsingDataController> controller(
      [[ClearBrowsingDataController alloc]
          initWithProfile:profile_]);
  [controller runModalDialog];
}

// Called to reset the theming info back to the defaults.
- (IBAction)resetTheme:(id)sender {
  [self recordUserAction:L"Options_ThemesReset"];
  NOTIMPLEMENTED();
}

- (void)setPasswordManagerEnabledIndex:(NSInteger)value {
  if (value == kEnabledIndex)
    [self recordUserAction:L"Options_PasswordManager_Enable"];
  else
    [self recordUserAction:L"Options_PasswordManager_Disable"];
  askSavePasswords_.SetValue(value == kEnabledIndex ? true : false);
}

- (NSInteger)passwordManagerEnabledIndex {
  return askSavePasswords_.GetValue() ? kEnabledIndex : kDisabledIndex;
}

- (void)setFormAutofillEnabledIndex:(NSInteger)value {
  if (value == kEnabledIndex)
    [self recordUserAction:L"Options_FormAutofill_Enable"];
  else
    [self recordUserAction:L"Options_FormAutofill_Disable"];
  formAutofill_.SetValue(value == kEnabledIndex ? true : false);
}

- (NSInteger)formAutofillEnabledIndex {
  return formAutofill_.GetValue() ? kEnabledIndex : kDisabledIndex;
}

//-------------------------------------------------------------------------
// Under the hood panel

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed, or |NULL| if all prefs should be updated.
// Handles prefs for the "Under the hood" panel.
- (void)underHoodPrefChanged:(std::wstring*)prefName {
}

//-------------------------------------------------------------------------

// Callback when preferences are changed. |prefName| is the name of the
// pref that has changed and should not be NULL.
- (void)prefChanged:(std::wstring*)prefName {
  DCHECK(prefName);
  if (!prefName) return;
  [self basicsPrefChanged:prefName];
  [self userDataPrefChanged:prefName];
  [self underHoodPrefChanged:prefName];
}

// Show the preferences window.
- (IBAction)showPreferences:(id)sender {
  [self showWindow:sender];
}

// Called when the window is being closed. Send out a notification that the
// user is done editing preferences.
- (void)windowWillClose:(NSNotification *)notification {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kUserDoneEditingPrefsNotification
                    object:self];
}

@end
