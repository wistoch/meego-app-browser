// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>

#include "chrome/browser/browser.h"

#include "base/command_line.h"
#include "base/idle_timer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/dock_info.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/interstitial_page.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/ssl_error_info.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/download_tab_view.h"
#include "chrome/browser/views/go_button.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/new_profile_dialog.h"
#include "chrome/browser/views/select_profile_dialog.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/web_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/win_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using base::TimeDelta;

static BrowserList g_browserlist;

// How long we wait before updating the browser chrome while loading a page.
static const int kUIUpdateCoalescingTimeMS = 200;

// Idle time before helping prune memory consumption.
static const int kBrowserReleaseMemoryInterval = 30;  // In seconds.

// How much horizontal and vertical offset there is between newly opened
// windows.
static const int kWindowTilePixels = 20;

///////////////////////////////////////////////////////////////////////////////

// A task to reduce the working set of the plugins.
class ReducePluginsWorkingSetTask : public Task {
 public:
  virtual void Run() {
    for (PluginProcessHostIterator iter; !iter.Done(); ++iter) {
      PluginProcessHost* plugin = const_cast<PluginProcessHost*>(*iter);
      DCHECK(plugin->process());
      base::Process process(plugin->process());
      process.ReduceWorkingSet();
    }
  }
};

// A browser task to run when the user is not using the browser.
// In our case, we're trying to be nice to the operating system and release
// memory not in use.
class BrowserIdleTimer : public base::IdleTimer {
 public:
  BrowserIdleTimer()
      : base::IdleTimer(TimeDelta::FromSeconds(kBrowserReleaseMemoryInterval),
                        false) {
  }

  virtual void OnIdle() {
    // We're idle.  Release browser and renderer unused pages.

    // Handle the Browser.
    base::Process process(GetCurrentProcess());
    process.ReduceWorkingSet();

    // Handle the Renderer(s).
    RenderProcessHost::iterator renderer_iter;
    for (renderer_iter = RenderProcessHost::begin(); renderer_iter !=
         RenderProcessHost::end(); renderer_iter++) {
       base::Process process = renderer_iter->second->process();
       process.ReduceWorkingSet();
    }

    // Handle the Plugin(s).  We need to iterate through the plugin processes
    // on the IO thread because that thread manages the plugin process
    // collection.
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        new ReducePluginsWorkingSetTask());
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Browser::UIUpdate {
  UIUpdate(const TabContents* src, unsigned flags)
      : source(src),
        changed_flags(flags) {
  }

  // The source of the update.
  const TabContents* source;

  // What changed in the UI.
  unsigned changed_flags;
};

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

Browser::Browser(Type type, Profile* profile)
    : type_(type),
      profile_(profile),
      window_(NULL),
      tabstrip_model_(this, profile),
      controller_(this),
      toolbar_model_(this),
      chrome_updater_factory_(this),
      is_attempting_to_close_browser_(false),
      override_maximized_(false),
      method_factory_(this),
      idle_task_(new BrowserIdleTimer) {
  tabstrip_model_.AddObserver(this);

  NotificationService::current()->AddObserver(
      this, NOTIFY_SSL_STATE_CHANGED, NotificationService::AllSources());

  InitCommandState();
  BrowserList::AddBrowser(this);

  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs(), NULL);

  // Trim browser memory on idle for low & medium memory models.
  if (g_browser_process->memory_model() < BrowserProcess::HIGH_MEMORY_MODEL)
    idle_task_->Start();
}

Browser::~Browser() {
  // The tab strip should be empty at this point.
  DCHECK(tabstrip_model_.empty());
  tabstrip_model_.RemoveObserver(this);

  BrowserList::RemoveBrowser(this);

  if (!BrowserList::HasBrowserWithProfile(profile_)) {
    // We're the last browser window with this profile. We need to nuke the
    // TabRestoreService, which will start the shutdown of the
    // NavigationControllers and allow for proper shutdown. If we don't do this
    // chrome won't shutdown cleanly, and may end up crashing when some
    // thread tries to use the IO thread (or another thread) that is no longer
    // valid.
    profile_->ResetTabRestoreService();
  }

  SessionService* session_service = profile_->GetSessionService();
  if (session_service)
    session_service->WindowClosed(session_id_);

  TabRestoreService* tab_restore_service = profile()->GetTabRestoreService();
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(this);

  NotificationService::current()->RemoveObserver(
      this, NOTIFY_SSL_STATE_CHANGED, NotificationService::AllSources());

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActive()) {
    // We reuse the OTR cookie store across OTR windows.  If the last OTR
    // window is closed, then we want to wipe the cookie store clean, so when
    // an OTR window is open again, it starts with an empty cookie store.  This
    // also frees up the memory that the OTR cookies were using.  OTR never
    // loads or writes persistent cookies (there is no backing store), so we
    // can just delete all of the cookies in the store.
    profile_->GetRequestContext()->cookie_store()->DeleteAll(false);
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

// static
Browser* Browser::Create(Profile* profile) {
  Browser* browser = new Browser(TYPE_NORMAL, profile);
  browser->CreateBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateForPopup(Profile* profile) {
  Browser* browser = new Browser(TYPE_POPUP, profile);
  browser->CreateBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateForApp(const std::wstring& app_name,
                               Profile* profile) {
  Browser* browser = new Browser(TYPE_APP, profile);
  browser->app_name_ = app_name;
  browser->CreateBrowserWindow();
  return browser;
}

void Browser::CreateBrowserWindow() {
  DCHECK(!window_);
  window_ = BrowserWindow::CreateBrowserWindow(this);

  // Show the First Run information bubble if we've been told to.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->IsPrefRegistered(prefs::kShouldShowFirstRunBubble) &&
      local_state->GetBoolean(prefs::kShouldShowFirstRunBubble)) {
    // Reset the preference so we don't show the bubble for subsequent windows.
    local_state->ClearPref(prefs::kShouldShowFirstRunBubble);
    GetLocationBarView()->ShowFirstRunBubble();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Creation Helpers:

// static
void Browser::OpenEmptyWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->AddBlankTab(true);
  browser->window()->Show();
}

// static
void Browser::OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Profile* off_the_record_profile = profile->GetOffTheRecordProfile();
  Browser* browser = BrowserList::FindBrowserWithType(
      off_the_record_profile,
      TYPE_NORMAL);
  if (!browser)
    browser = Browser::Create(off_the_record_profile);
  // TODO(eroman): should we have referrer here?
  browser->AddTabWithURL(url, GURL(), PageTransition::LINK, true, NULL);
  browser->window()->Show();
}

// static
void Browser::OpenWebApplication(Profile* profile, WebApp* app) {
  const std::wstring& app_name =
      app->name().empty() ? ComputeApplicationNameFromURL(app->url()) :
                            app->name();
  RegisterAppPrefs(app_name);

  Browser* browser = Browser::CreateForApp(app_name, profile);
  browser->AddWebApplicationTab(profile, app, false);
  browser->window()->Show();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command API:

bool Browser::SupportsCommand(int id) const {
  return controller_.SupportsCommand(id);
}

bool Browser::IsCommandEnabled(int id) const {
  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (!GetSelectedTabContents())
    return false;

  // TODO(pkasting): Should maybe enable/disable this in
  // NavigationStateChanged()?
  if (id == IDC_STOP)
    return IsCurrentPageLoading();

  return controller_.IsCommandEnabled(id);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, State Storage and Retrieval for UI:

std::wstring Browser::GetWindowPlacementKey() const {
  std::wstring name(prefs::kBrowserWindowPlacement);
  if (!app_name_.empty()) {
    name.append(L"_");
    name.append(app_name_);
  }
  return name;
}

bool Browser::ShouldSaveWindowPlacement() const {
  // We don't save window position for popups.
  return type() != TYPE_POPUP;
}

void Browser::SaveWindowPlacement(const gfx::Rect& bounds, bool maximized) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  if (profile()->HasSessionService()) {
    SessionService* session_service = profile()->GetSessionService();
    if (session_service)
      session_service->SetWindowBounds(session_id_, bounds, maximized);
  }
}

gfx::Rect Browser::GetSavedWindowBounds() const {
  CommandLine parsed_command_line;
  bool record_mode = parsed_command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = parsed_command_line.HasSwitch(switches::kPlaybackMode);
  if (record_mode || playback_mode) {
    // In playback/record mode we always fix the size of the browser and
    // move it to (0,0).  The reason for this is two reasons:  First we want
    // resize/moves in the playback to still work, and Second we want
    // playbacks to work (as much as possible) on machines w/ different
    // screen sizes.
    return gfx::Rect(0, 0, 800, 600);
  }

  gfx::Rect restored_bounds = override_bounds_;
  bool maximized;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds,
                                      &restored_bounds, &maximized);
  return restored_bounds;
}

// TODO(beng): obtain maximized state some other way so we don't need to go
//             through all this hassle.
bool Browser::GetSavedMaximizedState() const {
  if (CommandLine().HasSwitch(switches::kStartMaximized))
    return true;

  gfx::Rect restored_bounds;
  bool maximized = override_maximized_;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds,
                                      &restored_bounds, &maximized);
  return maximized;
}

SkBitmap Browser::GetCurrentPageIcon() const {
  return GetSelectedTabContents()->GetFavIcon();
}

std::wstring Browser::GetCurrentPageTitle() const {
  TabContents* contents = tabstrip_model_.GetSelectedTabContents();
  std::wstring title;

  // |contents| can be NULL because GetCurrentPageTitle is called by the window
  // during the window's creation (before tabs have been added).
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }
  if (title.empty())
    title = l10n_util::GetString(IDS_TAB_UNTITLED_TITLE);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, title);
}

bool Browser::IsCurrentPageLoading() const {
  // GetSelectedTabContents can return NULL for example under Purify when
  // the animations are running slowly and this function is called on a timer
  // through LoadingAnimationCallback.
  TabContents* tab_contents = GetSelectedTabContents();
  return tab_contents && tab_contents->is_loading();
}

// static
void Browser::FormatTitleForDisplay(std::wstring* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) !=
         std::wstring::npos) {
    title->replace(match_index, 1, L"");
    current_index = match_index;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling:

bool Browser::ShouldCloseWindow() {
  if (HasCompletedUnloadProcessing()) {
    return true;
  }
  is_attempting_to_close_browser_ = true;

  for (int i = 0; i < tab_count(); ++i) {
    if (tabstrip_model_.TabHasUnloadListener(i)) {
      TabContents* tab = GetTabContentsAt(i);
      tabs_needing_before_unload_fired_.insert(tab);
    }
  }

  if (tabs_needing_before_unload_fired_.empty())
    return true;

  ProcessPendingTabs();
  return false;
}

void Browser::OnWindowClosing() {
  if (!ShouldCloseWindow())
    return;

  if (BrowserList::size() == 1)
    browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);

  // Don't use HasSessionService here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionService* session_service = profile()->GetSessionService();
  if (session_service)
    session_service->WindowClosing(session_id());

  TabRestoreService* tab_restore_service = profile()->GetTabRestoreService();
  if (tab_restore_service)
    tab_restore_service->BrowserClosing(this);

  CloseAllTabs();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Tab adding/showing functions:

TabContents* Browser::AddTabWithURL(
    const GURL& url, const GURL& referrer, PageTransition::Type transition,
    bool foreground, SiteInstance* instance) {
  if (type_ == TYPE_APP && tabstrip_model_.count() == 1) {
    NOTREACHED() << "Cannot add a tab in a mono tab application.";
    return NULL;
  }

  GURL url_to_load = url;
  if (url_to_load.is_empty())
    url_to_load = GetHomePage();
  TabContents* contents =
      CreateTabContentsForURL(url_to_load, referrer, profile_, transition,
                              false, instance);
  tabstrip_model_.AddTabContents(contents, -1, transition, foreground);
  // By default, content believes it is not hidden.  When adding contents
  // in the background, tell it that it's hidden.
  if (!foreground)
    contents->WasHidden();
  return contents;
}

TabContents* Browser::AddWebApplicationTab(Profile* profile,
                                           WebApp* web_app,
                                           bool lazy) {
  DCHECK(web_app);

  // TODO(acw): Do we need an "application launched" transition type?
  // TODO(creis): Should we reuse the current instance (ie. process) here?
  TabContents* contents =
      CreateTabContentsForURL(web_app->url(), GURL(), profile,
                              PageTransition::LINK, lazy, NULL);
  if (contents->AsWebContents())
    contents->AsWebContents()->SetWebApp(web_app);

  if (lazy) {
    contents->controller()->LoadURLLazily(
        web_app->url(), GURL(), PageTransition::LINK, web_app->name(), NULL);
  }
  tabstrip_model_.AddTabContents(contents, -1, PageTransition::LINK, !lazy);
  return contents;
}

TabContents* Browser::AddTabWithNavigationController(
    NavigationController* ctrl, PageTransition::Type type) {
  TabContents* tc = ctrl->active_contents();
  tabstrip_model_.AddTabContents(tc, -1, type, true);
  return tc;
}

NavigationController* Browser::AddRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    bool select) {
  NavigationController* restored_controller =
      BuildRestoredNavigationController(navigations, selected_navigation);

  tabstrip_model_.InsertTabContentsAt(
      tab_index,
      restored_controller->active_contents(),
      select, false);
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(restored_controller);
  }
  return restored_controller;
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation) {
  NavigationController* restored_controller =
      BuildRestoredNavigationController(navigations, selected_navigation);

  tabstrip_model_.ReplaceNavigationControllerAt(
      tabstrip_model_.selected_index(),
      restored_controller);
}

void Browser::ShowNativeUITab(const GURL& url) {
  int i, c;
  TabContents* tc;
  for (i = 0, c = tabstrip_model_.count(); i < c; ++i) {
    tc = tabstrip_model_.GetTabContentsAt(i);
    if (tc->type() == TAB_CONTENTS_NATIVE_UI &&
        tc->GetURL() == url) {
      tabstrip_model_.SelectTabContentsAt(i, false);
      return;
    }
  }

  TabContents* contents = CreateTabContentsForURL(url, GURL(), profile_,
                                                  PageTransition::LINK, false,
                                                  NULL);
  AddNewContents(NULL, contents, NEW_FOREGROUND_TAB, gfx::Rect(), true);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

void Browser::GoBack() {
  UserMetrics::RecordAction(L"Back", profile_);

  // If we are showing an interstitial, just hide it.
  TabContents* current_tab = GetSelectedTabContents();
  WebContents* web_contents = current_tab->AsWebContents();
  if (web_contents && web_contents->showing_interstitial_page()) {
    // Pressing back on an interstitial page means "don't proceed".
    web_contents->interstitial_page()->DontProceed();
    return;
  }
  if (current_tab->controller()->CanGoBack())
    current_tab->controller()->GoBack();
}

void Browser::GoForward() {
  UserMetrics::RecordAction(L"Forward", profile_);
  if (GetSelectedTabContents()->controller()->CanGoForward())
    GetSelectedTabContents()->controller()->GoForward();
}

void Browser::Reload() {
  UserMetrics::RecordAction(L"Reload", profile_);

  // If we are showing an interstitial, treat this as an OpenURL.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab) {
    WebContents* web_contents = current_tab->AsWebContents();
    if (web_contents && web_contents->showing_interstitial_page()) {
      NavigationEntry* entry = current_tab->controller()->GetActiveEntry();
      DCHECK(entry);  // Should exist if interstitial is showing.
      OpenURL(entry->url(), GURL(), CURRENT_TAB, PageTransition::RELOAD);
      return;
    }
  }

  if (current_tab) {
    // As this is caused by a user action, give the focus to the page.
    current_tab->Focus();
    current_tab->controller()->Reload(true);
  }
}

void Browser::Home() {
  UserMetrics::RecordAction(L"Home", profile_);
  GURL homepage_url = GetHomePage();
  GetSelectedTabContents()->controller()->LoadURL(
      homepage_url, GURL(), PageTransition::AUTO_BOOKMARK);
}

void Browser::OpenCurrentURL() {
  UserMetrics::RecordAction(L"LoadURL", profile_);
  LocationBarView* lbv = GetLocationBarView();
  if (lbv) {
    OpenURL(GURL(lbv->location_input()), GURL(), lbv->disposition(),
            lbv->transition());
  }
}

void Browser::Go() {
  UserMetrics::RecordAction(L"Go", profile_);
  LocationBarView* lbv = GetLocationBarView();
  if (lbv)
    lbv->location_entry()->model()->AcceptInput(CURRENT_TAB, false);
}

void Browser::Stop() {
  UserMetrics::RecordAction(L"Stop", profile_);
  GetSelectedTabContents()->AsWebContents()->Stop();
}

void Browser::NewWindow() {
  UserMetrics::RecordAction(L"NewWindow", profile_);
  Browser::OpenEmptyWindow(profile_->GetOriginalProfile());
}

void Browser::NewIncognitoWindow() {
  UserMetrics::RecordAction(L"NewIncognitoWindow", profile_);
  Browser::OpenEmptyWindow(profile_->GetOffTheRecordProfile());
}

void Browser::NewProfileWindowByIndex(int index) {
  UserMetrics::RecordAction(L"NewProfileWindowByIndex", profile_);
  UserDataManager::Get()->LaunchChromeForProfile(index);
}

void Browser::CloseWindow() {
  UserMetrics::RecordAction(L"CloseWindow", profile_);
  window_->Close();
}

void Browser::NewTab() {
  UserMetrics::RecordAction(L"NewTab", profile_);
  if (type() == TYPE_NORMAL) {
    AddBlankTab(true);
  } else {
    Browser* b = GetOrCreateTabbedBrowser();
    b->AddBlankTab(true);
    b->window()->Show();
  }
}

void Browser::CloseTab() {
  UserMetrics::RecordAction(L"CloseTab_Accelerator", profile_);
  tabstrip_model_.CloseTabContentsAt(tabstrip_model_.selected_index());
}

void Browser::SelectNextTab() {
  UserMetrics::RecordAction(L"SelectNextTab", profile_);
  tabstrip_model_.SelectNextTab();
}

void Browser::SelectPreviousTab() {
  UserMetrics::RecordAction(L"SelectPrevTab", profile_);
  tabstrip_model_.SelectPreviousTab();
}

void Browser::SelectNumberedTab(int index) {
  if (index < tab_count()) {
    UserMetrics::RecordAction(L"SelectNumberedTab", profile_);
    tabstrip_model_.SelectTabContentsAt(index, true);
  }
}

void Browser::SelectLastTab() {
  UserMetrics::RecordAction(L"SelectLastTab", profile_);
  tabstrip_model_.SelectLastTab();
}

void Browser::DuplicateTab() {
  UserMetrics::RecordAction(L"Duplicate", profile_);
  DuplicateContentsAt(selected_index());
}

void Browser::RestoreTab() {
  UserMetrics::RecordAction(L"RestoreTab", profile_);
  TabRestoreService* service = profile_->GetTabRestoreService();
  if (!service)
    return;

  service->RestoreMostRecentEntry(this);
}

void Browser::ConvertPopupToTabbedBrowser() {
  UserMetrics::RecordAction(L"ShowAsTab", profile_);

  if (type() != TYPE_NORMAL) {
    NOTREACHED();
    return;
  }

  int tab_strip_index = tabstrip_model_.selected_index();
  TabContents* contents = tabstrip_model_.DetachTabContentsAt(tab_strip_index);
  Browser* browser = Browser::Create(profile_);
  browser->tabstrip_model()->AppendTabContents(contents, true);
  browser->window()->Show();
}

void Browser::Exit() {
  UserMetrics::RecordAction(L"Exit", profile_);
  BrowserList::CloseAllBrowsers(true);
}

void Browser::BookmarkCurrentPage() {
  if (type() != TYPE_NORMAL) {
    // We disable bookmarking for types other than normal and shouldn't get
    // here.
    NOTREACHED();
    return;
  }

  UserMetrics::RecordAction(L"Star", profile_);

  TabContents* tab = GetSelectedTabContents();
  if (!tab->AsWebContents())
    return;

  WebContents* rvh = tab->AsWebContents();
  BookmarkModel* model = tab->profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  NavigationEntry* entry = rvh->controller()->GetActiveEntry();
  if (!entry)
    return;  // Can't star if there is no URL.
  const GURL& url = entry->display_url();
  if (url.is_empty() || !url.is_valid())
    return;

  if (window_->GetStarButton()) {
    if (!window_->GetStarButton()->is_bubble_showing()) {
      const bool newly_bookmarked = !model->IsBookmarked(url);
      if (newly_bookmarked) {
        model->SetURLStarred(url, entry->title(), true);
        if (!model->IsBookmarked(url)) {
          // Starring failed. This shouldn't happen.
          NOTREACHED();
          return;
        }
      }
      window_->GetStarButton()->ShowStarBubble(url, newly_bookmarked);
    }
  } else if (model->IsBookmarked(url)) {
    // If we can't find the star button and the user wanted to unstar it,
    // go ahead and unstar it without showing the bubble.
    model->SetURLStarred(url, std::wstring(), false);
  }
}

void Browser::ViewSource() {
  UserMetrics::RecordAction(L"ViewSource", profile_);

  TabContents* current_tab = GetSelectedTabContents();
  NavigationEntry* entry = current_tab->controller()->GetLastCommittedEntry();
  if (entry) {
    GURL url("view-source:" + entry->url().spec());
    AddTabWithURL(url, GURL(), PageTransition::LINK, true, NULL);
  }
}

void Browser::ClosePopups() {
  UserMetrics::RecordAction(L"CloseAllSuppressedPopups", profile_);
  GetSelectedTabContents()->CloseAllSuppressedPopups();
}

void Browser::Print() {
  UserMetrics::RecordAction(L"PrintPreview", profile_);
  GetSelectedTabContents()->AsWebContents()->PrintPreview();
}

void Browser::SavePage() {
  UserMetrics::RecordAction(L"SavePage", profile_);
  GetSelectedTabContents()->AsWebContents()->OnSavePage();
}

void Browser::ToggleEncodingAutoDetect() {
  UserMetrics::RecordAction(L"AutoDetectChange", profile_);
  encoding_auto_detect_.SetValue(!encoding_auto_detect_.GetValue());
  // Reload the page so we can try to auto-detect the charset.
  Reload();
}

void Browser::OverrideEncoding(int encoding_id) {
  UserMetrics::RecordAction(L"OverrideEncoding", profile_);
  const std::wstring selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
  TabContents* current_tab = GetSelectedTabContents();
  if (!selected_encoding.empty() && current_tab->AsWebContents())
     current_tab->AsWebContents()->override_encoding(selected_encoding);
  // Update the list of recently selected encodings.
  std::wstring new_selected_encoding_list;
  if (CharacterEncoding::UpdateRecentlySelectdEncoding(
          profile_->GetPrefs()->GetString(prefs::kRecentlySelectedEncoding),
          encoding_id,
          &new_selected_encoding_list)) {
    profile_->GetPrefs()->SetString(prefs::kRecentlySelectedEncoding,
                                    new_selected_encoding_list);
  }
}

// TODO(devint): http://b/issue?id=1117225 Cut, Copy, and Paste are always
// enabled in the page menu regardless of whether the command will do
// anything. When someone selects the menu item, we just act as if they hit
// the keyboard shortcut for the command by sending the associated key press
// to windows. The real fix to this bug is to disable the commands when they
// won't do anything. We'll need something like an overall clipboard command
// manager to do that.

void Browser::Cut() {
  UserMetrics::RecordAction(L"Cut", profile_);
  ui_controls::SendKeyPress(L'X', true, false, false);
}

void Browser::Copy() {
  UserMetrics::RecordAction(L"Copy", profile_);
  ui_controls::SendKeyPress(L'C', true, false, false);
}

void Browser::CopyCurrentPageURL() {
  UserMetrics::RecordAction(L"CopyURLToClipBoard", profile_);
  std::string url = GetSelectedTabContents()->GetURL().spec();

  if (!::OpenClipboard(NULL)) {
    NOTREACHED();
    return;
  }

  if (::EmptyClipboard()) {
    HGLOBAL text = ::GlobalAlloc(GMEM_MOVEABLE, url.size() + 1);
    LPSTR ptr = static_cast<LPSTR>(::GlobalLock(text));
    memcpy(ptr, url.c_str(), url.size());
    ptr[url.size()] = '\0';
    ::GlobalUnlock(text);

    ::SetClipboardData(CF_TEXT, text);
  }

  if (!::CloseClipboard()) {
    NOTREACHED();
  }
}

void Browser::Paste() {
  UserMetrics::RecordAction(L"Paste", profile_);
  ui_controls::SendKeyPress(L'V', true, false, false);
}

void Browser::Find() {
  UserMetrics::RecordAction(L"Find", profile_);
  GetSelectedTabContents()->AsWebContents()->view()->FindInPage(*this, false,
                                                                false);
}

void Browser::FindNext() {
  UserMetrics::RecordAction(L"FindNext", profile_);
  AdvanceFindSelection(true);
}

void Browser::FindPrevious() {
  UserMetrics::RecordAction(L"FindPrevious", profile_);
  AdvanceFindSelection(false);
}

void Browser::ZoomIn() {
  UserMetrics::RecordAction(L"ZoomPlus", profile_);
  GetSelectedTabContents()->AsWebContents()->render_view_host()->Zoom(
      PageZoom::LARGER);
}

void Browser::ZoomReset() {
  UserMetrics::RecordAction(L"ZoomNormal", profile_);
  GetSelectedTabContents()->AsWebContents()->render_view_host()->Zoom(
      PageZoom::STANDARD);
}

void Browser::ZoomOut() {
  UserMetrics::RecordAction(L"ZoomMinus", profile_);
  GetSelectedTabContents()->AsWebContents()->render_view_host()->Zoom(
      PageZoom::SMALLER);
}

void Browser::FocusToolbar() {
  UserMetrics::RecordAction(L"FocusToolbar", profile_);
  window_->FocusToolbar();
}

void Browser::FocusLocationBar() {
  UserMetrics::RecordAction(L"FocusLocation", profile_);
  LocationBarView* lbv = GetLocationBarView();
  if (lbv) {
    AutocompleteEditView* aev = lbv->location_entry();
    aev->SetFocus();
    aev->SelectAll(true);
  }
}

void Browser::FocusSearch() {
  // TODO(beng): replace this with FocusLocationBar
  UserMetrics::RecordAction(L"FocusSearch", profile_);
  LocationBarView* lbv = GetLocationBarView();
  if (lbv) {
    AutocompleteEditView* aev = lbv->location_entry();
    aev->SetUserText(L"?");
    aev->SetFocus();
  }
}

void Browser::OpenFile() {
  UserMetrics::RecordAction(L"OpenFile", profile_);
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  // TODO(beng): figure out how to juggle this.
  HWND parent_hwnd = reinterpret_cast<HWND>(window_->GetNativeHandle());
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  std::wstring(), std::wstring(),
                                  std::wstring(), std::wstring(),
                                  parent_hwnd, NULL);
}

void Browser::OpenCreateShortcutsDialog() {
  UserMetrics::RecordAction(L"CreateShortcut", profile_);
  GetSelectedTabContents()->AsWebContents()->CreateShortcut();
}

void Browser::OpenDebuggerWindow() {
#ifndef CHROME_DEBUGGER_DISABLED
  UserMetrics::RecordAction(L"Debugger", profile_);
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab->AsWebContents()) {
    // Only one debugger instance can exist at a time right now.
    // TODO(erikkay): need an alert, dialog, something
    // or better yet, fix the one instance limitation
    if (!DebuggerWindow::DoesDebuggerExist()) {
      debugger_window_ = new DebuggerWindow();
    }
    debugger_window_->Show(current_tab);
  }
#endif
}

void Browser::OpenJavaScriptConsole() {
  UserMetrics::RecordAction(L"ShowJSConsole", profile_);
  GetSelectedTabContents()->AsWebContents()->render_view_host()->
      ShowJavaScriptConsole();
}

void Browser::OpenTaskManager() {
  UserMetrics::RecordAction(L"TaskManager", profile_);
  TaskManager::Open();
}

void Browser::OpenSelectProfileDialog() {
  UserMetrics::RecordAction(L"SelectProfile", profile_);
  SelectProfileDialog::RunDialog();
}

void Browser::OpenNewProfileDialog() {
  UserMetrics::RecordAction(L"CreateProfile", profile_);
  NewProfileDialog::RunDialog();
}

void Browser::OpenBugReportDialog() {
  UserMetrics::RecordAction(L"ReportBug", profile_);
  window_->ShowReportBugDialog();
}

void Browser::ToggleBookmarkBar() {
  UserMetrics::RecordAction(L"ShowBookmarksBar", profile_);
  window_->ToggleBookmarkBar();
}

void Browser::ShowHistoryTab() {
  UserMetrics::RecordAction(L"ShowHistory", profile_);
  ShowNativeUITab(HistoryTabUI::GetURL());
}

void Browser::OpenBookmarkManager() {
  UserMetrics::RecordAction(L"ShowBookmarkManager", profile_);
  window_->ShowBookmarkManager();
}

void Browser::ShowDownloadsTab() {
  UserMetrics::RecordAction(L"ShowDownloads", profile_);
  ShowNativeUITab(DownloadTabUI::GetURL());
}

void Browser::OpenClearBrowsingDataDialog() {
  UserMetrics::RecordAction(L"ClearBrowsingData_ShowDlg", profile_);
  window_->ShowClearBrowsingDataDialog();
}

void Browser::OpenImportSettingsDialog() {
  UserMetrics::RecordAction(L"Import_ShowDlg", profile_);
  window_->ShowImportDialog();
}

void Browser::OpenOptionsDialog() {
  UserMetrics::RecordAction(L"ShowOptions", profile_);
  ShowOptionsWindow(OPTIONS_PAGE_DEFAULT, OPTIONS_GROUP_NONE, profile_);
}

void Browser::OpenKeywordEditor() {
  UserMetrics::RecordAction(L"EditSearchEngines", profile_);
  window_->ShowSearchEnginesDialog();
}

void Browser::OpenPasswordManager() {
  window_->ShowPasswordManager();
}

void Browser::OpenAboutChromeDialog() {
  UserMetrics::RecordAction(L"AboutChrome", profile_);
  window_->ShowAboutChromeDialog();
}

void Browser::OpenHelpTab() {
  GURL help_url(l10n_util::GetString(IDS_HELP_CONTENT_URL));
  AddTabWithURL(help_url, GURL(), PageTransition::AUTO_BOOKMARK, true,
                NULL);
}

///////////////////////////////////////////////////////////////////////////////

// static
void Browser::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  prefs->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
}

// static
void Browser::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kHomePage, L"chrome-internal:");
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true);
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::CookiePolicy::ALLOW_ALL_COOKIES);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton, false);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding, L"");
  // TODO(peterson): bug #3870 move this to the AutofillManager once it is
  //                 checked-in.
  prefs->RegisterBooleanPref(prefs::kFormAutofillEnabled, true);
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCache, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies, true);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords, false);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData, true);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod, 0);
}

// static
Browser* Browser::GetBrowserForController(
    const NavigationController* controller, int* index_result) {
  BrowserList::const_iterator it;
  for (it = BrowserList::begin(); it != BrowserList::end(); ++it) {
    int index = (*it)->tabstrip_model_.GetIndexOfController(controller);
    if (index != TabStripModel::kNoTab) {
      if (index_result)
        *index_result = index;
      return *it;
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CommandHandler implementation:

void Browser::ExecuteCommand(int id) {
  DCHECK(IsCommandEnabled(id)) << "Invalid or disabled command";

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:                  GoBack();                      break;
    case IDC_FORWARD:               GoForward();                   break;
    case IDC_RELOAD:                Reload();                      break;
    case IDC_HOME:                  Home();                        break;
    case IDC_OPEN_CURRENT_URL:      OpenCurrentURL();              break;
    case IDC_GO:                    Go();                          break;
    case IDC_STOP:                  Stop();                        break;

     // Window management commands
    case IDC_NEW_WINDOW:            NewWindow();                   break;
    case IDC_NEW_INCOGNITO_WINDOW:  NewIncognitoWindow();          break;
    case IDC_NEW_WINDOW_PROFILE_0:
    case IDC_NEW_WINDOW_PROFILE_1:
    case IDC_NEW_WINDOW_PROFILE_2:
    case IDC_NEW_WINDOW_PROFILE_3:
    case IDC_NEW_WINDOW_PROFILE_4:
    case IDC_NEW_WINDOW_PROFILE_5:
    case IDC_NEW_WINDOW_PROFILE_6:
    case IDC_NEW_WINDOW_PROFILE_7:
    case IDC_NEW_WINDOW_PROFILE_8: 
        NewProfileWindowByIndex(id - IDC_NEW_WINDOW_PROFILE_0);    break;
    case IDC_CLOSE_WINDOW:          CloseWindow();                 break;
    case IDC_NEW_TAB:               NewTab();                      break;
    case IDC_CLOSE_TAB:             CloseTab();                    break;
    case IDC_SELECT_NEXT_TAB:       SelectNextTab();               break;
    case IDC_SELECT_PREVIOUS_TAB:   SelectPreviousTab();           break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:          SelectNumberedTab(id - IDC_SELECT_TAB_0);
                                                                   break;
    case IDC_SELECT_LAST_TAB:       SelectLastTab();               break;
    case IDC_DUPLICATE_TAB:         DuplicateTab();                break;
    case IDC_RESTORE_TAB:           RestoreTab();                  break;
    case IDC_SHOW_AS_TAB:           ConvertPopupToTabbedBrowser(); break;
    case IDC_EXIT:                  Exit();                        break;

    // Page-related commands
    case IDC_STAR:                  BookmarkCurrentPage();         break;
    case IDC_VIEW_SOURCE:           ViewSource();                  break;
    case IDC_CLOSE_POPUPS:          ClosePopups();                 break;
    case IDC_PRINT:                 Print();                       break;
    case IDC_SAVE_PAGE:             SavePage();                    break;
    case IDC_ENCODING_AUTO_DETECT:  ToggleEncodingAutoDetect();    break;
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GB2312:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5HKSCS:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:  OverrideEncoding(id);          break;

    // Clipboard commands
    case IDC_CUT:                   Cut();                         break;
    case IDC_COPY:                  Copy();                        break;
    case IDC_COPY_URL:              CopyCurrentPageURL();          break;
    case IDC_PASTE:                 Paste();                       break;

    // Find-in-page
    case IDC_FIND:                  Find();                        break;
    case IDC_FIND_NEXT:             FindNext();                    break;
    case IDC_FIND_PREVIOUS:         FindPrevious();                break;

    // Zoom
    case IDC_ZOOM_PLUS:             ZoomIn();                      break;
    case IDC_ZOOM_NORMAL:           ZoomReset();                   break;
    case IDC_ZOOM_MINUS:            ZoomOut();                     break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:         FocusToolbar();                break;
    case IDC_FOCUS_LOCATION:        FocusLocationBar();            break;
    case IDC_FOCUS_SEARCH:          FocusSearch();                 break;

    // Show various bits of UI
    case IDC_OPEN_FILE:             OpenFile();                    break;
    case IDC_CREATE_SHORTCUTS:      OpenCreateShortcutsDialog();   break;
    case IDC_DEBUGGER:              OpenDebuggerWindow();          break;
    case IDC_JS_CONSOLE:            OpenJavaScriptConsole();       break;
    case IDC_TASK_MANAGER:          OpenTaskManager();             break;
    case IDC_SELECT_PROFILE:        OpenSelectProfileDialog();     break;
    case IDC_NEW_PROFILE:           OpenNewProfileDialog();        break;
    case IDC_REPORT_BUG:            OpenBugReportDialog();         break;
    case IDC_SHOW_BOOKMARK_BAR:     ToggleBookmarkBar();           break;
    case IDC_SHOW_HISTORY:          ShowHistoryTab();              break;
    case IDC_SHOW_BOOKMARK_MANAGER: OpenBookmarkManager();         break;
    case IDC_SHOW_DOWNLOADS:        ShowDownloadsTab();            break;
    case IDC_CLEAR_BROWSING_DATA:   OpenClearBrowsingDataDialog(); break;
    case IDC_IMPORT_SETTINGS:       OpenImportSettingsDialog();    break;
    case IDC_OPTIONS:               OpenOptionsDialog();           break;
    case IDC_EDIT_SEARCH_ENGINES:   OpenKeywordEditor();           break;
    case IDC_VIEW_PASSWORDS:        OpenPasswordManager();         break;
    case IDC_ABOUT:                 OpenAboutChromeDialog();       break;
    case IDC_HELP_PAGE:             OpenHelpTab();                 break;

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelDelegate implementation:

GURL Browser::GetBlankTabURL() const {
  return NewTabUIURL();
}

void Browser::CreateNewStripWithContents(TabContents* detached_contents,
                                         const gfx::Rect& window_bounds,
                                         const DockInfo& dock_info) {
  DCHECK(type_ == TYPE_NORMAL);

  gfx::Rect new_window_bounds = window_bounds;
  bool maximize = false;
  if (dock_info.GetNewWindowBounds(&new_window_bounds, &maximize))
    dock_info.AdjustOtherWindowBounds();

  // Create an empty new browser window the same size as the old one.
  Browser* browser = new Browser(TYPE_NORMAL, profile_);
  browser->set_override_bounds(new_window_bounds);
  browser->set_override_maximized(maximize);
  browser->CreateBrowserWindow();
  browser->tabstrip_model()->AppendTabContents(detached_contents, true);
  browser->window()->Show();

  // When we detach a tab we need to make sure any associated Find window moves
  // along with it to its new home (basically we just make new_window the
  // parent of the Find window).
  // TODO(brettw) this could probably be improved, see
  // WebContentsView::ReparentFindWindow for more.
  if (detached_contents->AsWebContents())
    detached_contents->AsWebContents()->view()->ReparentFindWindow(browser);
}

int Browser::GetDragActions() const {
  int result = 0;
  if (BrowserList::GetBrowserCountForType(profile_, TYPE_NORMAL) > 1 ||
      tab_count() > 1)
    result |= TAB_TEAROFF_ACTION;
  if (tab_count() > 1)
    result |= TAB_MOVE_ACTION;
  return result;
}

TabContents* Browser::CreateTabContentsForURL(
    const GURL& url, const GURL& referrer, Profile* profile,
    PageTransition::Type transition, bool defer_load,
    SiteInstance* instance) const {
  // Create an appropriate tab contents.
  GURL real_url = url;
  TabContentsType type = TabContents::TypeForURL(&real_url);
  DCHECK(type != TAB_CONTENTS_UNKNOWN_TYPE);

  TabContents* contents = TabContents::CreateWithType(type, profile, instance);
  contents->SetupController(profile);

  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->controller()->LoadURL(url, referrer, transition);
  }

  return contents;
}

bool Browser::CanDuplicateContentsAt(int index) {
  TabContents* contents = GetTabContentsAt(index);
  DCHECK(contents);

  NavigationController* nc = contents->controller();
  return nc ? (nc->active_contents() && nc->GetLastCommittedEntry()) : false;
}

void Browser::DuplicateContentsAt(int index) {
  TabContents* contents = GetTabContentsAt(index);
  TabContents* new_contents = NULL;
  DCHECK(contents);

  if (type_ == TYPE_NORMAL) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    new_contents = contents->controller()->Clone()->active_contents();
    // If you duplicate a tab that is not selected, we need to make sure to
    // select the tab being duplicated so that DetermineInsertionIndex returns
    // the right index (if tab 5 is selected and we right-click tab 1 we want
    // the new tab to appear in index position 2, not 6).
    if (tabstrip_model_.selected_index() != index)
      tabstrip_model_.SelectTabContentsAt(index, true);
    tabstrip_model_.AddTabContents(new_contents, index + 1,
                                   PageTransition::LINK, true);
  } else {
    Browser* browser = NULL;
    if (type_ == TYPE_APP) {
      browser = Browser::CreateForApp(app_name_, profile_);
    } else if (type_ == TYPE_POPUP) {
      browser = Browser::CreateForPopup(profile_);
    }

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // TabContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    new_contents = browser->AddTabWithNavigationController(
        contents->controller()->Clone(),
        PageTransition::LINK);
  }

  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(new_contents->controller());
  }
}

void Browser::CloseFrameAfterDragSession() {
  // This is scheduled to run after we return to the message loop because
  // otherwise the frame will think the drag session is still active and ignore
  // the request.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&Browser::CloseFrame));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::TabInsertedAt(TabContents* contents,
                            int index,
                            bool foreground) {
  contents->set_delegate(this);
  contents->controller()->SetWindowID(session_id());

  SyncHistoryWithTabs(tabstrip_model_.GetIndexOfTabContents(contents));

  // When a tab is dropped into a tab strip we need to make sure that the
  // associated Find window is moved along with it. We therefore change the
  // parent of the Find window (if the parent is already correctly set this
  // does nothing).
  // TODO(brettw) this could probably be improved, see
  // WebContentsView::ReparentFindWindow for more.
  if (contents->AsWebContents())
    contents->AsWebContents()->view()->ReparentFindWindow(this);

  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  NotificationService::current()->
      AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                  Source<TabContents>(contents));
}

void Browser::TabClosingAt(TabContents* contents, int index) {
  NavigationController* controller = contents->controller();
  DCHECK(controller);
  NotificationService::current()->
      Notify(NOTIFY_TAB_CLOSING,
              Source<NavigationController>(controller),
              NotificationService::NoDetails());

  // Sever the TabContents' connection back to us.
  contents->set_delegate(NULL);
}

void Browser::TabDetachedAt(TabContents* contents, int index) {
  contents->set_delegate(NULL);
  if (!tabstrip_model_.closing_all())
    SyncHistoryWithTabs(0);

  RemoveScheduledUpdatesFor(contents);

  NotificationService::current()->
      RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                     Source<TabContents>(contents));
}

void Browser::TabSelectedAt(TabContents* old_contents,
                            TabContents* new_contents,
                            int index,
                            bool user_gesture) {
  DCHECK(old_contents != new_contents);

  // If we have any update pending, do it now.
  if (!chrome_updater_factory_.empty() && old_contents)
    ProcessPendingUIUpdates();

  LocationBarView* location_bar = GetLocationBarView();
  if (old_contents) {
    // Save what the user's currently typing, so it can be restored when we
    // switch back to this tab.
    if (location_bar)
      location_bar->location_entry()->SaveStateToTab(old_contents);
  }

  // Propagate the profile to the location bar.
  UpdateToolbar(true);

  // Force the go/stop button to change.
  GetGoButton()->ChangeMode(
      (new_contents->AsWebContents() && new_contents->is_loading()) ?
      GoButton::MODE_STOP : GoButton::MODE_GO);

  // Update commands to reflect current state.
  UpdateCommandsForTabState();

  // Reset the status bubble.
  GetStatusBubble()->Hide();

  // Show the loading state (if any).
  GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service && !tabstrip_model_.closing_all()) {
      session_service->SetSelectedTabInWindow(
          session_id(), tabstrip_model_.selected_index());
    }
  }
}

void Browser::TabMoved(TabContents* contents,
                       int from_index,
                       int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::TabStripEmpty() {
  // Close the frame after we return to the message loop (not immediately,
  // otherwise it will destroy this object before the stack has a chance to
  // cleanly unwind.)
  // Note: This will be called several times if TabStripEmpty is called several
  //       times. This is because it does not close the window if tabs are
  //       still present.
  // NOTE: If you change to be immediate (no invokeLater) then you'll need to
  //       update BrowserList::CloseAllBrowsers.
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&Browser::CloseFrame));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabContentsDelegate implementation:

void Browser::OpenURLFromTab(TabContents* source,
                             const GURL& url, const GURL& referrer,
                             WindowOpenDisposition disposition,
                             PageTransition::Type transition) {
  // No code for these yet
  DCHECK((disposition != NEW_POPUP) && (disposition != SAVE_TO_DISK));

  TabContents* current_tab = source ? source : GetSelectedTabContents();
  bool source_tab_was_frontmost = (current_tab == GetSelectedTabContents());
  TabContents* new_contents = NULL;

  // If the URL is part of the same web site, then load it in the same
  // SiteInstance (and thus the same process).  This is an optimization to
  // reduce process overhead; it is not necessary for compatibility.  (That is,
  // the new tab will not have script connections to the previous tab, so it
  // does not need to be part of the same SiteInstance or BrowsingInstance.)
  // Default to loading in a new SiteInstance and BrowsingInstance.
  // TODO(creis): should this apply to applications?
  SiteInstance* instance = NULL;
  // Don't use this logic when "--process-per-tab" is specified.
  if (!CommandLine().HasSwitch(switches::kProcessPerTab)) {
    if (current_tab) {
      const WebContents* const web_contents = current_tab->AsWebContents();
      if (web_contents) {
        const GURL& current_url = web_contents->GetURL();
        if (SiteInstance::IsSameWebSite(current_url, url))
          instance = web_contents->GetSiteInstance();
      }
    }
  }

  // If this is an application we can only have one tab so a new tab always
  // goes into a tabbed browser window.
  if (disposition != NEW_WINDOW && type_ == TYPE_APP) {
    // If the disposition is OFF_THE_RECORD we don't want to create a new
    // browser that will itself create another OTR browser. This will result in
    // a browser leak (and crash below because no tab is created or selected).
    if (disposition == OFF_THE_RECORD) {
      OpenURLOffTheRecord(profile_, url);
      return;
    }

    Browser* b = GetOrCreateTabbedBrowser();
    DCHECK(b);

    // If we have just created a new browser window, make sure we select the
    // tab.
    if (b->tab_count() == 0 && disposition == NEW_BACKGROUND_TAB)
      disposition = NEW_FOREGROUND_TAB;

    b->OpenURL(url, referrer, disposition, transition);
    b->window()->Show();
    return;
  }

  if (profile_->IsOffTheRecord() && disposition == OFF_THE_RECORD)
    disposition = NEW_FOREGROUND_TAB;

  if (disposition == NEW_WINDOW) {
    Browser* browser = Browser::Create(profile_);
    new_contents = browser->AddTabWithURL(url, referrer, transition, true,
                                          instance);
    browser->window()->Show();
  } else if ((disposition == CURRENT_TAB) && current_tab) {
    if (transition == PageTransition::TYPED ||
        transition == PageTransition::AUTO_BOOKMARK ||
        transition == PageTransition::GENERATED ||
        transition == PageTransition::START_PAGE) {
      // Don't forget the openers if this tab is a New Tab page opened at the
      // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
      // navigation of one of these transition types before resetting the
      // opener relationships (this allows for the use case of opening a new
      // tab to do a quick look-up of something while viewing a tab earlier in
      // the strip). We can make this heuristic more permissive if need be.
      // TODO(beng): (http://b/1306495) write unit tests for this once this
      //             object is unit-testable.
      int current_tab_index =
          tabstrip_model_.GetIndexOfTabContents(current_tab);
      bool forget_openers =
          !(current_tab->type() == TAB_CONTENTS_NEW_TAB_UI &&
          current_tab_index == (tab_count() - 1) &&
          current_tab->controller()->GetEntryCount() == 1);
      if (forget_openers) {
        // If the user navigates the current tab to another page in any way
        // other than by clicking a link, we want to pro-actively forget all
        // TabStrip opener relationships since we assume they're beginning a
        // different task by reusing the current tab.
        tabstrip_model_.ForgetAllOpeners();
        // In this specific case we also want to reset the group relationship,
        // since it is now technically invalid.
        tabstrip_model_.ForgetGroup(current_tab);
      }
    }
    current_tab->controller()->LoadURL(url, referrer, transition);
    // The TabContents might have changed as part of the navigation (ex: new
    // tab page can become WebContents).
    new_contents = current_tab->controller()->active_contents();
    GetStatusBubble()->Hide();

    // Synchronously update the location bar. This allows us to immediately
    // have the URL bar update when the user types something, rather than
    // going through the normal system of ScheduleUIUpdate which has a delay.
    UpdateToolbar(false);
  } else if (disposition == OFF_THE_RECORD) {
    OpenURLOffTheRecord(profile_, url);
    return;
  } else if (disposition != SUPPRESS_OPEN) {
    new_contents = AddTabWithURL(url, referrer, transition,
                                 disposition != NEW_BACKGROUND_TAB, instance);
  }

  if (disposition != NEW_BACKGROUND_TAB && source_tab_was_frontmost) {
    // Give the focus to the newly navigated tab, if the source tab was
    // front-most.
    new_contents->Focus();
  }
}

void Browser::NavigationStateChanged(const TabContents* source,
                                     unsigned changed_flags) {
  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We don't schedule updates to commands since they will only change once per
  // navigation, so we don't have to worry about flickering.
  if (changed_flags & TabContents::INVALIDATE_URL)
    UpdateCommandsForTabState();
}

void Browser::ReplaceContents(TabContents* source, TabContents* new_contents) {
  source->set_delegate(NULL);
  new_contents->set_delegate(this);

  RemoveScheduledUpdatesFor(source);

  int index = tabstrip_model_.GetIndexOfTabContents(source);
  tabstrip_model_.ReplaceTabContentsAt(index, new_contents);

  if (is_attempting_to_close_browser_) {
    // Need to do this asynchronously as it will close the tab, which is
    // currently on the call stack above us.
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&Browser::ClearUnloadState,
        Source<TabContents>(source).ptr()));
  }
  // Need to remove ourselves as an observer for disconnection on the replaced
  // TabContents, since we only care to fire onbeforeunload handlers on active
  // Tabs. Make sure an observer is added for the replacement TabContents.
  NotificationService::current()->
      RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                     Source<TabContents>(source));
  NotificationService::current()->
      AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                  Source<TabContents>(new_contents));
}

void Browser::AddNewContents(TabContents* source,
                             TabContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  DCHECK(disposition != SAVE_TO_DISK);  // No code for this yet

  // If this is an application we can only have one tab so we need to process
  // this in tabbed browser window.
  if (tabstrip_model_.count() > 0 &&
      disposition != NEW_WINDOW && disposition != NEW_POPUP &&
      type_ != TYPE_NORMAL) {
    Browser* b = GetOrCreateTabbedBrowser();
    DCHECK(b);
    PageTransition::Type transition = PageTransition::LINK;
    // If we were called from an "installed webapp" we want to emulate the code
    // that is run from browser_init.cc for links from external applications.
    // This means we need to open the tab with the START PAGE transition.
    // AddNewContents doesn't support this but the TabStripModel's
    // AddTabContents method does.
    if (type_ == TYPE_APP)
      transition = PageTransition::START_PAGE;
    b->tabstrip_model()->AddTabContents(new_contents, -1, transition, true);
    b->window()->Show();
    return;
  }

  if (disposition == NEW_POPUP) {
    BuildPopupWindow(source, new_contents, initial_pos);
  } else if (disposition == NEW_WINDOW) {
    Browser* browser = Browser::Create(profile_);
    browser->AddNewContents(source, new_contents, NEW_FOREGROUND_TAB,
                            initial_pos, user_gesture);
    browser->window()->Show();
  } else if (disposition == CURRENT_TAB) {
    ReplaceContents(source, new_contents);
  } else if (disposition != SUPPRESS_OPEN) {
    tabstrip_model_.AddTabContents(new_contents, -1, PageTransition::LINK,
                                   disposition == NEW_FOREGROUND_TAB);
  }
}

void Browser::ActivateContents(TabContents* contents) {
  tabstrip_model_.SelectTabContentsAt(
      tabstrip_model_.GetIndexOfTabContents(contents), false);
  window_->Activate();
}

void Browser::LoadingStateChanged(TabContents* source) {
  window_->UpdateLoadingAnimations(tabstrip_model_.TabsAreLoading());
  window_->UpdateTitleBar();

  // Let the go button know that it should change appearance if possible.
  if (source == GetSelectedTabContents()) {
    GetGoButton()->ScheduleChangeMode(
      source->is_loading() ? GoButton::MODE_STOP : GoButton::MODE_GO);

    GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());
  }
}

void Browser::CloseContents(TabContents* source) {
  if (is_attempting_to_close_browser_) {
    // If we're trying to close the browser, just clear the state related to
    // waiting for unload to fire. Don't actually try to close the tab as it
    // will go down the slow shutdown path instead of the fast path of killing
    // all the renderer processes.
    ClearUnloadState(source);
    return;
  }

  int index = tabstrip_model_.GetIndexOfTabContents(source);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED() << "CloseContents called for tab not in our strip";
    return;
  }
  tabstrip_model_.CloseTabContentsAt(index);
}

void Browser::MoveContents(TabContents* source, const gfx::Rect& pos) {
  if (type() != TYPE_POPUP) {
    NOTREACHED() << "moving invalid browser type";
    return;
  }
  window_->SetBounds(pos);
}

bool Browser::IsPopup(TabContents* source) {
  // A non-tabbed BROWSER is an unconstrained popup.
  return (type() == TYPE_POPUP);
}

void Browser::ToolbarSizeChanged(TabContents* source, bool is_animating) {
  if (source == GetSelectedTabContents() || source == NULL) {
    // This will refresh the shelf if needed.
    window_->SelectedTabToolbarSizeChanged(is_animating);
  }
}

void Browser::URLStarredChanged(TabContents* source, bool starred) {
  if (source == GetSelectedTabContents())
    SetStarredButtonToggled(starred);
}

void Browser::ContentsMouseEvent(TabContents* source, UINT message) {
  if (source == GetSelectedTabContents()) {
    if (message == WM_MOUSEMOVE) {
      GetStatusBubble()->MouseMoved();
    } else if (message == WM_MOUSELEAVE) {
      GetStatusBubble()->SetURL(GURL(), std::wstring());
    }
  }
}

void Browser::UpdateTargetURL(TabContents* source, const GURL& url) {
  if (source == GetSelectedTabContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::ContentsZoomChange(bool zoom_in) {
  controller_.ExecuteCommand(zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

bool Browser::IsApplication() const {
  return type_ == TYPE_APP;
}

void Browser::ConvertContentsToApplication(TabContents* contents) {
  if (!contents->AsWebContents() || !contents->AsWebContents()->web_app()) {
    NOTREACHED();
    return;
  }

  int index = tabstrip_model_.GetIndexOfTabContents(contents);
  if (index < 0)
    return;

  WebApp* app = contents->AsWebContents()->web_app();
  const std::wstring& app_name =
      app->name().empty() ? ComputeApplicationNameFromURL(app->url()) :
                            app->name();
  RegisterAppPrefs(app_name);

  tabstrip_model_.DetachTabContentsAt(index);
  Browser* browser = Browser::CreateForApp(app_name, profile_);
  browser->tabstrip_model()->AppendTabContents(contents, true);
  browser->window()->Show();
}

void Browser::ContentsStateChanged(TabContents* source) {
  int index = tabstrip_model_.GetIndexOfTabContents(source);
  if (index != TabStripModel::kNoTab)
    tabstrip_model_.UpdateTabContentsStateAt(index);
}

bool Browser::ShouldDisplayURLField() {
  return !IsApplication();
}

void Browser::BeforeUnloadFired(TabContents* tab,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (!is_attempting_to_close_browser_) {
    *proceed_to_fire_unload = proceed;
    return;
  }

  if (!proceed) {
    CancelWindowClose();
    *proceed_to_fire_unload = false;
    return;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, tab)) {
    // Now that beforeunload has fired, put the tab on the queue to fire
    // unload.
    tabs_needing_unload_fired_.insert(tab);
    ProcessPendingTabs();
    // We want to handle firing the unload event ourselves since we want to
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    *proceed_to_fire_unload = false;
    return;
  }

  *proceed_to_fire_unload = true;
}

void Browser::ShowHtmlDialog(HtmlDialogContentsDelegate* delegate,
                             void* parent_window) {
  window_->ShowHTMLDialog(delegate, parent_window);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SelectFileDialog::Listener implementation:

void Browser::FileSelected(const std::wstring& path, void* params) {
  GURL file_url = net::FilePathToFileURL(path);
  if (!file_url.is_empty())
    OpenURL(file_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, NotificationObserver implementation:

void Browser::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_WEB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        // Need to do this asynchronously as it will close the tab, which is
        // currently on the call stack above us.
        MessageLoop::current()->PostTask(FROM_HERE,
            method_factory_.NewRunnableMethod(&Browser::ClearUnloadState,
                Source<TabContents>(source).ptr()));
      }
      break;

    case NOTIFY_SSL_STATE_CHANGED:
      // When the current tab's SSL state changes, we need to update the URL
      // bar to reflect the new state. Note that it's possible for the selected
      // tab contents to be NULL. This is because we listen for all sources
      // (NavigationControllers) for convenience, so the notification could
      // actually be for a different window while we're doing asynchronous
      // closing of this one.
      if (GetSelectedTabContents() &&
          GetSelectedTabContents()->controller() ==
          Source<NavigationController>(source).ptr())
        UpdateToolbar(false);
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  // Navigation commands
  controller_.UpdateCommandEnabled(IDC_RELOAD, true);
  controller_.UpdateCommandEnabled(IDC_HOME, type() == TYPE_NORMAL);
  controller_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, true);
  controller_.UpdateCommandEnabled(IDC_GO, true);

  // Window management commands
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  controller_.UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  controller_.UpdateCommandEnabled(IDC_PROFILE_MENU, true);
  // TODO(pkasting): Perhaps the code that populates this submenu should do
  // this?
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_0, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_1, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_2, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_3, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_4, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_5, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_6, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_7, true);
  controller_.UpdateCommandEnabled(IDC_NEW_WINDOW_PROFILE_8, true);
  controller_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  controller_.UpdateCommandEnabled(IDC_NEW_TAB, true);
  controller_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_0, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_1, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_2, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_3, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_4, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_5, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_6, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_TAB_7, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, true);
  controller_.UpdateCommandEnabled(IDC_DUPLICATE_TAB, true);
  controller_.UpdateCommandEnabled(IDC_RESTORE_TAB,
      (!profile_->IsOffTheRecord() && type() == TYPE_NORMAL));
  controller_.UpdateCommandEnabled(IDC_SHOW_AS_TAB, type() == TYPE_POPUP);
  controller_.UpdateCommandEnabled(IDC_EXIT, true);

  // Page-related commands
  controller_.UpdateCommandEnabled(IDC_CLOSE_POPUPS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88591, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_GB2312, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_BIG5HKSCS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_BIG5, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_THAI, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOREAN, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_SHIFTJIS, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO2022JP, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_EUCJP, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885915, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_MACINTOSH, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88592, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1250, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88595, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1251, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOI8R, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_KOI8U, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88597, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1253, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88594, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885913, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1257, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88593, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885910, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885914, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO885916, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1254, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88596, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1256, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_ISO88598, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1255, true);
  controller_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1258, true);

  // Clipboard commands
  controller_.UpdateCommandEnabled(IDC_CUT, true);
  controller_.UpdateCommandEnabled(IDC_COPY, true);
  controller_.UpdateCommandEnabled(IDC_COPY_URL, true);
  controller_.UpdateCommandEnabled(IDC_PASTE, true);

  // Focus various bits of UI
  controller_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, true);
  controller_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, true);
  controller_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, true);

  // Show various bits of UI
  controller_.UpdateCommandEnabled(IDC_OPEN_FILE, true);
  controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  controller_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, true);
  // The debugger doesn't work in single process mode.
  controller_.UpdateCommandEnabled(IDC_DEBUGGER,
      !RenderProcessHost::run_renderer_in_process());
  controller_.UpdateCommandEnabled(IDC_TASK_MANAGER, true);
  controller_.UpdateCommandEnabled(IDC_SELECT_PROFILE, true);
  controller_.UpdateCommandEnabled(IDC_NEW_PROFILE, true);
  controller_.UpdateCommandEnabled(IDC_REPORT_BUG, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_BAR, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER, true);
  controller_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  controller_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  controller_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);
  controller_.UpdateCommandEnabled(IDC_OPTIONS, true);
  controller_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, true);
  controller_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, true);
  controller_.UpdateCommandEnabled(IDC_ABOUT, true);
  controller_.UpdateCommandEnabled(IDC_HELP_PAGE, true);
}

void Browser::UpdateCommandsForTabState() {
  TabContents* current_tab = GetSelectedTabContents();
  if (!current_tab) {
    // It's possible for this to be null during tab restore.
    return;
  }

  // Navigation commands
  NavigationController* nc = current_tab->controller();
  controller_.UpdateCommandEnabled(IDC_BACK, nc->CanGoBack());
  controller_.UpdateCommandEnabled(IDC_FORWARD, nc->CanGoForward());

  // Window management commands
  controller_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
                                   CanDuplicateContentsAt(selected_index()));

  // Show various bits of UI
  controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS,
      current_tab->type() == TAB_CONTENTS_WEB &&
      !current_tab->GetFavIcon().isNull());

  WebContents* web_contents = current_tab->AsWebContents();
  if (web_contents) {
    // Page-related commands
    // Only allow bookmarking for tabbed browsers.
    controller_.UpdateCommandEnabled(IDC_STAR, type() == TYPE_NORMAL);
    SetStarredButtonToggled(web_contents->is_starred());
    // View-source should not be enabled if already in view-source mode.
    controller_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
        current_tab->type() != TAB_CONTENTS_VIEW_SOURCE &&
        current_tab->controller()->GetActiveEntry());
    controller_.UpdateCommandEnabled(IDC_PRINT, true);
    controller_.UpdateCommandEnabled(IDC_SAVE_PAGE,
        SavePackage::IsSavableURL(current_tab->GetURL()));
    controller_.UpdateCommandEnabled(IDC_ENCODING_MENU,
        SavePackage::IsSavableContents(web_contents->contents_mime_type()) &&
        SavePackage::IsSavableURL(current_tab->GetURL()));

    // Find-in-page
    controller_.UpdateCommandEnabled(IDC_FIND, true);
    controller_.UpdateCommandEnabled(IDC_FIND_NEXT, true);
    controller_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, true);

    // Zoom
    controller_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
    controller_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
    controller_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, true);
    controller_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

    // Show various bits of UI
    controller_.UpdateCommandEnabled(IDC_JS_CONSOLE, true);
  } else {
    // Page-related commands
    // Both disable the starring button and ensure it doesn't show a star.
    controller_.UpdateCommandEnabled(IDC_STAR, false);
    SetStarredButtonToggled(false);
    controller_.UpdateCommandEnabled(IDC_VIEW_SOURCE, false);
    controller_.UpdateCommandEnabled(IDC_PRINT, false);
    controller_.UpdateCommandEnabled(IDC_SAVE_PAGE, false);
    controller_.UpdateCommandEnabled(IDC_ENCODING_MENU, false);

    // Find-in-page
    controller_.UpdateCommandEnabled(IDC_FIND, false);
    controller_.UpdateCommandEnabled(IDC_FIND_NEXT, false);
    controller_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, false);

    // Zoom
    controller_.UpdateCommandEnabled(IDC_ZOOM_MENU, false);
    controller_.UpdateCommandEnabled(IDC_ZOOM_PLUS, false);
    controller_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, false);
    controller_.UpdateCommandEnabled(IDC_ZOOM_MINUS, false);

    // Show various bits of UI
    controller_.UpdateCommandEnabled(IDC_JS_CONSOLE, false);
  }
}

void Browser::SetStarredButtonToggled(bool starred) {
  window_->GetStarButton()->SetToggled(starred);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(GetSelectedTabContents(), should_restore_state);
}

void Browser::ScheduleUIUpdate(const TabContents* source,
                               unsigned changed_flags) {
  // Synchronously update the URL.
  if (changed_flags & TabContents::INVALIDATE_URL &&
      source == GetSelectedTabContents()) {
    // Only update the URL for the current tab. Note that we do not update
    // the navigation commands since those would have already been updated
    // synchronously by NavigationStateChanged.
    UpdateToolbar(false);

    if (changed_flags == TabContents::INVALIDATE_URL)
      return;  // Just had an update URL and nothing else.
  }

  // Save the dirty bits.
  scheduled_updates_.push_back(UIUpdate(source, changed_flags));

  if (chrome_updater_factory_.empty()) {
    // No task currently scheduled, start another.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        chrome_updater_factory_.NewRunnableMethod(
            &Browser::ProcessPendingUIUpdates),
            kUIUpdateCoalescingTimeMS);
  }
}

void Browser::ProcessPendingUIUpdates() {
#ifndef NDEBUG
  // Validate that all tabs we have pending updates for exist. This is scary
  // because the pending list must be kept in sync with any detached or
  // deleted tabs. This code does not dereference any TabContents pointers.
  for (size_t i = 0; i < scheduled_updates_.size(); i++) {
    bool found = false;
    for (int tab = 0; tab < tab_count(); tab++) {
      if (GetTabContentsAt(tab)->controller() ==
          scheduled_updates_[i].source->controller()) {
        found = true;
        break;
      }
    }
    DCHECK(found);
  }
#endif

  chrome_updater_factory_.RevokeAll();

  // We could have many updates for the same thing in the queue. This map
  // tracks the bits of the stuff we've already updated for each TabContents so
  // we don't update again.
  typedef std::map<const TabContents*, unsigned> UpdateTracker;
  UpdateTracker updated_stuff;

  for (size_t i = 0; i < scheduled_updates_.size(); i++) {
    // Do not dereference |contents|, it may be out-of-date!
    const TabContents* contents = scheduled_updates_[i].source;
    unsigned flags = scheduled_updates_[i].changed_flags;

    // Remove any bits we have already updated, and save the new bits.
    UpdateTracker::iterator updated = updated_stuff.find(contents);
    if (updated != updated_stuff.end()) {
      // Turn off bits already set.
      flags &= ~updated->second;
      if (!flags)
        continue;

      updated->second |= flags;
    } else {
      updated_stuff[contents] = flags;
    }

    // Updates to the title or favicon require a tab repaint. However, the
    // inverse is not true since updates to the title also update the window
    // title.
    bool invalidate_tab = false;
    if (flags & TabContents::INVALIDATE_TITLE ||
        flags & TabContents::INVALIDATE_FAVICON) {
      invalidate_tab = true;

      // Anything that repaints the tab means the favicon is updated.
      updated_stuff[contents] |= TabContents::INVALIDATE_FAVICON;
    }

    // Updating the URL happens synchronously in ScheduleUIUpdate.

    if (flags & TabContents::INVALIDATE_LOAD)
      GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());

    if (invalidate_tab) {  // INVALIDATE_TITLE or INVALIDATE_FAVICON.
      tabstrip_model_.UpdateTabContentsStateAt(
          tabstrip_model_.GetIndexOfController(contents->controller()));
      window_->UpdateTitleBar();

      if (contents == GetSelectedTabContents()) {
        TabContents* current_tab = GetSelectedTabContents();
        controller_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS,
            current_tab->type() == TAB_CONTENTS_WEB &&
            !current_tab->GetFavIcon().isNull());
      }
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

void Browser::RemoveScheduledUpdatesFor(TabContents* contents) {
  if (!contents)
    return;

  // Remove any pending UI updates for the detached tab.
  UpdateVector::iterator cur_update = scheduled_updates_.begin();
  while (cur_update != scheduled_updates_.end()) {
    if (cur_update->source == contents) {
      cur_update = scheduled_updates_.erase(cur_update);
    } else {
      ++cur_update;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Getters for UI (private):

LocationBarView* Browser::GetLocationBarView() const {
  return window_->GetLocationBarView();
}

GoButton* Browser::GetGoButton() {
  return window_->GetGoButton();
}

StatusBubble* Browser::GetStatusBubble() {
  return window_->GetStatusBubble();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Session restore functions (private):

void Browser::SyncHistoryWithTabs(int index) {
  if (!profile()->HasSessionService())
    return;
  SessionService* session_service = profile()->GetSessionService();
  if (session_service) {
    for (int i = index; i < tab_count(); ++i) {
      TabContents* contents = GetTabContentsAt(i);
      if (contents) {
        session_service->SetTabIndexInWindow(
            session_id(), contents->controller()->session_id(), i);
      }
    }
  }
}

NavigationController* Browser::BuildRestoredNavigationController(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation) {
  if (!navigations.empty()) {
    DCHECK(selected_navigation >= 0 &&
           selected_navigation < static_cast<int>(navigations.size()));
    // Create a NavigationController. This constructor creates the appropriate
    // set of TabContents.
    return new NavigationController(profile_, navigations, selected_navigation);
  } else {
    // No navigations. Create a tab with about:blank.
    TabContents* contents =
        CreateTabContentsForURL(GURL("about:blank"), GURL(), profile_,
                                PageTransition::START_PAGE, false, NULL);
    return new NavigationController(contents, profile_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling (private):

void Browser::ProcessPendingTabs() {
  DCHECK(is_attempting_to_close_browser_);

  if (HasCompletedUnloadProcessing()) {
    // We've finished all the unload events and can proceed to close the
    // browser.
    OnWindowClosing();
    return;
  }

  // Process beforeunload tabs first. When that queue is empty, process
  // unload tabs.
  if (!tabs_needing_before_unload_fired_.empty()) {
    TabContents* tab = *(tabs_needing_before_unload_fired_.begin());
    tab->AsWebContents()->render_view_host()->FirePageBeforeUnload();
  } else if (!tabs_needing_unload_fired_.empty()) {
    // We've finished firing all beforeunload events and can proceed with unload
    // events.
    // TODO(ojan): We should add a call to browser_shutdown::OnShutdownStarting
    // somewhere around here so that we have accurate measurements of shutdown
    // time.
    // TODO(ojan): We can probably fire all the unload events in parallel and
    // get a perf benefit from that in the cases where the tab hangs in it's
    // unload handler or takes a long time to page in.
    TabContents* tab = *(tabs_needing_unload_fired_.begin());
    tab->AsWebContents()->render_view_host()->FirePageUnload();
  } else {
    NOTREACHED();
  }
}

bool Browser::HasCompletedUnloadProcessing() {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_fired_.empty() &&
      tabs_needing_unload_fired_.empty();
}

void Browser::CancelWindowClose() {
  DCHECK(is_attempting_to_close_browser_);
  // Only cancelling beforeunload should be able to cancel the window's close.
  // So there had better be a tab that we think needs beforeunload fired.
  DCHECK(!tabs_needing_before_unload_fired_.empty());

  tabs_needing_before_unload_fired_.clear();
  tabs_needing_unload_fired_.clear();

  is_attempting_to_close_browser_ = false;
}

bool Browser::RemoveFromSet(UnloadListenerSet* set, TabContents* tab) {
  DCHECK(is_attempting_to_close_browser_);

  UnloadListenerSet::iterator iter = std::find(set->begin(), set->end(), tab);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void Browser::ClearUnloadState(TabContents* tab) {
  DCHECK(is_attempting_to_close_browser_);
  RemoveFromSet(&tabs_needing_before_unload_fired_, tab);
  RemoveFromSet(&tabs_needing_unload_fired_, tab);
  ProcessPendingTabs();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted utility functions (private):

Browser* Browser::GetOrCreateTabbedBrowser() {
  Browser* browser = BrowserList::FindBrowserWithType(
      profile_, TYPE_NORMAL);
  if (!browser)
    browser = Browser::Create(profile_);
  return browser;
}

void Browser::BuildPopupWindow(TabContents* source,
                               TabContents* new_contents,
                               const gfx::Rect& initial_pos) {
  Type type = type_ == TYPE_APP ? type_ : TYPE_POPUP;
  Browser* browser = new Browser(type, profile_);
  browser->set_override_bounds(initial_pos);
  browser->CreateBrowserWindow();
  // TODO(beng): See if this can be made to use
  //             TabStripModel::AppendTabContents.
  browser->AddNewContents(source, new_contents, NEW_FOREGROUND_TAB,
                          gfx::Rect(), true);
  browser->window()->Show();
}

GURL Browser::GetHomePage() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return NewTabUIURL();
  GURL home_page = GURL(URLFixerUpper::FixupURL(
      profile_->GetPrefs()->GetString(prefs::kHomePage),
      std::wstring()));
  if (!home_page.is_valid())
    return NewTabUIURL();
  return home_page;
}

void Browser::AdvanceFindSelection(bool forward_direction) {
  GetSelectedTabContents()->AsWebContents()->view()->FindInPage(
      *this, true, forward_direction);
}

void Browser::CloseFrame() {
  window_->Close();
}

// static
std::wstring Browser::ComputeApplicationNameFromURL(const GURL& url) {
  std::string t;
  t.append(url.host());
  t.append("_");
  t.append(url.path());
  return UTF8ToWide(t);
}

// static
void Browser::RegisterAppPrefs(const std::wstring& app_name) {
  // A set of apps that we've already started.
  static std::set<std::wstring>* g_app_names = NULL;

  if (!g_app_names)
    g_app_names = new std::set<std::wstring>;

  // Only register once for each app name.
  if (g_app_names->find(app_name) != g_app_names->end())
    return;
  g_app_names->insert(app_name);

  // We need to register the window position pref.
  std::wstring window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append(L"_");
  window_pref.append(app_name);
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);

  prefs->RegisterDictionaryPref(window_pref.c_str());
}
