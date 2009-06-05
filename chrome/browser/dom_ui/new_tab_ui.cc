// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/dom_ui/new_tab_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/string_piece.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/dom_ui/dom_ui_thumbnail_source.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/dom_ui/downloads_dom_handler.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// The number of most visited pages we show.
const size_t kMostVisitedPages = 9;

// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

// The number of recent bookmarks we show.
const int kRecentBookmarks = 9;

// The number of search URLs to show.
const int kSearchURLs = 3;

// Strings sent to the page via jstemplates used to set the direction of the
// HTML document based on locale.
const wchar_t kRTLHtmlTextDirection[] = L"rtl";
const wchar_t kDefaultHtmlTextDirection[] = L"ltr";

// Adds "url", "title", and "direction" keys on incoming dictionary, setting
// title as the url as a fallback on empty title.
void SetURLTitleAndDirection(DictionaryValue* dictionary,
                             const string16& title,
                             const GURL& gurl) {
  std::wstring wstring_url = UTF8ToWide(gurl.spec());
  dictionary->SetString(L"url", wstring_url);

  std::wstring wstring_title = UTF16ToWide(title);

  bool using_url_as_the_title = false;
  std::wstring title_to_set(wstring_title);
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
      if (l10n_util::StringContainsStrongRTLChars(wstring_title)) {
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

////////////////////////////////////////////////////////////////////////////////
// PaintTimer

// To measure end-to-end performance of the new tab page, we observe paint
// messages and wait for the page to stop repainting.
class PaintTimer : public RenderWidgetHost::PaintObserver {
 public:
  PaintTimer()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
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
      UMA_HISTOGRAM_TIMES("NewTabUI load", load_time);
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

///////////////////////////////////////////////////////////////////////////////
// NewTabHTMLSource

class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit NewTabHTMLSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

  // Setters and getters for first_view.
  static void set_first_view(bool first_view) { first_view_ = first_view; }
  static bool first_view() { return first_view_; }

 private:
  // In case a file path to the new new tab page was provided this tries to load
  // the file and returns the file content if successful. This returns an empty
  // string in case of failure.
  static std::string GetNewNewTabFromCommandLine();

  // Whether this is the is the first viewing of the new tab page and
  // we think it is the user's startup page.
  static bool first_view_;

  // The user's profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
};

bool NewTabHTMLSource::first_view_ = true;

NewTabHTMLSource::NewTabHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()),
      profile_(profile) {
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
  localized_strings.SetString(L"bookmarkbarattached",
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      "true" : "false");
  localized_strings.SetString(L"hasattribution",
      profile_->GetThemeProvider()->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ?
      "true" : "false");
  localized_strings.SetString(L"title", title);
  localized_strings.SetString(L"mostvisited", most_visited);
  localized_strings.SetString(L"searches",
      l10n_util::GetString(IDS_NEW_TAB_SEARCHES));
  localized_strings.SetString(L"bookmarks",
      l10n_util::GetString(IDS_NEW_TAB_BOOKMARKS));
  localized_strings.SetString(L"recent",
      l10n_util::GetString(IDS_NEW_TAB_RECENT));
  localized_strings.SetString(L"showhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SHOW));
  localized_strings.SetString(L"showhistoryurl",
      chrome::kChromeUIHistoryURL);
  localized_strings.SetString(L"editthumbnails",
      l10n_util::GetString(IDS_NEW_TAB_REMOVE_THUMBNAILS));
  localized_strings.SetString(L"restorethumbnails",
      l10n_util::GetString(IDS_NEW_TAB_RESTORE_THUMBNAILS_LINK));
  localized_strings.SetString(L"editmodeheading",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_EDIT_MODE_HEADING));
  localized_strings.SetString(L"doneediting",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_DONE_REMOVING_BUTTON));
  localized_strings.SetString(L"cancelediting",
      l10n_util::GetString(IDS_NEW_TAB_MOST_VISITED_CANCEL_REMOVING_BUTTON));
  localized_strings.SetString(L"searchhistory",
      l10n_util::GetString(IDS_NEW_TAB_HISTORY_SEARCH));
  localized_strings.SetString(L"recentlyclosed",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED));
  localized_strings.SetString(L"mostvisitedintro",
      l10n_util::GetStringF(IDS_NEW_TAB_MOST_VISITED_INTRO,
          l10n_util::GetString(IDS_WELCOME_PAGE_URL)));
  localized_strings.SetString(L"closedwindowsingle",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE));
  localized_strings.SetString(L"closedwindowmultiple",
      l10n_util::GetString(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE));
  localized_strings.SetString(L"attributionintro",
      l10n_util::GetString(IDS_NEW_TAB_ATTRIBUTION_INTRO));

  SetFontAndTextDirection(&localized_strings);

  // Let the tab know whether it's the first tab being viewed.
  localized_strings.SetString(L"firstview",
                              first_view_ ? L"true" : std::wstring());
  first_view_ = false;

#ifdef CHROME_PERSONALIZATION
  localized_strings.SetString(L"p13nsrc", Personalization::GetNewTabSource());
#endif

  // In case we have the new new tab page enabled we first try to read the file
  // provided on the command line. If that fails we just get the resource from
  // the resource bundle.
  StringPiece new_tab_html;
  std::string new_tab_html_str;
  if (NewTabUI::EnableNewNewTabPage()) {
    new_tab_html_str = GetNewNewTabFromCommandLine();

    if (!new_tab_html_str.empty()) {
      new_tab_html = StringPiece(new_tab_html_str);
    } else {
      // Use the new new tab page from the resource bundle.
      new_tab_html = ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_NEW_NEW_TAB_HTML);
    }
  } else {
    // Use the default new tab page resource.
    new_tab_html = ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_NEW_TAB_HTML);
  }

  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      new_tab_html, &localized_strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

// static
std::string NewTabHTMLSource::GetNewNewTabFromCommandLine() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  const std::wstring file_path_wstring = command_line->GetSwitchValue(
      switches::kNewNewTabPage);

#if defined(OS_WIN)
  const FilePath::StringType file_path = file_path_wstring;
#else
  const FilePath::StringType file_path = WideToASCII(file_path_wstring);
#endif

  if (!file_path.empty()) {
    std::string file_contents;
    if (file_util::ReadFileToString(FilePath(file_path), &file_contents))
      return file_contents;
  }

  return std::string();
}

///////////////////////////////////////////////////////////////////////////////
// IncognitoTabHTMLSource

class IncognitoTabHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // Creates our datasource and sets our user message to a specific message
  // from our string bundle.
  IncognitoTabHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoTabHTMLSource);
};

IncognitoTabHTMLSource::IncognitoTabHTMLSource()
    : DataSource(chrome::kChromeUINewTabHost, MessageLoop::current()) {
}

void IncognitoTabHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_NEW_TAB_TITLE));
  localized_strings.SetString(L"content",
      l10n_util::GetStringF(IDS_NEW_TAB_OTR_MESSAGE,
          l10n_util::GetString(IDS_LEARN_MORE_INCOGNITO_URL)));

  SetFontAndTextDirection(&localized_strings);

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

// The handler for Javascript messages related to the "most visited" view.
class MostVisitedHandler : public DOMMessageHandler,
                           public NotificationObserver {
 public:
  explicit MostVisitedHandler(DOMUI* dom_ui);
  virtual ~MostVisitedHandler();

  // Callback for the "getMostVisited" message.
  void HandleGetMostVisited(const Value* value);

  // Callback for the "blacklistURLFromMostVisited" message.
  void HandleBlacklistURL(const Value* url);

  // Callback for the "removeURLsFromMostVisitedBlacklist" message.
  void HandleRemoveURLsFromBlacklist(const Value* url);

  // Callback for the "clearMostVisitedURLsBlacklist" message.
  void HandleClearBlacklist(const Value* url);

  // Callback for the "addPinnedURL" message.
  void HandleAddPinnedURL(const Value* value);

  // Callback for the "removePinnedURL" message.
  void HandleRemovePinnedURL(const Value* value);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const std::vector<GURL>& most_visited_urls() const {
    return most_visited_urls_;
  }

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // Callback from the history system when the most visited list is available.
  void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* data);

  // Puts the passed URL in the blacklist (so it does not show as a thumbnail).
  void BlacklistURL(const GURL& url);

  // Returns the key used in url_blacklist_ and pinned_urls_ for the passed
  // |url|.
  std::wstring GetDictionaryKeyForURL(const std::string& url);

  // Gets the |url| and |title| for a pinned URL at a given index. This returns
  // true if found.
  const bool GetPinnedURLAtIndex(const int index, std::string* url,
                                 std::string* title);

  void AddPinnedURL(const GURL& url, const std::string& title, int index);
  void RemovePinnedURL(const GURL& url);

  NotificationRegistrar registrar_;

  // Our consumer for the history service.
  CancelableRequestConsumerTSimple<PageUsageData*> cancelable_consumer_;

  // The most visited URLs, in priority order.
  // Only used for matching up clicks on the page to which most visited entry
  // was clicked on for metrics purposes.
  std::vector<GURL> most_visited_urls_;

  // The URL blacklist: URLs we do not want to show in the thumbnails list.  It
  // is a dictionary for quick access (it associates a dummy boolean to the URL
  // string).
  DictionaryValue* url_blacklist_;

  // This is a dictionary for the pinned URLs for the the most visited part of
  // the new tab page. The key of the dictionary is a hash of the URL and the
  // value is a dictionary with title, url and index.
  DictionaryValue* pinned_urls_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedHandler);
};

MostVisitedHandler::MostVisitedHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui) {
  // Register ourselves as the handler for the "mostvisited" message from
  // Javascript.
  dom_ui_->RegisterMessageCallback("getMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleGetMostVisited));

  // Register ourselves for any most-visited item blacklisting.
  dom_ui_->RegisterMessageCallback("blacklistURLFromMostVisited",
      NewCallback(this, &MostVisitedHandler::HandleBlacklistURL));
  dom_ui_->RegisterMessageCallback("removeURLsFromMostVisitedBlacklist",
      NewCallback(this, &MostVisitedHandler::HandleRemoveURLsFromBlacklist));
  dom_ui_->RegisterMessageCallback("clearMostVisitedURLsBlacklist",
      NewCallback(this, &MostVisitedHandler::HandleClearBlacklist));

  url_blacklist_ = dom_ui_->GetProfile()->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);

  // Register ourself for pinned URL messages.
  dom_ui->RegisterMessageCallback("addPinnedURL",
      NewCallback(this, &MostVisitedHandler::HandleAddPinnedURL));
  dom_ui->RegisterMessageCallback("removePinnedURL",
      NewCallback(this, &MostVisitedHandler::HandleRemovePinnedURL));

  pinned_urls_ = dom_ui_->GetProfile()->GetPrefs()->
    GetMutableDictionary(prefs::kNTPMostVisitedPinnedURLs);

  // Set up our sources for thumbnail and favicon data. Since we may be in
  // testing mode with no I/O thread, only add our handler when an I/O thread
  // exists. Ownership is passed to the ChromeURLDataManager.
  if (g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
                          &ChromeURLDataManager::AddDataSource,
                          new DOMUIThumbnailSource(dom_ui->GetProfile())));
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
                          &ChromeURLDataManager::AddDataSource,
                          new DOMUIFavIconSource(dom_ui->GetProfile())));
  }

  // Get notifications when history is cleared.
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                 Source<Profile>(dom_ui_->GetProfile()));
}

MostVisitedHandler::~MostVisitedHandler() {
}

void MostVisitedHandler::HandleGetMostVisited(const Value* value) {
  const int kMostVisitedCount = 9;
  // Let's query for the number of items we want plus the blacklist size as
  // we'll be filtering-out the returned list with the blacklist URLs.
  // We do not subtract the number of pinned URLs we have because the
  // HistoryService does not know about those.
  int result_count = kMostVisitedCount + url_blacklist_->GetSize();
  HistoryService* hs =
      dom_ui_->GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QuerySegmentUsageSince(
      &cancelable_consumer_,
      base::Time::Now() - base::TimeDelta::FromDays(kMostVisitedScope),
      result_count,
      NewCallback(this, &MostVisitedHandler::OnSegmentUsageAvailable));
}

void MostVisitedHandler::HandleBlacklistURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  std::string url;
  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() == 0 || !list->GetString(0, &url)) {
    NOTREACHED();
    return;
  }
  BlacklistURL(GURL(url));
  // Force a refresh of the thumbnails.
  HandleGetMostVisited(NULL);
}

void MostVisitedHandler::HandleRemoveURLsFromBlacklist(const Value* urls) {
  if (!urls->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }
  const ListValue* list = static_cast<const ListValue*>(urls);
  if (list->GetSize() == 0) {
    NOTREACHED();
    return;
  }

  for (ListValue::const_iterator iter = list->begin();
       iter != list->end(); ++iter) {
    std::wstring url;
    bool r = (*iter)->GetAsString(&url);
    if (!r) {
      NOTREACHED();
      return;
    }
    r = url_blacklist_->Remove(GetDictionaryKeyForURL(WideToUTF8(url)), NULL);
    DCHECK(r) << "Unknown URL removed from the NTP Most Visited blacklist.";
  }

  // Force a refresh of the thumbnails.
  HandleGetMostVisited(NULL);
}

void MostVisitedHandler::HandleClearBlacklist(const Value* value) {
  url_blacklist_->Clear();
  // Force a refresh of the thumbnails.
  HandleGetMostVisited(NULL);
}

void MostVisitedHandler::HandleAddPinnedURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  std::string url;
  std::string title;
  std::string index_string;
  int index;

  bool r = list->GetString(0, &url);
  DCHECK(r) << "Missing URL in addPinnedURL from the NTP Most Visited.";

  r = list->GetString(1, &title);
  DCHECK(r) << "Missing title in addPinnedURL from the NTP Most Visited.";

  r = list->GetString(2, &index_string);
  DCHECK(r) << "Missing index in addPinnedURL from the NTP Most Visited.";
  index = StringToInt(index_string);

  AddPinnedURL(GURL(url), title, index);
}

void MostVisitedHandler::AddPinnedURL(const GURL& url, const std::string& title,
                                      int index) {
  // Remove any pinned URL at the given index.
  std::string old_url;
  std::string old_title;
  if (GetPinnedURLAtIndex(index, &old_url, &old_title))
    RemovePinnedURL(GURL(old_url));

  DictionaryValue* new_value = new DictionaryValue();
  SetURLTitleAndDirection(new_value, UTF8ToUTF16(title), url);
  bool r = new_value->SetInteger(L"index", index);
  DCHECK(r) << "Failed to set the index for a pinned URL from the NTP Most "
            << "Visited.";

  r = pinned_urls_->Set(GetDictionaryKeyForURL(url.spec()), new_value);
  DCHECK(r) << "Failed to add pinned URL from the NTP Most Visited.";

  // TODO(arv): Notify observers?

  // Don't call HandleGetMostVisited. Let the client call this as needed.
}

void MostVisitedHandler::HandleRemovePinnedURL(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  std::string url;

  bool r = list->GetString(0, &url);
  DCHECK(r) << "Failed to read the URL to remove from the NTP Most Visited.";

  RemovePinnedURL(GURL(url));
}

void MostVisitedHandler::RemovePinnedURL(const GURL& url) {
  const std::wstring key = GetDictionaryKeyForURL(url.spec());
  if (pinned_urls_->HasKey(key))
    pinned_urls_->Remove(key, NULL);

  // TODO(arv): Notify observers?

  // Don't call HandleGetMostVisited. Let the client call this as needed.
}

const bool MostVisitedHandler::GetPinnedURLAtIndex(const int index,
                                                   std::string* url,
                                                   std::string* title) {
  // This iterates over all the pinned URLs. It might seem like it is worth
  // having a map from the index to the item but the number of items is limited
  // to the number of items the most visited section is showing on the NTP so
  // this will be fast enough for now.
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
      it != pinned_urls_->end_keys(); ++it) {
    Value* value;
    if (pinned_urls_->Get(*it, &value)) {
      if (!value->IsType(DictionaryValue::TYPE_DICTIONARY)) {
        NOTREACHED();
        return false;
      }

      int dict_index;
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      dict->GetInteger(L"index", &dict_index);
      if (dict_index == index) {
        if (dict->GetString(L"url", url))
          return false;
        return dict->GetString(L"title", title);
      }
    } else {
      NOTREACHED() << "DictionaryValue iterators are filthy liars.";
    }
  }

  return false;
}

void MostVisitedHandler::OnSegmentUsageAvailable(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* data) {
  most_visited_urls_.clear();
  ListValue pages_value;

  size_t i = 0;
  size_t j = 0;
  while (j < kMostVisitedPages && i < data->size()) {
    bool pinned = false;
    GURL url;
    string16 title;
    std::string pinned_url;
    std::string pinned_title;

    if (MostVisitedHandler::GetPinnedURLAtIndex(j, &pinned_url,
                                                &pinned_title)) {
      url = GURL(pinned_url);
      title = UTF8ToUTF16(pinned_title);
      pinned = true;
      j++;
    } else {
      const PageUsageData& page = *(*data)[i];
      i++;
      url = page.GetURL();

      // Don't include blacklisted or pinned URLs.
      std::wstring key = GetDictionaryKeyForURL(url.spec());
      if (pinned_urls_->HasKey(key) || url_blacklist_->HasKey(key))
        continue;

      title = page.GetTitle();
    }

    // Found a page.
    DictionaryValue* page_value = new DictionaryValue();
    SetURLTitleAndDirection(page_value, title, url);
    page_value->SetBoolean(L"pinned", pinned);
    pages_value.Append(page_value);
    most_visited_urls_.push_back(url);
  }

  dom_ui_->CallJavascriptFunction(L"mostVisitedPages", pages_value);
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

void MostVisitedHandler::BlacklistURL(const GURL& url) {
  RemovePinnedURL(url);

  std::wstring key = GetDictionaryKeyForURL(url.spec());
  if (url_blacklist_->HasKey(key))
    return;
  url_blacklist_->SetBoolean(key, true);
}

std::wstring MostVisitedHandler::GetDictionaryKeyForURL(
    const std::string& url) {
  return ASCIIToWide(MD5String(url));
}

// static
void MostVisitedHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedURLsBlacklist);
  prefs->RegisterDictionaryPref(prefs::kNTPMostVisitedPinnedURLs);
}

///////////////////////////////////////////////////////////////////////////////
// TemplateURLHandler

// The handler for Javascript messages related to the "common searches" view.
class TemplateURLHandler : public DOMMessageHandler,
                           public TemplateURLModelObserver {
 public:
  explicit TemplateURLHandler(DOMUI* dom_ui);
  virtual ~TemplateURLHandler();

  // Callback for the "getMostSearched" message, sent when the page requests
  // the list of available searches.
  void HandleGetMostSearched(const Value* content);
  // Callback for the "doSearch" message, sent when the user wants to
  // run a search.  Content of the message is an array containing
  // [<the search keyword>, <the search term>].
  void HandleDoSearch(const Value* content);

  // TemplateURLModelObserver implementation.
  virtual void OnTemplateURLModelChanged();

 private:
  DOMUI* dom_ui_;
  TemplateURLModel* template_url_model_;  // Owned by profile.

  DISALLOW_COPY_AND_ASSIGN(TemplateURLHandler);
};

TemplateURLHandler::TemplateURLHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui),
      template_url_model_(NULL) {
  dom_ui->RegisterMessageCallback("getMostSearched",
      NewCallback(this, &TemplateURLHandler::HandleGetMostSearched));
  dom_ui->RegisterMessageCallback("doSearch",
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
    template_url_model_ = dom_ui_->GetProfile()->GetTemplateURLModel();
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
            dom_ui_->GetProfile());
        break;
      }

      item_number++;
    }

    // Load the URL.
    dom_ui_->tab_contents()->OpenURL(url, GURL(), CURRENT_TAB,
                                     PageTransition::LINK);
    // We've been deleted.
    return;
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
  UMA_HISTOGRAM_COUNTS("NewTabPage.SearchURLs.Total", urls_value.GetSize());
  dom_ui_->CallJavascriptFunction(L"searchURLs", urls_value);
}

///////////////////////////////////////////////////////////////////////////////
// RecentlyBookmarkedHandler

class RecentlyBookmarkedHandler : public DOMMessageHandler,
                                  public BookmarkModelObserver {
 public:
  explicit RecentlyBookmarkedHandler(DOMUI* dom_ui);
  ~RecentlyBookmarkedHandler();

  // Callback which navigates to the bookmarks page.
  void HandleShowBookmarkPage(const Value*);

  // Callback for the "getRecentlyBookmarked" message.
  // It takes no arguments.
  void HandleGetRecentlyBookmarked(const Value*);

 private:
  void SendBookmarksToPage();

  // BookmarkModelObserver methods. These invoke SendBookmarksToPage.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);

  // These won't effect what is shown, so they do nothing.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}

  DOMUI* dom_ui_;
  // The model we're getting bookmarks from. The model is owned by the Profile.
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyBookmarkedHandler);
};

RecentlyBookmarkedHandler::RecentlyBookmarkedHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui),
      model_(NULL) {
  dom_ui->RegisterMessageCallback("getRecentlyBookmarked",
      NewCallback(this,
                  &RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked));
}

RecentlyBookmarkedHandler::~RecentlyBookmarkedHandler() {
  if (model_)
    model_->RemoveObserver(this);
}

void RecentlyBookmarkedHandler::HandleGetRecentlyBookmarked(const Value*) {
  if (!model_) {
    model_ = dom_ui_->GetProfile()->GetBookmarkModel();
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
    SetURLTitleAndDirection(entry_value,
                            WideToUTF16(node->GetTitle()), node->GetURL());
    entry_value->SetInteger(L"time",
                            static_cast<int>(node->date_added().ToTimeT()));
    list_value.Append(entry_value);
  }
  dom_ui_->CallJavascriptFunction(L"recentlyBookmarked", list_value);
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

class RecentlyClosedTabsHandler : public DOMMessageHandler,
                                  public TabRestoreService::Observer {
 public:
  explicit RecentlyClosedTabsHandler(DOMUI* dom_ui);
  virtual ~RecentlyClosedTabsHandler();

  // Callback for the "reopenTab" message. Rewrites the history of the
  // currently displayed tab to be the one in TabRestoreService with a
  // history of a session passed in through the content pointer.
  void HandleReopenTab(const Value* content);

  // Callback for the "getRecentlyClosedTabs" message.
  void HandleGetRecentlyClosedTabs(const Value* content);

  // Observer callback for TabRestoreService::Observer. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service);

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

 private:
  // Converts a closed tab to the value sent down to the NTP. Returns true on
  // success, false if the value shouldn't be sent down.
  bool TabToValue(const TabRestoreService::Tab& tab,
                  DictionaryValue* dictionary);

  // Converts a closed window to the value sent down to the NTP. Returns true
  // on success, false if the value shouldn't be sent down.
  bool WindowToValue(const TabRestoreService::Window& window,
                     DictionaryValue* dictionary);

  DOMUI* dom_ui_;

  // TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyClosedTabsHandler);
};

RecentlyClosedTabsHandler::RecentlyClosedTabsHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui),
      tab_restore_service_(NULL) {
  dom_ui->RegisterMessageCallback("getRecentlyClosedTabs",
      NewCallback(this,
                  &RecentlyClosedTabsHandler::HandleGetRecentlyClosedTabs));
  dom_ui->RegisterMessageCallback("reopenTab",
      NewCallback(this, &RecentlyClosedTabsHandler::HandleReopenTab));
}

RecentlyClosedTabsHandler::~RecentlyClosedTabsHandler() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsHandler::HandleReopenTab(const Value* content) {
  Browser* browser = Browser::GetBrowserForController(
      &dom_ui_->tab_contents()->controller(), NULL);
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
        int session_to_restore = StringToInt(WideToUTF16Hack(wstring_value));
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
    tab_restore_service_ = dom_ui_->GetProfile()->GetTabRestoreService();

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
  dom_ui_->CallJavascriptFunction(L"recentlyClosedTabs", list_value);
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
  if (current_navigation.url() == GURL(chrome::kChromeUINewTabURL))
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

class HistoryHandler : public DOMMessageHandler {
 public:
  explicit HistoryHandler(DOMUI* dom_ui);

  // Callback which navigates to the history page and performs a search.
  void HandleSearchHistoryPage(const Value* content);

 private:
  DOMUI* dom_ui_;

  DISALLOW_COPY_AND_ASSIGN(HistoryHandler);
};

HistoryHandler::HistoryHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui) {
  dom_ui->RegisterMessageCallback("searchHistoryPage",
      NewCallback(this, &HistoryHandler::HandleSearchHistoryPage));
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
        UserMetrics::RecordAction(L"NTP_SearchHistory", dom_ui_->GetProfile());
        dom_ui_->tab_contents()->controller().LoadURL(
            HistoryUI::GetHistoryURLWithSearchText(wstring_value),
            GURL(),
            PageTransition::LINK);
        // We are deleted by LoadURL, so do not call anything else.
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// MetricsHandler

// Let the page contents record UMA actions. Only use when you can't do it from
// C++. For example, we currently use it to let the NTP log the postion of the
// Most Visited or Bookmark the user clicked on, as we don't get that
// information through RequestOpenURL. You will need to update the metrics
// dashboard with the action names you use, as our processor won't catch that
// information (treat it as RecordComputedMetrics)
class MetricsHandler : public DOMMessageHandler {
 public:
  explicit MetricsHandler(DOMUI* dom_ui);

  // Callback which records a user action.
  void HandleMetrics(const Value* content);

 private:
  DOMUI* dom_ui_;

  DISALLOW_COPY_AND_ASSIGN(MetricsHandler);
};

MetricsHandler::MetricsHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui) {
  dom_ui->RegisterMessageCallback("metrics",
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
        UserMetrics::RecordComputedAction(wstring_value, dom_ui_->GetProfile());
      }
    }
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NewTabUI

NewTabUI::NewTabUI(TabContents* contents)
    : DOMUI(contents),
      motd_message_id_(0),
      incognito_(false) {
  // Override some options on the DOM UI.
  hide_favicon_ = true;
  force_bookmark_bar_visible_ = true;
  focus_location_bar_by_default_ = true;
  should_hide_url_ = true;
  overridden_title_ = WideToUTF16Hack(l10n_util::GetString(IDS_NEW_TAB_TITLE));

  // We count all link clicks as AUTO_BOOKMARK, so that site can be ranked more
  // highly. Note this means we're including clicks on not only most visited
  // thumbnails, but also clicks on recently bookmarked.
  link_transition_type_ = PageTransition::AUTO_BOOKMARK;

  if (NewTabHTMLSource::first_view() &&
      (GetProfile()->GetPrefs()->GetInteger(prefs::kRestoreOnStartup) != 0 ||
       !GetProfile()->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))) {
    NewTabHTMLSource::set_first_view(false);
  }

  tab_contents()->render_view_host()->set_paint_observer(new PaintTimer);

  if (GetProfile()->IsOffTheRecord()) {
    incognito_ = true;

    IncognitoTabHTMLSource* html_source = new IncognitoTabHTMLSource();

    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            html_source));
  } else {

    if (EnableNewNewTabPage()) {
      DownloadManager* dlm = GetProfile()->GetDownloadManager();
      DownloadsDOMHandler* downloads_handler =
          new DownloadsDOMHandler(this, dlm);

      AddMessageHandler(downloads_handler);
      AddMessageHandler(new BrowsingHistoryHandler(this));

      downloads_handler->Init();
    }

    AddMessageHandler(new TemplateURLHandler(this));
    AddMessageHandler(new MostVisitedHandler(this));
    AddMessageHandler(new RecentlyBookmarkedHandler(this));
    AddMessageHandler(new RecentlyClosedTabsHandler(this));
    AddMessageHandler(new HistoryHandler(this));
    AddMessageHandler(new MetricsHandler(this));
#ifdef CHROME_PERSONALIZATION
    if (!Personalization::IsP13NDisabled(GetProfile())) {
      AddMessageHandler(Personalization::CreateNewTabPageHandler(this));
    }
#endif

    // In testing mode there may not be an I/O thread.
    if (g_browser_process->io_thread()) {
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(&chrome_url_data_manager,
              &ChromeURLDataManager::AddDataSource,
              new DOMUIThemeSource(GetProfile())));

      NewTabHTMLSource* html_source = new NewTabHTMLSource(GetProfile());
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(&chrome_url_data_manager,
              &ChromeURLDataManager::AddDataSource,
              html_source));
    }
  }

  // Listen for theme installation.
  registrar_.Add(this, NotificationType::THEME_INSTALLED,
                 NotificationService::AllSources());
  // Listen for bookmark bar visibility changes.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
}

NewTabUI::~NewTabUI() {
}

void NewTabUI::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (NotificationType::THEME_INSTALLED == type) {
    CallJavascriptFunction(L"themeChanged");
  } else if (NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED) {
    if (GetProfile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
      CallJavascriptFunction(L"bookmarkBarAttached");
    else
      CallJavascriptFunction(L"bookmarkBarDetached");
  }
}


// static
void NewTabUI::RegisterUserPrefs(PrefService* prefs) {
  MostVisitedHandler::RegisterUserPrefs(prefs);
}

// static
bool NewTabUI::EnableNewNewTabPage() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kNewNewTabPage);
}
