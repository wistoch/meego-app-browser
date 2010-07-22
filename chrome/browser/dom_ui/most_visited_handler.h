// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_MOST_VISITED_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_MOST_VISITED_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class ListValue;
class PageUsageData;
class PrefService;
class Value;

// The handler for Javascript messages related to the "most visited" view.
class MostVisitedHandler : public DOMMessageHandler,
                           public NotificationObserver {
 public:
  // This struct is used when getting the pre-populated pages in case the user
  // hasn't filled up his most visited pages.
  struct MostVisitedPage {
    std::wstring title;
    GURL url;
    GURL thumbnail_url;
    GURL favicon_url;
  };

  MostVisitedHandler();
  virtual ~MostVisitedHandler() { }

  // DOMMessageHandler override and implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

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
  // Send a request to the HistoryService to get the most visited pages.
  void StartQueryForMostVisited();

  // Callback from the history system when the most visited list is available.
  void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* data);

  // Sets pages_value_ form a vector of URLs.
  void SetPagesValue(std::vector<PageUsageData*>* data);

  // Callback for TopSites.
  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data);

  // Puts the passed URL in the blacklist (so it does not show as a thumbnail).
  void BlacklistURL(const GURL& url);

  // Returns the key used in url_blacklist_ and pinned_urls_ for the passed
  // |url|.
  std::wstring GetDictionaryKeyForURL(const std::string& url);

  // Gets the page data for a pinned URL at a given index. This returns
  // true if found.
  bool GetPinnedURLAtIndex(int index, MostVisitedPage* page);

  void AddPinnedURL(const MostVisitedPage& page, int index);
  void RemovePinnedURL(const GURL& url);

  // Sends pages_value_ to the javascript side to and resets page_value_.
  void SendPagesValue();

  // Returns true if we should treat this as the first run of the new tab page.
  bool IsFirstRun();

  static const std::vector<MostVisitedPage>& GetPrePopulatedPages();

  static MostVisitedPage GetChromeStorePage();

  // Whether we have any apps installed.
  bool HasApps() const;

  NotificationRegistrar registrar_;

  // Our consumer for the history service.
  CancelableRequestConsumerTSimple<PageUsageData*> cancelable_consumer_;
  CancelableRequestConsumer topsites_consumer_;

  // The most visited URLs, in priority order.
  // Only used for matching up clicks on the page to which most visited entry
  // was clicked on for metrics purposes.
  std::vector<GURL> most_visited_urls_;

  // The URL blacklist: URLs we do not want to show in the thumbnails list.  It
  // is a dictionary for quick access (it associates a dummy boolean to the URL
  // string).  This is owned by the PrefService.
  DictionaryValue* url_blacklist_;

  // This is a dictionary for the pinned URLs for the the most visited part of
  // the new tab page. The key of the dictionary is a hash of the URL and the
  // value is a dictionary with title, url and index.  This is owned by the
  // PrefService.
  DictionaryValue* pinned_urls_;

  // We pre-fetch the first set of result pages.  This variable is false until
  // we get the first getMostVisited() call.
  bool got_first_most_visited_request_;

  // Keep the results of the db query here.
  scoped_ptr<ListValue> pages_value_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_MOST_VISITED_HANDLER_H_
