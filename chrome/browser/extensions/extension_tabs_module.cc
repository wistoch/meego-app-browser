// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/histogram.h"
#include "base/base64.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/url_constants.h"
#include "gfx/codec/jpeg_codec.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace keys = extension_tabs_module_constants;

// Forward declare static helper functions defined below.

// |error_message| can optionally be passed in a will be set with an appropriate
// message if the window cannot be found by id.
static Browser* GetBrowserInProfileWithId(Profile* profile,
                                          const int window_id,
                                          bool include_incognito,
                                          std::string* error_message);

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
static bool GetTabById(int tab_id, Profile* profile,
                       bool include_incognito,
                       Browser** browser,
                       TabStripModel** tab_strip,
                       TabContents** contents,
                       int* tab_index, std::string* error_message);

// Takes |url_string| and returns a GURL which is either valid and absolute
// or invalid. If |url_string| is not directly interpretable as a valid (it is
// likely a relative URL) an attempt is made to resolve it. |extension| is
// provided so it can be resolved relative to its extension base
// (chrome-extension://<id>/). |source_url| is provided so that we can test
// whether |url_string| resolves differently relative to the source frame url.
// Using the source frame url would be more correct, but because the api shipped
// with urls resolved relative to their extension base, we must first measure
// how much of an inpact making the change would have.
static GURL ResolvePossiblyRelativeURL(std::string url_string,
                                       Extension* extension,
                                       const GURL& source_url);

// Return the type name for a browser window type.
static std::string GetWindowTypeText(Browser::Type type);

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetTabId(const TabContents* tab_contents) {
  return tab_contents->controller().session_id().id();
}

ExtensionTabUtil::TabStatus ExtensionTabUtil::GetTabStatus(
    const TabContents* tab_contents) {
  return tab_contents->is_loading() ? TAB_LOADING : TAB_COMPLETE;
}

std::string ExtensionTabUtil::GetTabStatusText(TabStatus status) {
  std::string text;
  switch (status) {
    case TAB_LOADING:
      text = keys::kStatusValueLoading;
      break;
    case TAB_COMPLETE:
      text = keys::kStatusValueComplete;
      break;
  }

  return text;
}

int ExtensionTabUtil::GetWindowIdOfTab(const TabContents* tab_contents) {
  return tab_contents->controller().window_id().id();
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const TabContents* contents) {
  // Find the tab strip and index of this guy.
  for (BrowserList::const_iterator it = BrowserList::begin();
      it != BrowserList::end(); ++it) {
    TabStripModel* tab_strip = (*it)->tabstrip_model();
    int tab_index = tab_strip->GetIndexOfTabContents(contents);
    if (tab_index != -1) {
      return ExtensionTabUtil::CreateTabValue(contents, tab_strip, tab_index);
    }
  }

  // Couldn't find it.  This can happen if the tab is being dragged.
  return ExtensionTabUtil::CreateTabValue(contents, NULL, -1);
}

ListValue* ExtensionTabUtil::CreateTabList(const Browser* browser) {
  ListValue* tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tabstrip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(ExtensionTabUtil::CreateTabValue(
        tab_strip->GetTabContentsAt(i), tab_strip, i));
  }

  return tab_list;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const TabContents* contents, TabStripModel* tab_strip, int tab_index) {
  TabStatus status = GetTabStatus(contents);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetTabId(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey,
                     ExtensionTabUtil::GetWindowIdOfTab(contents));
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kStatusKey, GetTabStatusText(status));
  result->SetBoolean(keys::kSelectedKey,
                     tab_strip && tab_index == tab_strip->selected_index());
  result->SetString(keys::kTitleKey, UTF16ToWide(contents->GetTitle()));
  result->SetBoolean(keys::kIncognitoKey,
                     contents->profile()->IsOffTheRecord());

  if (status != TAB_LOADING) {
    NavigationEntry* entry = contents->controller().GetActiveEntry();
    if (entry) {
      if (entry->favicon().is_valid())
        result->SetString(keys::kFavIconUrlKey, entry->favicon().url().spec());
    }
  }

  return result;
}

// if |populate| is true, each window gets a list property |tabs| which contains
// fully populated tab objects.
DictionaryValue* ExtensionTabUtil::CreateWindowValue(const Browser* browser,
                                                     bool populate_tabs) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetWindowId(browser));
  bool focused = false;
  if (browser->window())
    focused = browser->window()->IsActive();

  result->SetBoolean(keys::kIncognitoKey,
                     browser->profile()->IsOffTheRecord());
  result->SetBoolean(keys::kFocusedKey, focused);
  gfx::Rect bounds = browser->window()->GetRestoredBounds();

  // TODO(rafaelw): zIndex ?
  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());
  result->SetString(keys::kWindowTypeKey, GetWindowTypeText(browser->type()));

  if (populate_tabs) {
    result->Set(keys::kTabsKey, ExtensionTabUtil::CreateTabList(browser));
  }

  return result;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser, TabContents** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);
  DCHECK(tab_id);

  *contents = browser->tabstrip_model()->GetSelectedTabContents();
  if (*contents) {
    if (tab_id)
      *tab_id = ExtensionTabUtil::GetTabId(*contents);
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id, Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  TabContents** contents,
                                  int* tab_index) {
  Browser* target_browser;
  TabStripModel* target_tab_strip;
  TabContents* target_contents;
  Profile* incognito_profile =
      include_incognito ? profile->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    target_browser = *iter;
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      target_tab_strip = target_browser->tabstrip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        target_contents = target_tab_strip->GetTabContentsAt(i);
        if (target_contents->controller().session_id().id() == tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

// Windows ---------------------------------------------------------------------

bool GetWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser)
    return false;

  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetCurrentWindowFunction::RunImpl() {
  Browser* browser = GetCurrentBrowser();
  if (!browser) {
    error_ = keys::kNoCurrentWindowError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetLastFocusedWindowFunction::RunImpl() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile());
  if (!browser) {
    error_ = keys::kNoLastFocusedWindowError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));
  return true;
}

bool GetAllWindowsFunction::RunImpl() {
  bool populate_tabs = false;
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue* args = args_as_dictionary();
    if (args->HasKey(keys::kPopulateKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kPopulateKey,
          &populate_tabs));
    }
  }

  result_.reset(new ListValue());
  Profile* incognito_profile =
      include_incognito() ? profile()->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator browser = BrowserList::begin();
    browser != BrowserList::end(); ++browser) {
      // Only examine browsers in the current profile.
      if ((*browser)->profile() == profile() ||
          (*browser)->profile() == incognito_profile) {
        static_cast<ListValue*>(result_.get())->
          Append(ExtensionTabUtil::CreateWindowValue(*browser, populate_tabs));
      }
  }

  return true;
}

bool CreateWindowFunction::RunImpl() {
  GURL url;

  // Look for optional url.
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue *args = args_as_dictionary();
    std::string url_string;
    if (args->HasKey(keys::kUrlKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                  &url_string));
      url = ResolvePossiblyRelativeURL(url_string, GetExtension(),
          source_url());
      if (!url.is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kInvalidUrlError, url_string);
        return false;
      }
    }
  }

  // Try to position the new browser relative its originating browser window.
  gfx::Rect empty_bounds;
  gfx::Rect bounds;
  bool maximized;
  // The call offsets the bounds by kWindowTilePixels (defined in WindowSizer to
  // be 10)
  //
  // NOTE(rafaelw): It's ok if GetCurrentBrowser() returns NULL here.
  // GetBrowserWindowBounds will default to saved "default" values for the app.
  WindowSizer::GetBrowserWindowBounds(std::wstring(), empty_bounds,
                                      GetCurrentBrowser(), &bounds,
                                      &maximized);

  Profile* window_profile = profile();
  Browser::Type window_type = Browser::TYPE_NORMAL;

  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    const DictionaryValue *args = args_as_dictionary();

    // Any part of the bounds can optionally be set by the caller.
    int bounds_val;
    if (args->HasKey(keys::kLeftKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kLeftKey,
                                                   &bounds_val));
      bounds.set_x(bounds_val);
    }

    if (args->HasKey(keys::kTopKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kTopKey,
                                                   &bounds_val));
      bounds.set_y(bounds_val);
    }

    if (args->HasKey(keys::kWidthKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kWidthKey,
                                                   &bounds_val));
      bounds.set_width(bounds_val);
    }

    if (args->HasKey(keys::kHeightKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kHeightKey,
                                                   &bounds_val));
      bounds.set_height(bounds_val);
    }

    bool incognito = false;
    if (args->HasKey(keys::kIncognitoKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kIncognitoKey,
                                                   &incognito));
      if (incognito)
        window_profile = window_profile->GetOffTheRecordProfile();
    }

    std::string type_str;
    if (args->HasKey(keys::kWindowTypeKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kWindowTypeKey,
                                                  &type_str));
      if (type_str == keys::kWindowTypeValueNormal) {
        window_type = Browser::TYPE_NORMAL;
      } else if (type_str == keys::kWindowTypeValuePopup) {
        window_type = Browser::TYPE_POPUP;
      } else {
        EXTENSION_FUNCTION_VALIDATE(false);
      }
    }
  }

  Browser* new_window = new Browser(window_type, window_profile);
  new_window->CreateBrowserWindow();
  new_window->AddTabWithURL(url, GURL(), PageTransition::LINK, true,
                            -1, false, NULL);

  new_window->window()->SetBounds(bounds);
  new_window->window()->Show();

  // TODO(rafaelw): support |focused|, |zIndex|
  if (new_window->profile()->IsOffTheRecord() && !include_incognito()) {
    // Don't expose incognito windows if the extension isn't allowed.
    result_.reset(Value::CreateNullValue());
  } else {
    result_.reset(ExtensionTabUtil::CreateWindowValue(new_window, false));
  }

  return true;
}

bool UpdateWindowFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = args_as_list();
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &window_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser)
    return false;

  gfx::Rect bounds = browser->window()->GetRestoredBounds();
  // Any part of the bounds can optionally be set by the caller.
  int bounds_val;
  if (update_props->HasKey(keys::kLeftKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kLeftKey,
        &bounds_val));
    bounds.set_x(bounds_val);
  }

  if (update_props->HasKey(keys::kTopKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kTopKey,
        &bounds_val));
    bounds.set_y(bounds_val);
  }

  if (update_props->HasKey(keys::kWidthKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kWidthKey,
        &bounds_val));
    bounds.set_width(bounds_val);
  }

  if (update_props->HasKey(keys::kHeightKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kHeightKey,
        &bounds_val));
    bounds.set_height(bounds_val);
  }

  browser->window()->SetBounds(bounds);
  // TODO(rafaelw): Support |focused|.
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));

  return true;
}

bool RemoveWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
  if (!browser)
    return false;

  browser->CloseWindow();

  return true;
}

// Tabs ------------------------------------------------------------------------

bool GetSelectedTabFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;

  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();
  TabContents* contents = tab_strip->GetSelectedTabContents();
  if (!contents) {
    error_ = keys::kNoSelectedTabError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip,
      tab_strip->selected_index()));
  return true;
}

bool GetAllTabsInWindowFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  result_.reset(ExtensionTabUtil::CreateTabList(browser));

  return true;
}

bool CreateTabFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = args_as_dictionary();

  Browser *browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (args->HasKey(keys::kWindowIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(
        keys::kWindowIdKey, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  std::string url_string;
  GURL url;
  if (args->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                &url_string));
    url = ResolvePossiblyRelativeURL(url_string, GetExtension(), source_url());
    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       url_string);
      return false;
    }
  }

  // Default to foreground for the new tab. The presence of 'selected' property
  // will override this default.
  bool selected = true;
  if (args->HasKey(keys::kSelectedKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kSelectedKey,
                                                 &selected));
  // If index is specified, honor the value, but keep it bound to
  // 0 <= index <= tab_strip->count()
  int index = -1;
  if (args->HasKey(keys::kIndexKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kIndexKey,
                                                 &index));

  // We can't load extension URLs into incognito windows. Special case to
  // fall back to a normal window.
  if (url.SchemeIs(chrome::kExtensionScheme) &&
      browser->profile()->IsOffTheRecord()) {
    browser = Browser::GetOrCreateTabbedBrowser(
        browser->profile()->GetOriginalProfile());
    DCHECK(browser);
  }

  TabStripModel* tab_strip = browser->tabstrip_model();

  if (index < 0) {
    // Default insert behavior.
    index = -1;
  }
  if (index > tab_strip->count()) {
    index = tab_strip->count();
  }

  TabContents* contents = browser->AddTabWithURL(url, GURL(),
      PageTransition::LINK, selected, index, true, NULL);
  index = tab_strip->GetIndexOfTabContents(contents);

  if (selected)
    contents->Focus();

  // Return data about the newly created tab.
  if (has_callback())
    result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip, index));

  return true;
}

bool GetTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  TabStripModel* tab_strip = NULL;
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_))
    return false;

  result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip,
      tab_index));
  return true;
}

bool UpdateTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = args_as_list();
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &tab_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  TabStripModel* tab_strip = NULL;
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  NULL, &tab_strip, &contents, &tab_index, &error_))
    return false;

  NavigationController& controller = contents->controller();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url different.
  std::string url_string;
  if (update_props->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetString(
        keys::kUrlKey, &url_string));
    GURL url = ResolvePossiblyRelativeURL(url_string, GetExtension(),
        source_url());

    if (!url.is_valid()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                       url_string);
      return false;
    }

    // JavaScript URLs can do the same kinds of things as cross-origin XHR, so
    // we need to check host permissions before allowing them.
    if (url.SchemeIs(chrome::kJavaScriptScheme)) {
      if (!GetExtension()->CanExecuteScriptOnHost(contents->GetURL(), &error_))
        return false;

      // TODO(aa): How does controller queue URLs? Is there any chance that this
      // JavaScript URL will end up applying to something other than
      // controller->GetURL()?
    }

    if (tab_strip->IsTabPinned(tab_index)) {
      // Don't allow changing the url of pinned tabs.
      error_ = keys::kCannotUpdatePinnedTab;
      return false;
    }

    controller.LoadURL(url, GURL(), PageTransition::LINK);

    // The URL of a tab contents never actually changes to a JavaScript URL, so
    // this check only makes sense in other cases.
    if (!url.SchemeIs(chrome::kJavaScriptScheme))
      DCHECK_EQ(url.spec(), contents->GetURL().spec());
  }

  bool selected = false;
  // TODO(rafaelw): Setting |selected| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (update_props->HasKey(keys::kSelectedKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kSelectedKey,
        &selected));
    if (selected) {
      if (tab_strip->selected_index() != tab_index) {
        tab_strip->SelectTabContentsAt(tab_index, false);
        DCHECK_EQ(contents, tab_strip->GetSelectedTabContents());
      }
      contents->Focus();
    }
  }

  if (has_callback())
    result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip,
        tab_index));

  return true;
}

bool MoveTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = args_as_list();
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &tab_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  int new_index;
  EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
      keys::kIndexKey, &new_index));
  EXTENSION_FUNCTION_VALIDATE(new_index >= 0);

  Browser* source_browser = NULL;
  TabStripModel* source_tab_strip = NULL;
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  &source_browser, &source_tab_strip, &contents,
                  &tab_index, &error_))
    return false;

  if (update_props->HasKey(keys::kWindowIdKey)) {
    Browser* target_browser;
    int window_id;
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kWindowIdKey, &window_id));
    target_browser = GetBrowserInProfileWithId(profile(), window_id,
                                               include_incognito(), &error_);
    if (!target_browser)
      return false;

    // If windowId is different from the current window, move between windows.
    if (ExtensionTabUtil::GetWindowId(target_browser) !=
        ExtensionTabUtil::GetWindowId(source_browser)) {
      TabStripModel* target_tab_strip = target_browser->tabstrip_model();
      contents = source_tab_strip->DetachTabContentsAt(tab_index);
      if (!contents) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, IntToString(tab_id));
        return false;
      }

      // Clamp move location to the last position.
      // This is ">" because it can append to a new index position.
      if (new_index > target_tab_strip->count())
        new_index = target_tab_strip->count();

      target_tab_strip->InsertTabContentsAt(new_index, contents,
          false, true);

      if (has_callback())
        result_.reset(ExtensionTabUtil::CreateTabValue(contents,
            target_tab_strip, new_index));

      return true;
    }
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  if (new_index >= source_tab_strip->count())
    new_index = source_tab_strip->count() - 1;

  if (new_index != tab_index)
    source_tab_strip->MoveTabContentsAt(tab_index, new_index, false);

  if (has_callback())
    result_.reset(ExtensionTabUtil::CreateTabValue(contents, source_tab_strip,
        new_index));
  return true;
}


bool RemoveTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  Browser* browser = NULL;
  TabContents* contents = NULL;
  if (!GetTabById(tab_id, profile(), include_incognito(),
                  &browser, NULL, &contents, NULL, &error_))
    return false;

  int tab_index = browser->GetIndexOfController(&contents->controller());
  if (browser->tabstrip_model()->IsPhantomTab(tab_index)) {
    // Don't allow closing phantom tabs.
    error_ = keys::kCannotRemovePhantomTab;
    return false;
  }

  // Close the tab in this convoluted way, since there's a chance that the tab
  // is being dragged, or we're in some other nested event loop. This code path
  // should ensure that the tab is safely closed under such circumstances,
  // whereas |Browser::CloseTabContents()| does not.
  RenderViewHost* render_view_host = contents->render_view_host();
  render_view_host->delegate()->Close(render_view_host);
  return true;
}

bool CaptureVisibleTabFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;

  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id,
                                        include_incognito(), &error_);
  } else {
    browser = GetCurrentBrowser();
  }

  if (!browser) {
    error_ = keys::kNoCurrentWindowError;
    return false;
  }

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents) {
    error_ = keys::kInternalVisibleTabCaptureError;
    return false;
  }
  RenderViewHost* render_view_host = tab_contents->render_view_host();

  // If a backing store is cached for the tab we want to capture,
  // and it can be copied into a bitmap, then use it to generate the image.
  BackingStore* backing_store = render_view_host->GetBackingStore(false);
  if (backing_store && CaptureSnapshotFromBackingStore(backing_store))
    return true;

  // Ask the renderer for a snapshot of the tab.
  render_view_host->CaptureSnapshot();
  registrar_.Add(this,
                 NotificationType::TAB_SNAPSHOT_TAKEN,
                 NotificationService::AllSources());
  AddRef();  // Balanced in CaptureVisibleTabFunction::Observe().

  return true;
}

// Build the image of a tab's contents out of a backing store.
// This may fail if we can not copy a backing store into a bitmap.
// For example, some uncommon X11 visual modes are not supported by
// CopyFromBackingStore().
bool CaptureVisibleTabFunction::CaptureSnapshotFromBackingStore(
    BackingStore* backing_store) {

  skia::PlatformCanvas temp_canvas;
  if (!backing_store->CopyFromBackingStore(gfx::Rect(gfx::Point(0, 0),
                                                     backing_store->size()),
                                           &temp_canvas)) {
    return false;
  }
  SendResultFromBitmap(
      temp_canvas.getTopPlatformDevice().accessBitmap(false));
  return true;
}

// If a backing store was not available in CaptureVisibleTabFunction::RunImpl,
// than the renderer was asked for a snapshot.  Listen for a notification
// that the snapshot is available.
void CaptureVisibleTabFunction::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_SNAPSHOT_TAKEN);

  const SkBitmap *screen_capture = Details<const SkBitmap>(details).ptr();
  const bool error = screen_capture->empty();

  if (error) {
    error_ = keys::kInternalVisibleTabCaptureError;
    SendResponse(false);
  } else {
    SendResultFromBitmap(*screen_capture);
  }

  Release();  // Balanced in CaptureVisibleTabFunction::RunImpl().
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void CaptureVisibleTabFunction::SendResultFromBitmap(
    const SkBitmap& screen_capture) {
  scoped_refptr<RefCountedBytes> jpeg_data(new RefCountedBytes);
  SkAutoLockPixels screen_capture_lock(screen_capture);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(screen_capture.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, screen_capture.width(),
      screen_capture.height(),
      static_cast<int>(screen_capture.rowBytes()), 90,
      &jpeg_data->data);
  if (!encoded) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kInternalVisibleTabCaptureError, "");
    SendResponse(false);
    return;
  }

  std::string base64_result;
  std::string stream_as_string;
  stream_as_string.resize(jpeg_data->data.size());
  memcpy(&stream_as_string[0],
      reinterpret_cast<const char*>(&jpeg_data->data[0]),
      jpeg_data->data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(0, "data:image/jpg;base64,");
  result_.reset(new StringValue(base64_result));
  SendResponse(true);
}

bool DetectTabLanguageFunction::RunImpl() {
  int tab_id = 0;
  Browser* browser = NULL;
  TabContents* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));
    if (!GetTabById(tab_id, profile(), include_incognito(),
                    &browser, NULL, &contents, NULL, &error_)) {
      return false;
    }
    if (!browser || !contents)
      return false;
  } else {
    browser = GetCurrentBrowser();
    if (!browser)
      return false;
    contents = browser->tabstrip_model()->GetSelectedTabContents();
    if (!contents)
      return false;
  }

  if (contents->controller().needs_reload()) {
    // If the tab hasn't been loaded, such as happens with phantom tabs, don't
    // wait for the tab to load, instead return.
    error_ = keys::kCannotDetermineLanguageOfUnloadedTab;
    return false;
  }

  AddRef();  // Balanced in GotLanguage()

  if (!contents->language_state().original_language().empty()) {
    // Delay the callback invocation until after the current JS call has
    // returned.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DetectTabLanguageFunction::GotLanguage,
        contents->language_state().original_language()));
    return true;
  }
  // The tab contents does not know its language yet.  Let's  wait until it
  // receives it, or until the tab is closed/navigates to some other page.
  registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
                 Source<TabContents>(contents));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(&(contents->controller())));
  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&(contents->controller())));
  return true;
}

void DetectTabLanguageFunction::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  std::string language;
  if (type == NotificationType::TAB_LANGUAGE_DETERMINED)
    language = *Details<std::string>(details).ptr();

  registrar_.RemoveAll();

  // Call GotLanguage in all cases as we want to guarantee the callback is
  // called for every API call the extension made.
  GotLanguage(language);
}

void DetectTabLanguageFunction::GotLanguage(const std::string& language) {
  result_.reset(Value::CreateStringValue(language.c_str()));
  SendResponse(true);

  Release();  // Balanced in Run()
}

// static helpers

static Browser* GetBrowserInProfileWithId(Profile* profile,
                                          const int window_id,
                                          bool include_incognito,
                                          std::string* error_message) {
  Profile* incognito_profile =
      include_incognito ? profile->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator browser = BrowserList::begin();
       browser != BrowserList::end(); ++browser) {
    if (((*browser)->profile() == profile ||
         (*browser)->profile() == incognito_profile) &&
        ExtensionTabUtil::GetWindowId(*browser) == window_id)
      return *browser;
  }

  if (error_message)
    *error_message= ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, IntToString(window_id));

  return NULL;
}

static bool GetTabById(int tab_id, Profile* profile,
                       bool include_incognito,
                       Browser** browser,
                       TabStripModel** tab_strip,
                       TabContents** contents,
                       int* tab_index,
                       std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                   browser, tab_strip, contents, tab_index))
    return true;

  if (error_message)
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, IntToString(tab_id));

  return false;
}

static std::string GetWindowTypeText(Browser::Type type) {
  // Note: for app popups, we report "app".
  if ((type & Browser::TYPE_APP) != 0)
    return keys::kWindowTypeValueApp;
  if ((type & Browser::TYPE_POPUP) != 0)
    return keys::kWindowTypeValuePopup;

  DCHECK(type == Browser::TYPE_NORMAL);
  return keys::kWindowTypeValueNormal;
}

// These are histogram buckets passed to UMA in the following test of relative
// URL use in the tabs & windows API.
enum ExtensionAPIRelativeURLUse {
  ABSOLUTE_URL,
  RELATIVE_URL_RESOLUTIONS_DIFFER,
  RELATIVE_URL_RESOLUTIONS_AGREE,
  EXTENSION_API_RELATIVE_URL_USE_MAX_VALUE
};

static GURL ResolvePossiblyRelativeURL(std::string url_string,
                                       Extension* extension,
                                       const GURL& source_url) {
  ExtensionAPIRelativeURLUse use_type = ABSOLUTE_URL;

  GURL url = GURL(url_string);
  if (!url.is_valid()) {
    url = extension->GetResourceURL(url_string);
    GURL resolved_url = source_url.Resolve(url_string);

    // Note: It's possible that GetResourceURL() returned an invalid URL
    // meaning that the url_string contained some kind of invalid characters.
    // The first test for url.is_valid on the next line puts this case into
    // the resolutions agree bucket -- in the sense that both resolutions would
    // have resulted in an invald URL and thus an error being returned to the
    // caller.
    use_type = url.is_valid() && (url != resolved_url) ?
        RELATIVE_URL_RESOLUTIONS_DIFFER : RELATIVE_URL_RESOLUTIONS_AGREE;
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.APIUse_RelativeURL", use_type,
      EXTENSION_API_RELATIVE_URL_USE_MAX_VALUE);

  return url;
}

