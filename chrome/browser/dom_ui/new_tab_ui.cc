// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/new_tab_ui.h"

#include "base/histogram.h"
#include "base/string_piece.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"
#include "chrome/browser/history_tab_ui.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// The URL scheme used for the new tab.
static const char kNewTabUIScheme[] = "chrome-internal";

// The number of most visited pages we show.
const int kMostVisitedPages = 9;

// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

// The number of recent bookmarks we show.
static const int kRecentBookmarks = 9;

// The number of search URLs to show.
static const int kSearchURLs = 3;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
static const wchar_t kRTLHtmlTextDirection[] = L"rtl";
static const wchar_t kDefaultHtmlTextDirection[] = L"ltr";

bool NewTabHTMLSource::first_view_ = true;

namespace {

// To measure end-to-end performance of the new tab page, we observe paint
// messages and wait for the page to stop repainting.
class PaintTimer : public RenderWidgetHost::PaintObserver {
 public:
  PaintTimer() : method_factory_(this) {
    Start();
  }

  // Start the benchmarking and the timer.
  void Start() {
    start_ = base::TimeTicks::Now();
    last_paint_ = start_;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&PaintTimer::Timeout), kTimeoutMs);
  }

  // A callback that is invoked whenever our RenderWidgetHost paints.
  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) {
    last_paint_ = base::TimeTicks::Now();
  }

  // The timer callback.  If enough time has elapsed since the last paint
  // message, we say we're done painting; otherwise, we keep waiting.
  void Timeout() {
    base::TimeTicks now = base::TimeTicks::Now();
    if ((now - last_paint_) >= base::TimeDelta::FromMilliseconds(kTimeoutMs)) {
      // Painting has quieted down.  Log this as the full time to run.
      base::TimeDelta load_time = last_paint_ - start_;
      int load_time_ms = static_cast<int>(load_time.InMilliseconds());
      NotificationService::current()->Notify(
          NotificationType::INITIAL_NEW_TAB_UI_LOAD,
          NotificationService::AllSources(),
          Details<int>(&load_time_ms));
      UMA_HISTOGRAM_TIMES(L"NewTabUI load", load_time);
    } else {
      // Not enough quiet time has elapsed.
      // Some more paints must've occurred since we set the timeout.
      // Wait some more.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          method_factory_.NewRunnableMethod(&PaintTimer::Timeout), kTimeoutMs);
    }
  }

 private:
  // The amount of time there must be no painting for us to consider painting
  // finished.  Observed times are in the ~1200ms range.
  static const int kTimeoutMs = 2000;
  // The time when we started benchmarking.
  base::TimeTicks start_;
  // The last time we got a paint notification.
  base::TimeTicks last_paint_;
  // Scoping so we can be sure our timeouts don't outlive us.
  ScopedRunnableMethodFactory<PaintTimer> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaintTimer);
};

// Adds "url", "title", and "direction" keys on incoming dictionary, setting
// title as the url as a fallback on empty title.
void SetURLTitleAndDirection(DictionaryValue* dictionary,
                             const std::wstring& title,
                             const GURL& gurl) {
  std::wstring wstring_url = UTF8ToWide(gurl.spec());
  dictionary->SetString(L"url", wstring_url);

  bool using_url_as_the_title = false;
  std::wstring title_to_set(title);
  if (title_to_set.empty()) {
    using_url_as_the_title = true;
    title_to_set = wstring_url;
  }

  // We set the "dir" attribute of the title, so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For example,
  // the title of http://msdn.microsoft.com/en-us/default.aspx is "MSDN:
  // Microsoft developer network". In RTL locales, in the [New Tab] page, if
  // the "dir" of this title is not specified, it takes Chrome UI's
  // directionality. So the title will be truncated as "soft developer
  // network". Setting the "dir" attribute as "ltr" renders the truncated title
  // as "MSDN: Microsoft D...". As another example, the title of
  // http://yahoo.com is "Yahoo!". In RTL locales, in the [New Tab] page, the
  // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
  // "ltr".
  //
  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings. Simply setting the title's "dir" attribute works
  // fine for rendering and truncating the title. However, it does not work for
  // entire title within a tooltip when the mouse is over the title link.. For
  // example, without LRE-PDF pair, the title "Yahoo!" will be rendered as
  // "!Yahoo" within the tooltip when the mouse is over the title link.
  std::wstring direction = kDefaultHtmlTextDirection;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    if (using_url_as_the_title) {
      l10n_util::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      if (l10n_util::StringContainsStrongRTLChars(title)) {
        l10n_util::WrapStringWithRTLFormatting(&title_to_set);
        direction = kRTLHtmlTextDirection;
      } else {
        l10n_util::WrapStringWithLTRFormatting(&title_to_set);
      }
    }
  }
  dictionary->SetString(L"title", title_to_set);
  dictionary->SetString(L"direction", direction);
}

}  // end anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

NewTabHTMLSource::NewTabHTMLSource()
    : DataSource("new-tab", MessageLoop::current()) {
}

void NewTabHTMLSource::StartDataRequest(const std::string& path,
                                        int request_id) {
  if (!path.empty()) {
    // A path under new-tab was requested; it's likely a bad relative
    // URL from the new tab page, but in any case it's an error.
    NOTREACHED();
    return;
  }

  // Show the profile name in the title and most visited labels if the current
  // profile is not the default.
  std::wstring title;
  std::wstring most_visited;
  if (UserDataManager::Get()->is_current_profile_default()) {
    title = l10n_util::GetString(IDS_NEW_TAB_TITLE);
    most_visited = l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED);
  } else {
    // Get the current profile name.
    std::wstring profile_name =
      UserDataManager::Get()->current_profile_name();
    title = l10n_util::GetStringF(IDS_NEW_TAB_TITLE_WITH_PROFILE_NAME,
                                  profile_name);
    most_visited = l10n_util::GetStringF(
        IDS_NEW_TAB_MOST_VISITED_WITH_PROFILE_NAME,
        profile_name);
  }
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title", title);
  localized_strings.SetString(L"mostvisited", most_visited);
  localized_strings.SetString(L"searches",
      l10n_util::GetString(IDS_NEW_TAB_SEARCHES));
  localized_strings.SetString(L"bookmarks",
      l10n_util::GetString(IDS_NEW_TAB_BOOKMARKS));
  localized_strings.SetString(L"showhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SHOW));
  localized_strings.SetString(L"searchhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SEARCH));
  localized_strings.SetString(L"recentlyclosed",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED));
  localized_strings.SetString(L"mostvisitedintro",
      l10n_util::GetStringF(IDS_NEW_TAB_MOST_VISITED_INTRO,
          l10n_util::GetString(IDS_WELCOME_PAGE_URL)));
  localized_strings.SetString(L"closedwindow",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW));

  localized_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       kRTLHtmlTextDirection : kDefaultHtmlTextDirection);

  // Let the tab know whether it's the first tab being viewed.
  localized_strings.SetString(L"firstview",
                              first_view_ ? L"true" : std::wstring());
  first_view_ = false;

#ifdef CHROME_PERSONALIZATION
  localized_strings.SetString(L"p13nsrc", Personalization::GetNewTabSource());
#endif

  static const StringPiece new_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_HTML));

  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      new_tab_html, &localized_strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

///////////////////////////////////////////////////////////////////////////////
// IncognitoTabHTMLSource

IncognitoTabHTMLSource::IncognitoTabHTMLSource()
    : DataSource("new-tab", MessageLoop::current()) {
}

void IncognitoTabHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"content",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_MESSAGE,
          l10n_util::GetString(IDS_LEARN_MORE_INCOGNITO_URL)));

  localized_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       kRTLHtmlTextDirection : kDefaultHtmlTextDirection);

  static const StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_INCOGNITO_TAB_HTML));

  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      incognito_tab_html, &localized_strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

///////////////////////////////////////////////////////////////////////////////
// MostVisitedHandler

MostVisitedHandler::MostVisitedHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host) {
  // Register ourselves as the handler for the "mostvisited" message from
  // Javascript.
  dom_ui_host_->RegisterMessageCallback("getMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleGetMostVisited));

  // Set up our sources for thumbnail and favicon data.
  // Ownership is passed to the ChromeURLDataManager.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new ThumbnailSource(dom_ui_host->profile())));
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new FavIconSource(dom_ui_host->profile())));

  // Get notifications when history is cleared.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NotificationType::HISTORY_URLS_DELETED,
                       Source<Profile>(dom_ui_host_->profile()));
}

MostVisitedHandler::~MostVisitedHandler() {
  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NotificationType::HISTORY_URLS_DELETED,
                          Source<Profile>(dom_ui_host_->profile()));
}

void MostVisitedHandler::HandleGetMostVisited(const Value* value) {
  HistoryService* hs =
      dom_ui_host_->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QuerySegmentUsageSince(
      &cancelable_consumer_,
      base::Time::Now() - base::TimeDelta::FromDays(kMostVisitedScope),
      NewCallback(this, &MostVisitedHandler::OnSegmentUsageAvailable));
}

void MostVisitedHandler::OnSegmentUsageAvailable(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* data) {
  most_visited_urls_.clear();

  ListValue pages_value;
  const size_t count = std::min<size_t>(kMostVisitedPages, data->size());
  for (size_t i = 0; i < count; ++i) {
    const PageUsageData& page = *(*data)[i];
    DictionaryValue* page_value = new DictionaryValue;
    SetURLTitleAndDirection(page_value, page.GetTitle(), page.GetURL());
    pages_value.Append(page_value);
    most_visited_urls_.push_back(page.GetURL());
  }
  dom_ui_host_->CallJavascriptFunction(L"mostVisitedPages", pages_value);
}

void MostVisitedHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Some URLs were deleted from history.  Reload the most visited list.
  HandleGetMostVisited(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// TemplateURLHandler

TemplateURLHandler::TemplateURLHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host), template_url_model_(NULL) {
  dom_ui_host->RegisterMessageCallback("getMostSearched",
      NewCallback(this, &TemplateURLHandler::HandleGetMostSearched));
  dom_ui_host->RegisterMessageCallback("doSearch",
      NewCallback(this, &TemplateURLHandler::HandleDoSearch));
}

TemplateURLHandler::~TemplateURLHandler() {
  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

void TemplateURLHandler::HandleGetMostSearched(const Value* content) {
  // The page Javascript has requested the list of keyword searches.
  // Start loading them from the template URL backend.
  if (!template_url_model_) {
    template_url_model_ = dom_ui_host_->profile()->GetTemplateURLModel();
    template_url_model_->AddObserver(this);
  }
  if (template_url_model_->loaded()) {
    OnTemplateURLModelChanged();
  } else {
    template_url_model_->Load();
  }
}

// A helper function for sorting TemplateURLs where the most used ones show up
// first.
static bool TemplateURLSortByUsage(const TemplateURL* a,
                                   const TemplateURL* b) {
  return a->usage_count() > b->usage_count();
}

void TemplateURLHandler::HandleDoSearch(const Value* content) {
  // Extract the parameters out of the input list.
  if (!content || !content->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() != 2) {
    NOTREACHED();
    return;
  }
  std::wstring keyword, search;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&keyword)) {
    NOTREACHED();
    return;
  }
  if (!args->Get(1, &value) || !value->GetAsString(&search)) {
    NOTREACHED();
    return;
  }

  // Combine the keyword and search into a URL.
  const TemplateURL* template_url =
      template_url_model_->GetTemplateURLForKeyword(keyword);
  if (!template_url) {
    // The keyword seems to have changed out from under us.
    // Not an error, but nothing we can do...
    return;
  }
  const TemplateURLRef* url_ref = template_url->url();
  if (!url_ref || !url_ref->SupportsReplacement()) {
    NOTREACHED();
    return;
  }
  GURL url = url_ref->ReplaceSearchTerms(*template_url, search,
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring());

  if (url.is_valid()) {
    // Load the URL.
    dom_ui_host_->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::LINK);

    // Record the user action
    std::vector<const TemplateURL*> urls =
        template_url_model_->GetTemplateURLs();
    sort(urls.begin(), urls.end(), TemplateURLSortByUsage);
    ListValue urls_value;
    int item_number = 0;
    for (size_t i = 0;
         i < std::min<size_t>(urls.size(), kSearchURLs); ++i) {
      if (urls[i]->usage_count() == 0)
        break;  // The remainder would be no good.

      const TemplateURLRef* urlref = urls[i]->url();
      if (!urlref)
        continue;

      if (urls[i] == template_url) {
        UserMetrics::RecordComputedAction(
            StringPrintf(L"NTP_SearchURL%d", item_number),
            dom_ui_host_->profile());
        break;
      }

      item_number++;
    }
  }
}

void TemplateURLHandler::OnTemplateURLModelChanged() {
  // We've loaded some template URLs.  Send them to the page.
  std::vector<const TemplateURL*> urls = template_url_model_->GetTemplateURLs();
  sort(urls.begin(), urls.end(), TemplateURLSortByUsage);
  ListValue urls_value;
  for (size_t i = 0; i < std::min<size_t>(urls.size(), kSearchURLs); ++i) {
    if (urls[i]->usage_count() == 0)
      break;  // urls is sorted by usage count; the remainder would be no good.

    const TemplateURLRef* urlref = urls[i]->url();
    if (!urlref)
      continue;
    DictionaryValue* entry_value = new DictionaryValue;
    entry_value->SetString(L"short_name", urls[i]->short_name());
    entry_value->SetString(L"keyword", urls[i]->keyword());

    const GURL& url = urls[i]->GetFavIconURL();
    if (url.is_valid())
     entry_value->SetString(L"favIconURL", UTF8ToWide(url.spec()));

    urls_value.Append(entry_value);
  }
  UMA_HISTOGRAM_COUNTS(L"NewTabPage.SearchURLs.Total", urls_value.GetSize());
  dom_ui_host_->CallJavascriptFunction(L"searchURLs", urls_value);
}

///////////////////////////////////////////////////////////////////////////////
// RecentlyBookmarkedHandler

RecentlyBookmarkedHandler::RecentlyBookmarkedHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host),
      model_(NULL) {
  dom_ui_host->RegisterMessageCallback("getRecentlyBookmarked",
      NewCallback(this,
                  &RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked));
}

RecentlyBookmarkedHandler::~RecentlyBookmarkedHandler() {
  if (model_)
    model_->RemoveObserver(this);
}

void RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked(const Value*) {
  if (!model_) {
    model_ = dom_ui_host_->profile()->GetBookmarkModel();
    model_->AddObserver(this);
  }
  // If the model is loaded, synchronously send the bookmarks down. Otherwise
  // when the model loads we'll send the bookmarks down.
  if (model_->IsLoaded())
    SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::SendBookmarksToPage() {
  std::vector<BookmarkNode*> recently_bookmarked;
  bookmark_utils::GetMostRecentlyAddedEntries(
      model_, kRecentBookmarks, &recently_bookmarked);
  ListValue list_value;
  for (size_t i = 0; i < recently_bookmarked.size(); ++i) {
    BookmarkNode* node = recently_bookmarked[i];
    DictionaryValue* entry_value = new DictionaryValue;
    SetURLTitleAndDirection(entry_value, node->GetTitle(), node->GetURL());
    list_value.Append(entry_value);
  }
  dom_ui_host_->CallJavascriptFunction(L"recentlyBookmarked", list_value);
}

void RecentlyBookmarkedHandler::Loaded(BookmarkModel* model) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeAdded(BookmarkModel* model,
                                                  BookmarkNode* parent,
                                                  int index) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeRemoved(BookmarkModel* model,
                                                    BookmarkNode* parent,
                                                    int index) {
  SendBookmarksToPage();
}

void RecentlyBookmarkedHandler::BookmarkNodeChanged(BookmarkModel* model,
                                                    BookmarkNode* node) {
  SendBookmarksToPage();
}

///////////////////////////////////////////////////////////////////////////////
// RecentlyClosedTabsHandler

RecentlyClosedTabsHandler::RecentlyClosedTabsHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host),
      tab_restore_service_(NULL) {
  dom_ui_host->RegisterMessageCallback("getRecentlyClosedTabs",
      NewCallback(this,
                  &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs));
  dom_ui_host->RegisterMessageCallback("reopenTab",
      NewCallback(this, &RecentlyClosedTabsHandler::HandleReopenTab));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const Value* content) {
  NavigationController* controller = dom_ui_host_->controller();
  Browser* browser = Browser::GetBrowserForController(
      controller, NULL);
  if (!browser)
    return;

  // Extract the integer value of the tab session to restore from the
  // incoming string array. This will be greatly simplified when
  // DOMUIBindings::send() is generalized to all data types instead of
  // silently failing when passed anything other then an array of
  // strings.
  if (content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        int session_to_restore = _wtoi(wstring_value.c_str());
        tab_restore_service_->RestoreEntryById(browser, session_to_restore,
                                               true);
        // The current tab has been nuked at this point; don't touch any member
        // variables.
      }
    }
  }
}

void RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs(
    const Value* content) {
  if (!tab_restore_service_) {
    tab_restore_service_ = dom_ui_host_->profile()->GetTabRestoreService();

    // GetTabRestoreService() can return NULL (i.e., when in Off the
    // Record mode)
    if (tab_restore_service_) {
      // This does nothing if the tabs have already been loaded or they
      // shouldn't be loaded.
      tab_restore_service_->LoadTabsFromLastSession();

      tab_restore_service_->AddObserver(this);
    }
  }

  if (tab_restore_service_)
    TabRestoreServiceChanged(tab_restore_service_);
}

void RecentlyClosedTabsHandler::TabRestoreServiceChanged(
    TabRestoreService* service) {
  const TabRestoreService::Entries& entries = service->entries();
  ListValue list_value;
  int added_count = 0;

  // We filter the list of recently closed to only show 'interesting' entries,
  // where an interesting entry is either a closed window or a closed tab
  // whose selected navigation is not the new tab ui.
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && added_count < 3; ++it) {
    TabRestoreService::Entry* entry = *it;
    DictionaryValue* value = new DictionaryValue();
    if ((entry->type == TabRestoreService::TAB &&
         TabToValue(*static_cast<TabRestoreService::Tab*>(entry), value)) ||
        (entry->type == TabRestoreService::WINDOW &&
         WindowToValue(*static_cast<TabRestoreService::Window*>(entry),
                       value))) {
      value->SetInteger(L"sessionId", entry->id);
      list_value.Append(value);
      added_count++;
    } else {
      delete value;
    }
  }
  dom_ui_host_->CallJavascriptFunction(L"recentlyClosedTabs", list_value);
}

void RecentlyClosedTabsHandler::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

bool RecentlyClosedTabsHandler::TabToValue(
    const TabRestoreService::Tab& tab,
    DictionaryValue* dictionary) {
  if (tab.navigations.empty())
    return false;

  const TabNavigation& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  if (current_navigation.url() == NewTabUIURL())
    return false;

  SetURLTitleAndDirection(dictionary, current_navigation.title(),
                          current_navigation.url());
  dictionary->SetString(L"type", L"tab");
  return true;
}

bool RecentlyClosedTabsHandler::WindowToValue(
    const TabRestoreService::Window& window,
    DictionaryValue* dictionary) {
  if (window.tabs.empty()) {
    NOTREACHED();
    return false;
  }

  ListValue* tab_values = new ListValue();
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    DictionaryValue* tab_value = new DictionaryValue();
    if (TabToValue(window.tabs[i], tab_value))
      tab_values->Append(tab_value);
    else
      delete tab_value;
  }
  if (tab_values->GetSize() == 0) {
    delete tab_values;
    return false;
  }

  dictionary->SetString(L"type", L"window");
  dictionary->Set(L"tabs", tab_values);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// HistoryHandler

HistoryHandler::HistoryHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host) {
  dom_ui_host->RegisterMessageCallback("showHistoryPage",
      NewCallback(this, &HistoryHandler::HandleShowHistoryPage));
  dom_ui_host->RegisterMessageCallback("searchHistoryPage",
      NewCallback(this, &HistoryHandler::HandleSearchHistoryPage));
}

void HistoryHandler::HandleShowHistoryPage(const Value*) {
  NavigationController* controller = dom_ui_host_->controller();
  if (controller) {
    controller->LoadURL(HistoryTabUI::GetURL(), GURL(), PageTransition::LINK);
    UserMetrics::RecordAction(L"NTP_ShowHistory",
        dom_ui_host_->profile());
  }
}

void HistoryHandler::HandleSearchHistoryPage(const Value* content) {
  if (content && content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        UserMetrics::RecordAction(L"NTP_SearchHistory",
            dom_ui_host_->profile());

        NavigationController* controller = dom_ui_host_->controller();
        controller->LoadURL(
            HistoryTabUI::GetHistoryURLWithSearchText(wstring_value),
            GURL(),
            PageTransition::LINK);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// MetricsHandler

MetricsHandler::MetricsHandler(DOMUIHost* dom_ui_host)
    : dom_ui_host_(dom_ui_host) {
  dom_ui_host->RegisterMessageCallback("metrics",
      NewCallback(this, &MetricsHandler::HandleMetrics));
}

void MetricsHandler::HandleMetrics(const Value* content) {
  if (content && content->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(content);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      if (string_value->GetAsString(&wstring_value)) {
        UserMetrics::RecordComputedAction(wstring_value,
            dom_ui_host_->profile());
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// NewTabUIContents

// This is the top-level URL handler for chrome-internal: URLs, and exposed in
// our header file.
bool NewTabUIHandleURL(GURL* url,
                       TabContentsType* result_type) {
  if (!url->SchemeIs(kNewTabUIScheme))
    return false;

  *result_type = TAB_CONTENTS_NEW_TAB_UI;
  *url = GURL(DOMUIContents::GetScheme() + "://new-tab/");

  return true;
}

GURL NewTabUIURL() {
  std::string url(kNewTabUIScheme);
  url += ":";
  return GURL(url);
}

NewTabUIContents::NewTabUIContents(Profile* profile,
    SiteInstance* instance, RenderViewHostFactory* render_view_factory) :
    DOMUIHost(profile, instance, render_view_factory),
    motd_message_id_(0),
    incognito_(false),
    most_visited_handler_(NULL) {
  set_type(TAB_CONTENTS_NEW_TAB_UI);

  // Show profile name in the title if the current profile is not the default.
  std::wstring title;
  if (UserDataManager::Get()->is_current_profile_default()) {
    title = l10n_util::GetString(IDS_NEW_TAB_TITLE);
  } else {
    title = l10n_util::GetStringF(
        IDS_NEW_TAB_TITLE_WITH_PROFILE_NAME,
        UserDataManager::Get()->current_profile_name());
  }
  set_forced_title(title);

  if (profile->IsOffTheRecord())
    incognito_ = true;

  if (NewTabHTMLSource::first_view() &&
      (profile->GetPrefs()->GetInteger(prefs::kRestoreOnStartup) != 0 ||
       !profile->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
     ) {
    NewTabHTMLSource::set_first_view(false);
  }

  render_view_host()->set_paint_observer(new PaintTimer);
}

void NewTabUIContents::AttachMessageHandlers() {
  // Regretfully, DataSources are global, instead of
  // per-TabContents. Because of the motd_message_id_ member, each
  // NewTabUIContents instance could theoretically have a different
  // message. Moving this from the constructor to here means that we
  // reconnect this source each time we reload so we should no longer
  // have the bug where we open a normal new tab page (no motd), open
  // another OTR new tab page (blurb motd describing what 'incognito'
  // means), refresh the normal new page (which now displays the
  // incognito blurb because that was the last NewTabHTMLSource hooked
  // up).
  //
  // This is a workaround until http://b/issue?id=1230312 is fixed.

  if (incognito_) {
    IncognitoTabHTMLSource* html_source = new IncognitoTabHTMLSource();

    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            html_source));
  } else {
    AddMessageHandler(new TemplateURLHandler(this));
    most_visited_handler_ = new MostVisitedHandler(this);
    AddMessageHandler(most_visited_handler_);  // Takes ownership.
    AddMessageHandler(new RecentlyBookmarkedHandler(this));
    AddMessageHandler(new RecentlyClosedTabsHandler(this));
    AddMessageHandler(new HistoryHandler(this));
    AddMessageHandler(new MetricsHandler(this));
#ifdef CHROME_PERSONALIZATION
    Personalization::AttachNewTabHandler(this);
#endif

    NewTabHTMLSource* html_source = new NewTabHTMLSource();

    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            html_source));
  }
}

bool NewTabUIContents::NavigateToPendingEntry(bool reload) {
  const bool result = WebContents::NavigateToPendingEntry(reload);
  controller()->GetPendingEntry()->set_title(forced_title_);
  return result;
}

const std::wstring& NewTabUIContents::GetTitle() const {
  if (!forced_title_.empty())
    return forced_title_;
  return WebContents::GetTitle();
}

void NewTabUIContents::SetInitialFocus() {
  if (delegate())
    delegate()->SetFocusToLocationBar();
}

bool NewTabUIContents::SupportsURL(GURL* url) {
  if (url->SchemeIs("javascript"))
    return true;
  return DOMUIHost::SupportsURL(url);
}

void NewTabUIContents::RequestOpenURL(const GURL& url,
                                      const GURL& /*referrer*/,
                                      WindowOpenDisposition disposition) {
  // The user opened a URL on the page (including "open in new window").
  // We count all such clicks as AUTO_BOOKMARK, which increments the site's
  // visit count (which is used for ranking the most visited entries).
  // Note this means we're including clicks on not only most visited thumbnails,
  // but also clicks on recently bookmarked.
  OpenURL(url, GURL(), disposition, PageTransition::AUTO_BOOKMARK);
}
