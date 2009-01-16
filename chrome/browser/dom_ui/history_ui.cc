// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/history_ui.h"

#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/time_format.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/time_format.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using base::Time;

// HistoryUI is accessible from chrome://history, and the raw HTML is
// accessed from chrome://history.
static const char kHistoryHost[] = "history";

// Maximum number of search results to return in a given search. We should 
// eventually remove this.
static const int kMaxSearchResults = 100;

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

HistoryUIHTMLSource::HistoryUIHTMLSource()
    : DataSource(kHistoryHost, MessageLoop::current()) {
}

void HistoryUIHTMLSource::StartDataRequest(const std::string& path,
                                           int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_HISTORY_TITLE));
  localized_strings.SetString(L"loading",
      l10n_util::GetString(IDS_HISTORY_LOADING));
  localized_strings.SetString(L"newest",
      l10n_util::GetString(IDS_HISTORY_NEWEST));
  localized_strings.SetString(L"newer",
      l10n_util::GetString(IDS_HISTORY_NEWER));
  localized_strings.SetString(L"older",
      l10n_util::GetString(IDS_HISTORY_OLDER));
  localized_strings.SetString(L"searchresultsfor",
      l10n_util::GetString(IDS_HISTORY_SEARCHRESULTSFOR));
  localized_strings.SetString(L"history",
      l10n_util::GetString(IDS_HISTORY_BROWSERESULTS));
  localized_strings.SetString(L"cont",
      l10n_util::GetString(IDS_HISTORY_CONTINUED));
  localized_strings.SetString(L"searchbutton",
      l10n_util::GetString(IDS_HISTORY_SEARCH_BUTTON));
  localized_strings.SetString(L"noresults",
      l10n_util::GetString(IDS_HISTORY_NO_RESULTS));
  localized_strings.SetString(L"noitems",
      l10n_util::GetString(IDS_HISTORY_NO_ITEMS));
  localized_strings.SetString(L"delete",
      l10n_util::GetString(IDS_HISTORY_DELETE));

  static const StringPiece history_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HISTORY_HTML));
  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      history_html, &localized_strings, "t");

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryHandler
//
////////////////////////////////////////////////////////////////////////////////
BrowsingHistoryHandler::BrowsingHistoryHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      search_text_() {
  dom_ui_->RegisterMessageCallback("getHistory",
      NewCallback(this, &BrowsingHistoryHandler::HandleGetHistory));

  // Create our favicon data source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new FavIconSource(dom_ui_->get_profile())));

  // Get notifications when history is cleared.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NOTIFY_HISTORY_URLS_DELETED,
                       Source<Profile>(dom_ui_->get_profile()));
}

BrowsingHistoryHandler::~BrowsingHistoryHandler() {
  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NOTIFY_HISTORY_URLS_DELETED,
                          Source<Profile>(dom_ui_->get_profile()));
}

void BrowsingHistoryHandler::HandleGetHistory(const Value* value) {
  // Anything in-flight is invalid.
  cancelable_consumer_.CancelAllRequests();

  // Get arguments (if any).
  int month;
  std::wstring query;
  ExtractGetHistoryArguments(value, &month, &query);

  // Set our query options.
  history::QueryOptions options = CreateQueryOptions(month, query);

  // Need to remember the query string for our results.
  search_text_ = query;
  HistoryService* hs =
    dom_ui_->get_profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text_,
      options,
      &cancelable_consumer_, 
      NewCallback(this, &BrowsingHistoryHandler::QueryComplete));
}

void BrowsingHistoryHandler::QueryComplete(
    HistoryService::Handle request_handle,
    history::QueryResults* results) {

  ListValue results_value;
  Time midnight_today = Time::Now().LocalMidnight();

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    DictionaryValue* page_value = new DictionaryValue();
    SetURLAndTitle(page_value, page.title(), page.url());

    // Need to pass the time in epoch time (fastest JS conversion).
    page_value->SetInteger(L"time", 
        static_cast<int>(page.visit_time().ToTimeT()));

    // Until we get some JS i18n infrastructure, we also need to
    // pass the dates in as strings. This could use some 
    // optimization.

    // Only pass in the strings we need (search results need a shortdate
    // and snippet, browse results need day and time information).
    if (search_text_.empty()) {
      // Figure out the relative date string.
      std::wstring date_str = TimeFormat::RelativeDate(page.visit_time(),
                                                       &midnight_today);
      if (date_str.empty()) {
        date_str = base::TimeFormatFriendlyDate(page.visit_time());
      } else {
        date_str = l10n_util::GetStringF(
            IDS_HISTORY_DATE_WITH_RELATIVE_TIME,
            date_str, base::TimeFormatFriendlyDate(page.visit_time()));
      }
      page_value->SetString(L"dateRelativeDay", date_str);
      page_value->SetString(L"dateTimeOfDay", 
          base::TimeFormatTimeOfDay(page.visit_time()));
    } else {
      page_value->SetString(L"dateShort", 
          base::TimeFormatShortDate(page.visit_time()));
      page_value->SetString(L"snippet", page.snippet().text());
    }

    results_value.Append(page_value);
  }

  dom_ui_->CallJavascriptFunction(L"historyResult", 
      StringValue(search_text_), results_value);
}

void BrowsingHistoryHandler::ExtractGetHistoryArguments(const Value* value, 
    int* month, std::wstring* query) {
  *month = 0;

  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;

    // Get search string.
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
      string_value->GetAsString(query);
    }

    // Get search month.
    if (list_value->Get(1, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
      std::wstring wstring_value;
      string_value->GetAsString(&wstring_value);
      *month = _wtoi(wstring_value.c_str());
    }
  }
}

history::QueryOptions BrowsingHistoryHandler::CreateQueryOptions(int month, 
    const std::wstring& query) {
  history::QueryOptions options;

  // Configure the begin point of the search to the start of the
  // current month.
  Time::Exploded exploded;
  Time::Now().LocalMidnight().LocalExplode(&exploded);
  exploded.day_of_month = 1;

  if (month == 0) {
    options.begin_time = Time::FromLocalExploded(exploded);

    // Set the end time of this first search to null (which will
    // show results from the future, should the user's clock have
    // been set incorrectly).
    options.end_time = Time();
  } else {
    // Set the end-time of this search to the end of the month that is
    // |depth| months before the search end point. The end time is not
    // inclusive, so we should feel free to set it to midnight on the
    // first day of the following month.
    exploded.month -= month - 1;
    while (exploded.month < 1) {
      exploded.month += 12;
      exploded.year--;
    }
    options.end_time = Time::FromLocalExploded(exploded);

    // Set the begin-time of the search to the start of the month
    // that is |depth| months prior to search_start_.
    if (exploded.month > 1) {
      exploded.month--;
    } else {
      exploded.month = 12;
      exploded.year--;
    }
    options.begin_time = Time::FromLocalExploded(exploded);
  }

  // If searching, only show the most recent entry and limit the number of
  // results returned.
  if (!query.empty()) {
    options.max_count = kMaxSearchResults;
    options.most_recent_visit_only = true;
  }

  return options;
}

void BrowsingHistoryHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type != NOTIFY_HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  // Some URLs were deleted from history.  Reload the most visited list.
  HandleGetHistory(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// HistoryUIContents
//
////////////////////////////////////////////////////////////////////////////////

HistoryUI::HistoryUI(DOMUIContents* contents) : DOMUI(contents) {
}

void HistoryUI::Init() {
  AddMessageHandler(new BrowsingHistoryHandler(this));

  HistoryUIHTMLSource* html_source = new HistoryUIHTMLSource();

  // Set up the chrome://history/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource,
          html_source));
}

// static
GURL HistoryUI::GetBaseURL() {
  std::string url = DOMUIContents::GetScheme();
  url += "://";
  url += kHistoryHost;
  return GURL(url);
}
