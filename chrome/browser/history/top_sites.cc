// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include <algorithm>
#include <set>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/most_visited_handler.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites_backend.h"
#include "chrome/browser/history/top_sites_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "gfx/codec/jpeg_codec.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// How many top sites to store in the cache.
static const size_t kTopSitesNumber = 20;
static const size_t kTopSitesShown = 8;
static const int kDaysOfHistory = 90;
// Time from startup to first HistoryService query.
static const int64 kUpdateIntervalSecs = 15;
// Intervals between requests to HistoryService.
static const int64 kMinUpdateIntervalMinutes = 1;
static const int64 kMaxUpdateIntervalMinutes = 60;

// IDs of the sites we force into top sites.
static const int kPrepopulatePageIDs[] =
    { IDS_CHROME_WELCOME_URL, IDS_THEMES_GALLERY_URL };

// Favicons of the sites we force into top sites.
static const char kPrepopulateFaviconURLs[][54] =
    { "chrome://theme/IDR_NEWTAB_CHROME_WELCOME_PAGE_FAVICON",
      "chrome://theme/IDR_NEWTAB_THEMES_GALLERY_FAVICON" };

static const int kPrepopulateTitleIDs[] =
    { IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE,
      IDS_NEW_TAB_THEMES_GALLERY_PAGE_TITLE };

namespace {

// HistoryDBTask used during migration of thumbnails from history to top sites.
// When run on the history thread it collects the top sites and the
// corresponding thumbnails. When run back on the ui thread it calls into
// TopSites::FinishHistoryMigration.
class LoadThumbnailsFromHistoryTask : public HistoryDBTask {
 public:
  LoadThumbnailsFromHistoryTask(TopSites* top_sites,
                                int result_count)
      : top_sites_(top_sites),
        result_count_(result_count) {
    // l10n_util isn't thread safe, so cache for use on the db thread.
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_CHROME_WELCOME_URL));
    ignore_urls_.insert(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL));
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    // Get the most visited urls.
    backend->QueryMostVisitedURLsImpl(result_count_,
                                      kDaysOfHistory,
                                      &data_.most_visited);

    // And fetch the thumbnails.
    for (size_t i = 0; i < data_.most_visited.size(); ++i) {
      const GURL& url = data_.most_visited[i].url;
      if (ShouldFetchThumbnailFor(url)) {
        scoped_refptr<RefCountedBytes> data;
        backend->GetPageThumbnailDirectly(url, &data);
        data_.url_to_thumbnail_map[url] = data;
      }
    }
    return true;
  }

  virtual void DoneRunOnMainThread() {
    top_sites_->FinishHistoryMigration(data_);
  }

 private:
  bool ShouldFetchThumbnailFor(const GURL& url) {
    return ignore_urls_.find(url.spec()) == ignore_urls_.end();
  }

  // Set of URLs we don't load thumbnails for. This is created on the UI thread
  // and used on the history thread.
  std::set<std::string> ignore_urls_;

  scoped_refptr<TopSites> top_sites_;

  // Number of results to request from history.
  const int result_count_;

  ThumbnailMigration data_;

  DISALLOW_COPY_AND_ASSIGN(LoadThumbnailsFromHistoryTask);
};

}  // namespace

TopSites::TopSites(Profile* profile)
    : backend_(new TopSitesBackend()),
      cache_(new TopSitesCache()),
      thread_safe_cache_(new TopSitesCache()),
      profile_(profile),
      last_num_urls_changed_(0),
      blacklist_(NULL),
      pinned_urls_(NULL),
      state_(WAITING_FOR_HISTORY_TO_LOAD) {
  if (!profile_)
    return;

  if (NotificationService::current()) {
    registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                   Source<Profile>(profile_));
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }

  blacklist_ = profile_->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);
  pinned_urls_ = profile_->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedPinnedURLs);
}

// static
bool TopSites::IsEnabled() {
  std::string switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnableTopSites);
  return switch_value.empty() || switch_value == "true";
}

void TopSites::Init(const FilePath& db_name) {
  backend_->Init(db_name);
  // Wait for history to finish so that we know if we need to migrate or can
  // read directly from top sites db.
}

bool TopSites::SetPageThumbnail(const GURL& url,
                                const SkBitmap& thumbnail,
                                const ThumbnailScore& score) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ != LOADED)
    return false;  // Ignore thumbnails until we're loaded.

  bool add_temp_thumbnail = false;
  if (!cache_->IsKnownURL(url)) {
    if (cache_->top_sites().size() < kTopSitesNumber) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!HistoryService::CanAddURL(url))
    return false;  // It's not a real webpage.

  scoped_refptr<RefCountedBytes> thumbnail_data;
  if (!EncodeBitmap(thumbnail, &thumbnail_data))
    return false;

  if (add_temp_thumbnail) {
    AddTemporaryThumbnail(url, thumbnail_data, score);
    return true;
  }

  return SetPageThumbnailEncoded(url, thumbnail_data, score);
}

void TopSites::GetMostVisitedURLs(CancelableRequestConsumer* consumer,
                                  GetTopSitesCallback* callback) {
  // WARNING: this may be invoked on any thread.
  scoped_refptr<CancelableRequest<GetTopSitesCallback> > request(
      new CancelableRequest<GetTopSitesCallback>(callback));
  // This ensures cancelation of requests when either the consumer or the
  // provider is deleted. Deletion of requests is also guaranteed.
  AddRequest(request, consumer);
  MostVisitedURLList filtered_urls;
  {
    AutoLock lock(lock_);
    if (state_ != LOADED) {
      // A request came in before we finished loading. Put the request in
      // pending_callbacks_ and we'll notify it when we finish loading.
      pending_callbacks_.insert(request);
      return;
    }

    filtered_urls = thread_safe_cache_->top_sites();
  }
  request->ForwardResult(GetTopSitesCallback::TupleType(filtered_urls));
}

bool TopSites::GetPageThumbnail(const GURL& url,
                                scoped_refptr<RefCountedBytes>* bytes) {
  // WARNING: this may be invoked on any thread.
  AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnail(url, bytes);
}

// Returns the index of |url| in |urls|, or -1 if not found.
static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

void TopSites::MigrateFromHistory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(state_ == WAITING_FOR_HISTORY_TO_LOAD);
  state_ = MIGRATING;
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->ScheduleDBTask(
      new LoadThumbnailsFromHistoryTask(
          this,
          num_results_to_request_from_history()),
      &cancelable_consumer_);
  MigratePinnedURLs();
}

void TopSites::FinishHistoryMigration(const ThumbnailMigration& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == MIGRATING);

  SetTopSites(data.most_visited);

  for (size_t i = 0; i < data.most_visited.size(); ++i) {
    URLToThumbnailMap::const_iterator image_i =
        data.url_to_thumbnail_map.find(data.most_visited[i].url);
    if (image_i != data.url_to_thumbnail_map.end()) {
      SetPageThumbnailEncoded(data.most_visited[i].url,
                              image_i->second,
                              ThumbnailScore());
    }
  }

  MoveStateToLoaded();

  ResetThreadSafeImageCache();

  // We've scheduled all the thumbnails and top sites to be written to the top
  // sites db, but it hasn't happened yet. Schedule a request on the db thread
  // that notifies us when done. When done we'll know everything was written and
  // we can tell history to finish its part of migration.
  backend_->DoEmptyRequest(
      &cancelable_consumer_,
      NewCallback(this, &TopSites::OnHistoryMigrationWrittenToDisk));
}

void TopSites::HistoryLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == WAITING_FOR_HISTORY_TO_LOAD) {
    state_ = READING_FROM_DB;
    backend_->GetMostVisitedThumbnails(
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnGotMostVisitedThumbnails));
  } else {
    DCHECK(state_ == MIGRATING);
  }
}

bool TopSites::HasBlacklistedItems() const {
  return !blacklist_->empty();
}

void TopSites::AddBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RemovePinnedURL(url);
  Value* dummy = Value::CreateNullValue();
  blacklist_->SetWithoutPathExpansion(GetURLHash(url), dummy);

  ResetThreadSafeCache();
}

void TopSites::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  blacklist_->RemoveWithoutPathExpansion(GetURLHash(url), NULL);
  ResetThreadSafeCache();
}

bool TopSites::IsBlacklisted(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return blacklist_->HasKey(GetURLHash(url));
}

void TopSites::ClearBlacklistedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  blacklist_->Clear();
  ResetThreadSafeCache();
}

void TopSites::AddPinnedURL(const GURL& url, size_t pinned_index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL old;
  if (GetPinnedURLAtIndex(pinned_index, &old))
    RemovePinnedURL(old);

  if (IsURLPinned(url))
    RemovePinnedURL(url);

  Value* index = Value::CreateIntegerValue(pinned_index);
  pinned_urls_->SetWithoutPathExpansion(GetURLString(url), index);

  ResetThreadSafeCache();
}

bool TopSites::IsURLPinned(const GURL& url) {
  int tmp;
  return pinned_urls_->GetIntegerWithoutPathExpansion(GetURLString(url), &tmp);
}

void TopSites::RemovePinnedURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pinned_urls_->RemoveWithoutPathExpansion(GetURLString(url), NULL);

  ResetThreadSafeCache();
}

bool TopSites::GetPinnedURLAtIndex(size_t index, GURL* url) {
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
       it != pinned_urls_->end_keys(); ++it) {
    int current_index;
    if (pinned_urls_->GetIntegerWithoutPathExpansion(*it, &current_index)) {
      if (static_cast<size_t>(current_index) == index) {
        *url = GURL(*it);
        return true;
      }
    }
  }
  return false;
}

void TopSites::Shutdown() {
  profile_ = NULL;
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_consumer_.CancelAllRequests();
  backend_->Shutdown();
}

// static
void TopSites::DiffMostVisited(const MostVisitedURLList& old_list,
                               const MostVisitedURLList& new_list,
                               TopSitesDelta* delta) {
  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  for (size_t i = 0; i < old_list.size(); i++)
    all_old_urls[old_list[i].url] = i;

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  for (size_t i = 0; i < new_list.size(); i++) {
    std::map<GURL, size_t>::iterator found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      MostVisitedURLWithRank added;
      added.url = new_list[i];
      added.rank = i;
      delta->added.push_back(added);
    } else {
      if (found->second != i) {
        MostVisitedURLWithRank moved;
        moved.url = new_list[i];
        moved.rank = i;
        delta->moved.push_back(moved);
      }
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      delta->deleted.push_back(old_list[i->second]);
  }
}

TopSites::~TopSites() {
}

bool TopSites::SetPageThumbnailNoDB(const GURL& url,
                                    const RefCountedBytes* thumbnail_data,
                                    const ThumbnailScore& score) {
  // This should only be invoked when we know about the url.
  DCHECK(cache_->IsKnownURL(url));

  const MostVisitedURL& most_visited =
      cache_->top_sites()[cache_->GetURLIndex(url)];
  Images* image = cache_->GetImage(url);

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is because the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image->thumbnail_score,
                                  new_score_with_redirects) &&
      image->thumbnail.get())
    return false;  // The one we already have is better.

  image->thumbnail = const_cast<RefCountedBytes*>(thumbnail_data);
  image->thumbnail_score = new_score_with_redirects;

  ResetThreadSafeImageCache();
  return true;
}

bool TopSites::SetPageThumbnailEncoded(const GURL& url,
                                       const RefCountedBytes* thumbnail,
                                       const ThumbnailScore& score) {
  if (!SetPageThumbnailNoDB(url, thumbnail, score))
    return false;

  // Update the database.
  if (!cache_->IsKnownURL(url))
    return false;

  size_t index = cache_->GetURLIndex(url);
  const MostVisitedURL& most_visited = cache_->top_sites()[index];
  backend_->SetPageThumbnail(most_visited,
                             index,
                             *(cache_->GetImage(most_visited.url)));
  return true;
}

// static
bool TopSites::EncodeBitmap(const SkBitmap& bitmap,
                            scoped_refptr<RefCountedBytes>* bytes) {
  *bytes = new RefCountedBytes();
  SkAutoLockPixels bitmap_lock(bitmap);
  return gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, bitmap.width(),
      bitmap.height(),
      static_cast<int>(bitmap.rowBytes()), 90,
      &((*bytes)->data));
}

void TopSites::AddTemporaryThumbnail(const GURL& url,
                                     const RefCountedBytes* thumbnail,
                                     const ThumbnailScore& score) {
  Images& img = temp_thumbnails_map_[url];
  img.thumbnail = const_cast<RefCountedBytes*>(thumbnail);
  img.thumbnail_score = score;
}

void TopSites::StartQueryForMostVisited() {
  if (!profile_)
    return;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (hs) {
    hs->QueryMostVisitedURLs(
        num_results_to_request_from_history(),
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailableFromHistory));
  }
}

// static
int TopSites::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                        const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

void TopSites::OnMigrationDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!profile_)
    return;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |hs| may be null during unit tests.
  if (!hs)
    return;
  hs->OnTopSitesReady();
}

// static
MostVisitedURLList TopSites::GetPrepopulatePages() {
  MostVisitedURLList urls;
  urls.resize(arraysize(kPrepopulatePageIDs));
  for (size_t i = 0; i < arraysize(kPrepopulatePageIDs); ++i) {
    MostVisitedURL& url = urls[i];
    url.url = GURL(l10n_util::GetStringUTF8(kPrepopulatePageIDs[i]));
    url.redirects.push_back(url.url);
    url.favicon_url = GURL(kPrepopulateFaviconURLs[i]);
    url.title = l10n_util::GetStringUTF16(kPrepopulateTitleIDs[i]);
  }
  return urls;
}

// static
bool TopSites::AddPrepopulatedPages(MostVisitedURLList* urls) {
  bool added = false;
  MostVisitedURLList prepopulate_urls = GetPrepopulatePages();
  for (size_t i = 0; i < prepopulate_urls.size(); ++i) {
    if (urls->size() < kTopSitesNumber &&
        IndexOf(*urls, prepopulate_urls[i].url) == -1) {
      urls->push_back(prepopulate_urls[i]);
      added = true;
    }
  }
  return added;
}

void TopSites::MigratePinnedURLs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<GURL, size_t> tmp_map;
  for (DictionaryValue::key_iterator it = pinned_urls_->begin_keys();
       it != pinned_urls_->end_keys(); ++it) {
    Value* value;
    if (!pinned_urls_->GetWithoutPathExpansion(*it, &value))
      continue;

    if (value->IsType(DictionaryValue::TYPE_DICTIONARY)) {
      DictionaryValue* dict = static_cast<DictionaryValue*>(value);
      std::string url_string;
      int index;
      if (dict->GetString("url", &url_string) &&
          dict->GetInteger("index", &index))
        tmp_map[GURL(url_string)] = index;
    }
  }
  pinned_urls_->Clear();
  for (std::map<GURL, size_t>::iterator it = tmp_map.begin();
       it != tmp_map.end(); ++it)
    AddPinnedURL(it->first, it->second);
}

void TopSites::ApplyBlacklistAndPinnedURLs(const MostVisitedURLList& urls,
                                           MostVisitedURLList* out) {
  MostVisitedURLList urls_copy;
  for (size_t i = 0; i < urls.size(); i++) {
    if (!IsBlacklisted(urls[i].url))
      urls_copy.push_back(urls[i]);
  }

  for (size_t pinned_index = 0; pinned_index < kTopSitesShown; pinned_index++) {
    GURL url;
    bool found = GetPinnedURLAtIndex(pinned_index, &url);
    if (!found)
      continue;

    DCHECK(!url.is_empty());
    int cur_index = IndexOf(urls_copy, url);
    MostVisitedURL tmp;
    if (cur_index < 0) {
      // Pinned URL not in urls.
      tmp.url = url;
    } else {
      tmp = urls_copy[cur_index];
      urls_copy.erase(urls_copy.begin() + cur_index);
    }
    if (pinned_index > out->size())
      out->resize(pinned_index);  // Add empty URLs as fillers.
    out->insert(out->begin() + pinned_index, tmp);
  }

  // Add non-pinned URLs in the empty spots.
  size_t current_url = 0;  // Index into the remaining URLs in urls_copy.
  for (size_t i = 0; i < kTopSitesShown && current_url < urls_copy.size();
       i++) {
    if (i == out->size()) {
      out->push_back(urls_copy[current_url]);
      current_url++;
    } else if (i < out->size()) {
      if ((*out)[i].url.is_empty()) {
        // Replace the filler
        (*out)[i] = urls_copy[current_url];
        current_url++;
      }
    } else {
      NOTREACHED();
    }
  }
}

std::string TopSites::GetURLString(const GURL& url) {
  return cache_->GetCanonicalURL(url).spec();
}

std::string TopSites::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return MD5String(url.spec());
}

base::TimeDelta TopSites::GetUpdateDelay() {
  if (cache_->top_sites().size() <= arraysize(kPrepopulateTitleIDs))
    return base::TimeDelta::FromSeconds(30);

  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / cache_->top_sites().size();
  return base::TimeDelta::FromMinutes(minutes);
}

// static
void TopSites::ProcessPendingCallbacks(PendingCallbackSet pending_callbacks,
                                       const MostVisitedURLList& urls) {
  PendingCallbackSet::iterator i;
  for (i = pending_callbacks.begin();
       i != pending_callbacks.end(); ++i) {
    scoped_refptr<CancelableRequest<GetTopSitesCallback> > request = *i;
    if (!request->canceled())
      request->ForwardResult(GetTopSitesCallback::TupleType(urls));
  }
  pending_callbacks.clear();
}

void TopSites::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (state_ != LOADED)
    return;

  if (type == NotificationType::HISTORY_URLS_DELETED) {
    Details<history::URLsDeletedDetails> deleted_details(details);
    if (deleted_details->all_history) {
      SetTopSites(MostVisitedURLList());
      backend_->ResetDatabase();
    } else {
      std::set<size_t> indices_to_delete;  // Indices into top_sites_.
      for (std::set<GURL>::iterator i = deleted_details->urls.begin();
           i != deleted_details->urls.end(); ++i) {
        if (cache_->IsKnownURL(*i))
          indices_to_delete.insert(cache_->GetURLIndex(*i));
      }

      if (indices_to_delete.empty())
        return;

      MostVisitedURLList new_top_sites(cache_->top_sites());
      for (std::set<size_t>::reverse_iterator i = indices_to_delete.rbegin();
           i != indices_to_delete.rend(); i++) {
        size_t index = *i;
        RemovePinnedURL(new_top_sites[index].url);
        new_top_sites.erase(new_top_sites.begin() + index);
      }
      SetTopSites(new_top_sites);
    }
    StartQueryForMostVisited();
  } else if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    if (cache_->top_sites().size() < kTopSitesNumber) {
      NavigationController::LoadCommittedDetails* load_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      if (!load_details)
        return;
      const GURL& url = load_details->entry->url();
      if (!cache_->IsKnownURL(url) && HistoryService::CanAddURL(url)) {
        // Ideally we would just invoke StartQueryForMostVisited, but at the
        // time this is invoked history hasn't been updated, which means if we
        // invoked StartQueryForMostVisited now we could get stale data.
        RestartQueryForTopSitesTimer(base::TimeDelta::FromMilliseconds(1));
      }
    }
  }
}

void TopSites::SetTopSites(const MostVisitedURLList& new_top_sites) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList top_sites(new_top_sites);
  AddPrepopulatedPages(&top_sites);

  TopSitesDelta delta;
  DiffMostVisited(cache_->top_sites(), top_sites, &delta);
  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty())
    backend_->UpdateTopSites(delta);

  last_num_urls_changed_ = delta.added.size() + delta.moved.size();

  // We always do the following steps (setting top sites in cache, and resetting
  // thread safe cache ...) as this method is invoked during startup at which
  // point the caches haven't been updated yet.
  cache_->SetTopSites(top_sites);

  // See if we have any tmp thumbnails for the new sites.
  if (!temp_thumbnails_map_.empty()) {
    for (size_t i = 0; i < top_sites.size(); ++i) {
      const MostVisitedURL& mv = top_sites[i];
      GURL canonical_url = cache_->GetCanonicalURL(mv.url);
      for (std::map<GURL, Images>::iterator it = temp_thumbnails_map_.begin();
           it != temp_thumbnails_map_.end(); ++it) {
        // Must map all temp URLs to canonical ones.
        // temp_thumbnails_map_ contains non-canonical URLs, because
        // when we add a temp thumbnail, redirect chain is not known.
        // This is slow, but temp_thumbnails_map_ should have very few URLs.
        if (canonical_url == cache_->GetCanonicalURL(it->first)) {
          SetPageThumbnailEncoded(mv.url,
                                  it->second.thumbnail,
                                  it->second.thumbnail_score);
          temp_thumbnails_map_.erase(it);
          break;
        }
      }
    }
  }

  if (top_sites.size() >= kTopSitesNumber)
    temp_thumbnails_map_.clear();

  ResetThreadSafeCache();
  ResetThreadSafeImageCache();

  // Restart the timer that queries history for top sites. This is done to
  // ensure we stay in sync with history.
  RestartQueryForTopSitesTimer(GetUpdateDelay());
}

int TopSites::num_results_to_request_from_history() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return kTopSitesNumber + blacklist_->size();
}

void TopSites::MoveStateToLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MostVisitedURLList filtered_urls;
  PendingCallbackSet pending_callbacks;
  {
    AutoLock lock(lock_);

    DCHECK(state_ != LOADED);
    state_ = LOADED;

    if (!pending_callbacks_.empty()) {
      filtered_urls = thread_safe_cache_->top_sites();
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  ProcessPendingCallbacks(pending_callbacks, filtered_urls);

  NotificationService::current()->Notify(NotificationType::TOP_SITES_LOADED,
                                         Source<Profile>(profile_),
                                         Details<TopSites>(this));
}

void TopSites::ResetThreadSafeCache() {
  AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklistAndPinnedURLs(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSites::ResetThreadSafeImageCache() {
  AutoLock lock(lock_);
  thread_safe_cache_->SetThumbnails(cache_->images());
  thread_safe_cache_->RemoveUnreferencedThumbnails();
}

void TopSites::RestartQueryForTopSitesTimer(base::TimeDelta delta) {
  timer_.Stop();
  timer_.Start(delta, this, &TopSites::StartQueryForMostVisited);
}

void TopSites::OnHistoryMigrationWrittenToDisk(TopSitesBackend::Handle handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!profile_)
    return;

  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history)
    history->OnTopSitesReady();
}

void TopSites::OnGotMostVisitedThumbnails(
    CancelableRequestProvider::Handle handle,
    scoped_refptr<MostVisitedThumbnails> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, READING_FROM_DB);

  // Set the top sites directly in the cache so that SetTopSites diffs
  // correctly.
  cache_->SetTopSites(data->most_visited);
  SetTopSites(data->most_visited);
  cache_->SetThumbnails(data->url_to_images_map);

  ResetThreadSafeImageCache();

  MoveStateToLoaded();

  // Start a timer that refreshes top sites from history.
  RestartQueryForTopSitesTimer(
      base::TimeDelta::FromSeconds(kUpdateIntervalSecs));
}

void TopSites::OnTopSitesAvailableFromHistory(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  SetTopSites(pages);
}

}  // namespace history
