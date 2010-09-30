// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_
#define WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

class UpdateJobInfo;
class HostNotifier;

// Application cache Update algorithm and state.
class AppCacheUpdateJob : public URLRequest::Delegate,
                          public AppCacheStorage::Delegate,
                          public AppCacheHost::Observer {
 public:
  AppCacheUpdateJob(AppCacheService* service, AppCacheGroup* group);
  ~AppCacheUpdateJob();

  // Triggers the update process or adds more info if this update is already
  // in progress.
  void StartUpdate(AppCacheHost* host, const GURL& new_master_resource);

 private:
  friend class ScopedRunnableMethodFactory<AppCacheUpdateJob>;
  friend class AppCacheUpdateJobTest;
  friend class UpdateJobInfo;

  // Master entries have multiple hosts, for example, the same page is opened
  // in different tabs.
  typedef std::vector<AppCacheHost*> PendingHosts;
  typedef std::map<GURL, PendingHosts> PendingMasters;
  typedef std::map<GURL, URLRequest*> PendingUrlFetches;
  typedef std::map<int64, GURL> LoadingResponses;

  static const int kRerunDelayMs = 1000;

  // TODO(michaeln): Rework the set of states vs update types vs stored states.
  // The NO_UPDATE state is really more of an update type. For all update types
  // storing the results is relevant.

  enum UpdateType {
    UNKNOWN_TYPE,
    UPGRADE_ATTEMPT,
    CACHE_ATTEMPT,
  };

  enum InternalUpdateState {
    FETCH_MANIFEST,
    NO_UPDATE,
    DOWNLOADING,

    // Every state after this comment indicates the update is terminating.
    REFETCH_MANIFEST,
    CACHE_FAILURE,
    CANCELLED,
    COMPLETED,
  };

  enum StoredState {
    UNSTORED,
    STORING,
    STORED,
  };

  struct UrlToFetch {
    UrlToFetch(const GURL& url, bool checked, AppCacheResponseInfo* info);
    ~UrlToFetch();

    GURL url;
    bool storage_checked;
    scoped_refptr<AppCacheResponseInfo> existing_response_info;
  };

  UpdateJobInfo* GetUpdateJobInfo(URLRequest* request);

  // Methods for URLRequest::Delegate.
  void OnResponseStarted(URLRequest* request);
  void OnReadCompleted(URLRequest* request, int bytes_read);
  void OnReceivedRedirect(URLRequest* request,
                          const GURL& new_url,
                          bool* defer_redirect);
  // TODO(jennb): any other delegate callbacks to handle? certificate?

  // Methods for AppCacheStorage::Delegate.
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64 response_id);
  void OnGroupAndNewestCacheStored(AppCacheGroup* group, AppCache* newest_cache,
                                   bool success, bool would_exceed_quota);
  void OnGroupMadeObsolete(AppCacheGroup* group, bool success);

  // Methods for AppCacheHost::Observer.
  void OnCacheSelectionComplete(AppCacheHost* host) {}  // N/A
  void OnDestructionImminent(AppCacheHost* host);

  void CheckPolicy();
  void OnPolicyCheckComplete(int rv);

  void HandleCacheFailure(const std::string& error_message);

  void FetchManifest(bool is_first_fetch);

  // Add extra conditional HTTP headers to the request based on the
  // currently cached response headers.
  void AddConditionalHeaders(URLRequest* request,
                             const net::HttpResponseInfo* info);

  void OnResponseCompleted(URLRequest* request);

  // Retries a 503 request with retry-after header of 0.
  // Returns true if request should be retried and deletes original request.
  bool RetryRequest(URLRequest* request);

  void ReadResponseData(URLRequest* request);

  // Returns false if response data is processed asynchronously, in which
  // case ReadResponseData will be invoked when it is safe to continue
  // reading more response data from the request.
  bool ConsumeResponseData(URLRequest* request,
                           UpdateJobInfo* info,
                           int bytes_read);
  void OnWriteResponseComplete(int result, URLRequest* request,
                               UpdateJobInfo* info);

  void HandleManifestFetchCompleted(URLRequest* request);
  void ContinueHandleManifestFetchCompleted(bool changed);

  void HandleUrlFetchCompleted(URLRequest* request);
  void HandleMasterEntryFetchCompleted(URLRequest* request);

  void HandleManifestRefetchCompleted(URLRequest* request);
  void OnManifestInfoWriteComplete(int result);
  void OnManifestDataWriteComplete(int result);

  void StoreGroupAndCache();

  void NotifySingleHost(AppCacheHost* host, EventID event_id);
  void NotifyAllAssociatedHosts(EventID event_id);
  void NotifyAllProgress(const GURL& url);
  void NotifyAllFinalProgress();
  void NotifyAllError(const std::string& error_message);
  void AddAllAssociatedHostsToNotifier(HostNotifier* notifier);

  // Checks if manifest is byte for byte identical with the manifest
  // in the newest application cache.
  void CheckIfManifestChanged();
  void OnManifestDataReadComplete(int result);

  // Creates the list of files that may need to be fetched and initiates
  // fetches. Section 6.9.4 steps 12-17
  void BuildUrlFileList(const Manifest& manifest);
  void AddUrlToFileList(const GURL& url, int type);
  void FetchUrls();
  void CancelAllUrlFetches();
  bool ShouldSkipUrlFetch(const AppCacheEntry& entry);

  // If entry already exists in the cache currently being updated, merge
  // the entry type information with the existing entry.
  // Returns true if entry exists in cache currently being updated.
  bool AlreadyFetchedEntry(const GURL& url, int entry_type);

  // TODO(jennb): Delete when update no longer fetches master entries directly.
  // Creates the list of master entries that need to be fetched and initiates
  // fetches.
  void AddMasterEntryToFetchList(AppCacheHost* host, const GURL& url,
                                 bool is_new);
  void FetchMasterEntries();
  void CancelAllMasterEntryFetches(const std::string& error_message);

  // Asynchronously loads the entry from the newest complete cache if the
  // HTTP caching semantics allow.
  // Returns false if immediately obvious that data cannot be loaded from
  // newest complete cache.
  bool MaybeLoadFromNewestCache(const GURL& url, AppCacheEntry& entry);
  void LoadFromNewestCacheFailed(const GURL& url,
                                 AppCacheResponseInfo* newest_response_info);

  // Does nothing if update process is still waiting for pending master
  // entries or URL fetches to complete downloading. Otherwise, completes
  // the update process.
  void MaybeCompleteUpdate();

  // Schedules a rerun of the entire update with the same parameters as
  // this update job after a short delay.
  void ScheduleUpdateRetry(int delay_ms);

  void Cancel();
  void ClearPendingMasterEntries();
  void DiscardInprogressCache();
  void DiscardDuplicateResponses();

  // Deletes this object after letting the stack unwind.
  void DeleteSoon();

  bool IsTerminating() { return internal_state_ >= REFETCH_MANIFEST ||
                                stored_state_ != UNSTORED; }

  // This factory will be used to schedule invocations of various methods.
  ScopedRunnableMethodFactory<AppCacheUpdateJob> method_factory_;

  GURL manifest_url_;  // here for easier access
  AppCacheService* service_;

  scoped_refptr<AppCache> inprogress_cache_;

  AppCacheGroup* group_;

  UpdateType update_type_;
  InternalUpdateState internal_state_;

  PendingMasters pending_master_entries_;
  size_t master_entries_completed_;

  // TODO(jennb): Delete when update no longer fetches master entries directly.
  // Helper containers to track which pending master entries have yet to be
  // fetched and which are currently being fetched. Master entries that
  // are listed in the manifest may be fetched as a regular URL instead of
  // as a separate master entry fetch to optimize against duplicate fetches.
  std::set<GURL> master_entries_to_fetch_;
  PendingUrlFetches master_entry_fetches_;

  // URLs of files to fetch along with their flags.
  AppCache::EntryMap url_file_list_;
  size_t url_fetches_completed_;

  // Helper container to track which urls have not been fetched yet. URLs are
  // removed when the fetch is initiated. Flag indicates whether an attempt
  // to load the URL from storage has already been tried and failed.
  std::deque<UrlToFetch> urls_to_fetch_;

  // Helper container to track which urls are being loaded from response
  // storage.
  LoadingResponses loading_responses_;

  // Keep track of pending URL requests so we can cancel them if necessary.
  URLRequest* manifest_url_request_;
  PendingUrlFetches pending_url_fetches_;

  // Temporary storage of manifest response data for parsing and comparison.
  std::string manifest_data_;
  std::string manifest_refetch_data_;
  scoped_ptr<net::HttpResponseInfo> manifest_response_info_;
  scoped_ptr<AppCacheResponseWriter> manifest_response_writer_;
  scoped_refptr<net::IOBuffer> read_manifest_buffer_;
  std::string loaded_manifest_data_;
  scoped_ptr<AppCacheResponseReader> manifest_response_reader_;

  // New master entries added to the cache by this job, used to cleanup
  // in error conditions.
  std::vector<GURL> added_master_entries_;

  // Response ids stored by this update job, used to cleanup in
  // error conditions.
  std::vector<int64> stored_response_ids_;

  // In some cases we fetch the same resource multiple times, and then
  // have to delete the duplicates upon successful update. These ids
  // are also in the stored_response_ids_ collection so we only schedule
  // these for deletion on success.
  // TODO(michaeln): Rework when we no longer fetches master entries directly.
  std::vector<int64> duplicate_response_ids_;

  // Whether we've stored the resulting group/cache yet.
  StoredState stored_state_;

  net::CompletionCallbackImpl<AppCacheUpdateJob> manifest_info_write_callback_;
  net::CompletionCallbackImpl<AppCacheUpdateJob> manifest_data_write_callback_;
  net::CompletionCallbackImpl<AppCacheUpdateJob> manifest_data_read_callback_;

  scoped_refptr<net::CancelableCompletionCallback<AppCacheUpdateJob> >
      policy_callback_;

  FRIEND_TEST_ALL_PREFIXES(AppCacheGroupTest, QueueUpdate);

  DISALLOW_COPY_AND_ASSIGN(AppCacheUpdateJob);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_
