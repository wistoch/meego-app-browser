// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/page_usage_data.h"
#include "gfx/codec/jpeg_codec.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// How many top sites to store in the cache.
static const int kTopSitesNumber = 20;
static const int kDaysOfHistory = 90;
static const int64 kUpdateIntervalSecs = 15;  // Time from startup to DB query.
// Intervals between requests to HistoryService.
static const int64 kMinUpdateIntervalMinutes = 1;
static const int64 kMaxUpdateIntervalMinutes = 60;


TopSites::TopSites(Profile* profile) : profile_(profile),
                                       mock_history_service_(NULL),
                                       last_num_urls_changed_(0) {
  registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                 Source<Profile>(profile_));
}

TopSites::~TopSites() {
  timer_.Stop();
}

void TopSites::Init(const FilePath& db_name) {
  db_path_ = db_name;
  db_.reset(new TopSitesDatabaseImpl());
  if (!db_->Init(db_name)) {
    NOTREACHED() << "Failed to initialize database.";
    return;
  }

  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::ReadDatabase));

  // Start the one-shot timer.
  timer_.Start(base::TimeDelta::FromSeconds(kUpdateIntervalSecs), this,
               &TopSites::StartQueryForMostVisited);
}

void TopSites::ReadDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  std::map<GURL, Images> thumbnails;

  DCHECK(db_.get());
  {
    AutoLock lock(lock_);
    MostVisitedURLList top_urls;
    db_->GetPageThumbnails(&top_urls, &thumbnails);
    StoreMostVisited(&top_urls);
  }  // Lock is released here.

  for (size_t i = 0; i < top_sites_.size(); i++) {
    MostVisitedURL url = top_sites_[i];
    Images thumbnail = thumbnails[url.url];
    SetPageThumbnailNoDB(url.url, thumbnail.thumbnail,
                         thumbnail.thumbnail_score);
  }
}

// Public function that encodes the bitmap into RefCountedBytes and
// updates the database.
bool TopSites::SetPageThumbnail(const GURL& url,
                                const SkBitmap& thumbnail,
                                const ThumbnailScore& score) {
  scoped_refptr<RefCountedBytes> thumbnail_data = new RefCountedBytes;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &thumbnail_data->data);
  if (!encoded)
    return false;
  if (!SetPageThumbnailNoDB(url, thumbnail_data, score))
    return false;

  // Update the database.
  if (!db_.get())
    return true;
  std::map<GURL, size_t>::iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return false;
  size_t index = found->second;

  MostVisitedURL& most_visited = top_sites_[index];
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::WriteThumbnailToDB,
      most_visited, index, top_images_[most_visited.url]));
  return true;
}

void TopSites::WriteThumbnailToDB(const MostVisitedURL& url,
                                  int url_rank,
                                  const TopSites::Images& thumbnail) {
  DCHECK(db_.get());
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  db_->SetPageThumbnail(url, url_rank, thumbnail);
}

// private
bool TopSites::SetPageThumbnailNoDB(const GURL& url,
                                    const RefCountedBytes* thumbnail_data,
                                    const ThumbnailScore& score) {
  AutoLock lock(lock_);

  std::map<GURL, size_t>::iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return false;  // This URL is not known to us.
  MostVisitedURL& most_visited = top_sites_[found->second];
  Images& image = top_images_[most_visited.url];

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is because the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image.thumbnail_score,
                                  new_score_with_redirects) &&
      image.thumbnail.get())
    return false;  // The one we already have is better.

  // Take ownership of the thumbnail data.
  image.thumbnail = const_cast<RefCountedBytes*>(thumbnail_data);
  image.thumbnail_score = new_score_with_redirects;

  return true;
}

MostVisitedURLList TopSites::GetMostVisitedURLs() {
  AutoLock lock(lock_);
  return top_sites_;
}

bool TopSites::GetPageThumbnail(const GURL& url, RefCountedBytes** data) const {
  std::map<GURL, Images>::const_iterator found = top_images_.find(url);
  if (found == top_images_.end())
    return false;  // No thumbnail for this URL.

  Images image = found->second;
  *data = image.thumbnail.get();
  return true;
}

void TopSites::UpdateMostVisited(MostVisitedURLList most_visited) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // TODO(brettw) filter for blacklist!

  if (!top_sites_.empty()) {
    std::vector<size_t> added;    // Indices into most_visited.
    std::vector<size_t> deleted;  // Indices into top_sites_.
    std::vector<size_t> moved;    // Indices into most_visited.
    DiffMostVisited(top_sites_, most_visited, &added, &deleted, &moved);

    // #added == #deleted; #added + #moved = total.
    last_num_urls_changed_ = added.size() + moved.size();

    // Process the diff: delete from images and disk, add to disk.
    // Delete all the thumbnails associated with URLs that were deleted.
    for (size_t i = 0; i < deleted.size(); i++) {
      MostVisitedURL deleted_url = top_sites_[deleted[i]];
      std::map<GURL, Images>::iterator found =
          top_images_.find(deleted_url.url);
      if (found != top_images_.end())
        top_images_.erase(found);

      // Delete from disk.
      if (db_.get())
        db_->RemoveURL(deleted_url);
    }

    if (db_.get()) {
      // Write both added and moved urls.
      for (size_t i = 0; i < added.size(); i++) {
        MostVisitedURL added_url = most_visited[added[i]];
        db_->SetPageThumbnail(added_url, added[i], Images());
      }
      for (size_t i = 0; i < moved.size(); i++) {
        MostVisitedURL moved_url = most_visited[moved[i]];
        db_->SetPageThumbnail(moved_url, moved[i], Images());
      }
    }
  }
  AutoLock lock(lock_);
  StoreMostVisited(&most_visited);
}

void TopSites::StoreMostVisited(MostVisitedURLList* most_visited) {
  lock_.AssertAcquired();
  // Take ownership of the most visited data.
  top_sites_.clear();
  top_sites_.swap(*most_visited);

  // Save the redirect information for quickly mapping to the canonical URLs.
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++)
    StoreRedirectChain(top_sites_[i].redirects, i);
}

void TopSites::StoreRedirectChain(const RedirectList& redirects,
                                  size_t destination) {
  lock_.AssertAcquired();

  if (redirects.empty()) {
    NOTREACHED();
    return;
  }

  // We shouldn't get any duplicates.
  DCHECK(canonical_urls_.find(redirects[0]) == canonical_urls_.end());

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++)
    canonical_urls_[redirects[i]] = destination;
}

GURL TopSites::GetCanonicalURL(const GURL& url) const {
  lock_.AssertAcquired();

  std::map<GURL, size_t>::const_iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return GURL();  // Don't know anything about this URL.
  return top_sites_[found->second].url;
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

// static
void TopSites::DiffMostVisited(const MostVisitedURLList& old_list,
                               const MostVisitedURLList& new_list,
                               std::vector<size_t>* added_urls,
                               std::vector<size_t>* deleted_urls,
                               std::vector<size_t>* moved_urls) {
  added_urls->clear();
  deleted_urls->clear();
  moved_urls->clear();

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
      added_urls->push_back(i);
    } else {
      if (found->second != i)
        moved_urls->push_back(i);
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      deleted_urls->push_back(i->second);
  }
}

void TopSites::StartQueryForMostVisited() {
  if (mock_history_service_) {
    // Testing with a mockup.
    // QueryMostVisitedURLs is not virtual, so we have to duplicate the code.
    mock_history_service_->QueryMostVisitedURLs(
        kTopSitesNumber,
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailable));
  } else {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    // |hs| may be null during unit tests.
    if (hs) {
      hs->QueryMostVisitedURLs(
          kTopSitesNumber,
          kDaysOfHistory,
          &cancelable_consumer_,
          NewCallback(this, &TopSites::OnTopSitesAvailable));
    } else {
      LOG(INFO) << "History Service not available.";
    }
  }

  timer_.Stop();
  timer_.Start(GetUpdateDelay(), this,
               &TopSites::StartQueryForMostVisited);
}

base::TimeDelta TopSites::GetUpdateDelay() {
  int64 range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64 minutes = kMaxUpdateIntervalMinutes -
      last_num_urls_changed_ * range / kTopSitesNumber;
  return base::TimeDelta::FromMinutes(minutes);
}

void TopSites::OnTopSitesAvailable(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE, NewRunnableMethod(
      this, &TopSites::UpdateMostVisited, pages));
}

void TopSites::SetMockHistoryService(MockHistoryService* mhs) {
  mock_history_service_ = mhs;
}

void TopSites::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (type != NotificationType::HISTORY_URLS_DELETED) {
    NOTREACHED();
    return;
  }

  Details<history::URLsDeletedDetails> deleted_details(details);
  if (deleted_details->all_history) {
    ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                           NewRunnableMethod(this, &TopSites::ResetDatabase));
  }
  StartQueryForMostVisited();
}

void TopSites::ResetDatabase() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  db_.reset(new TopSitesDatabaseImpl());
  file_util::Delete(db_path_, false);
  if (!db_->Init(db_path_)) {
    NOTREACHED() << "Failed to initialize database.";
    return;
  }
}

}  // namespace history
