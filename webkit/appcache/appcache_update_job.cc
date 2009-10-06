// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_update_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"

namespace appcache {

static const int kBufferSize = 4096;
static const size_t kMaxConcurrentUrlFetches = 2;

// Extra info associated with requests for use during response processing.
// This info is deleted when the URLRequest is deleted.
struct UpdateJobInfo :  public URLRequest::UserData {
  enum RequestType {
    MANIFEST_FETCH,
    URL_FETCH,
    MANIFEST_REFETCH,
  };

  explicit UpdateJobInfo(RequestType request_type)
      : type(request_type),
        buffer(new net::IOBuffer(kBufferSize)) {
  }

  RequestType type;
  scoped_refptr<net::IOBuffer> buffer;
  // TODO(jennb): need storage info to stream response data to storage
};

// Helper class for collecting hosts per frontend when sending notifications
// so that only one notification is sent for all hosts using the same frontend.
class HostNotifier {
 public:
  typedef std::vector<int> HostIds;
  typedef std::map<AppCacheFrontend*, HostIds> NotifyHostMap;

  // Caller is responsible for ensuring there will be no duplicate hosts.
  void AddHost(AppCacheHost* host) {
    std::pair<NotifyHostMap::iterator , bool> ret = hosts_to_notify.insert(
        NotifyHostMap::value_type(host->frontend(), HostIds()));
    ret.first->second.push_back(host->host_id());
  }

  void AddHosts(const std::set<AppCacheHost*>& hosts) {
    for (std::set<AppCacheHost*>::const_iterator it = hosts.begin();
         it != hosts.end(); ++it) {
      AddHost(*it);
    }
  }

  void SendNotifications(EventID event_id) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnEventRaised(it->second, event_id);
    }
  }

 private:
  NotifyHostMap hosts_to_notify;
};

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheService* service,
                                     AppCacheGroup* group)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      service_(service),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(FETCH_MANIFEST),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_url_request_(NULL) {
  DCHECK(group_);
  manifest_url_ = group_->manifest_url();
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  Cancel();

  DCHECK(!manifest_url_request_);
  DCHECK(pending_url_fetches_.empty());

  group_->SetUpdateStatus(AppCacheGroup::IDLE);
}

void AppCacheUpdateJob::StartUpdate(AppCacheHost* host,
                                    const GURL& new_master_resource) {
  DCHECK(group_->update_job() == this);

  if (!new_master_resource.is_empty()) {
    /* TODO(jennb): uncomment when processing master entries is implemented
    std::pair<PendingMasters::iterator, bool> ret =
        pending_master_entries_.insert(
            PendingMasters::value_type(new_master_resource, PendingHosts()));
    ret.first->second.push_back(host);
    */
  }

  // Notify host (if any) if already checking or downloading.
  appcache::AppCacheGroup::UpdateStatus update_status = group_->update_status();
  if (update_status == AppCacheGroup::CHECKING ||
      update_status == AppCacheGroup::DOWNLOADING) {
    if (host) {
      NotifySingleHost(host, CHECKING_EVENT);
      if (update_status == AppCacheGroup::DOWNLOADING)
        NotifySingleHost(host, DOWNLOADING_EVENT);
    }
    return;
  }

  // Begin update process for the group.
  group_->SetUpdateStatus(AppCacheGroup::CHECKING);
  if (group_->HasCache()) {
    update_type_ = UPGRADE_ATTEMPT;
    NotifyAllAssociatedHosts(CHECKING_EVENT);
  } else {
    update_type_ = CACHE_ATTEMPT;
    DCHECK(host);
    NotifySingleHost(host, CHECKING_EVENT);
  }

  FetchManifest(true);
}

void AppCacheUpdateJob::FetchManifest(bool is_first_fetch) {
  DCHECK(!manifest_url_request_);
  manifest_url_request_ = new URLRequest(manifest_url_, this);
  UpdateJobInfo::RequestType fetch_type = is_first_fetch ?
      UpdateJobInfo::MANIFEST_FETCH : UpdateJobInfo::MANIFEST_REFETCH;
  manifest_url_request_->SetUserData(this, new UpdateJobInfo(fetch_type));
  manifest_url_request_->set_context(service_->request_context());
  // TODO(jennb): add "If-Modified-Since" if have previous date
  manifest_url_request_->set_load_flags(
      manifest_url_request_->load_flags() | net::LOAD_DISABLE_INTERCEPT);
  manifest_url_request_->Start();
}

void AppCacheUpdateJob::OnResponseStarted(URLRequest *request) {
  if (request->status().is_success())
    ReadResponseData(request);
  else
    OnResponseCompleted(request);
}

void AppCacheUpdateJob::ReadResponseData(URLRequest* request) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  int bytes_read = 0;
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  request->Read(info->buffer, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void AppCacheUpdateJob::OnReadCompleted(URLRequest* request, int bytes_read) {
  bool data_consumed = true;
  if (request->status().is_success() && bytes_read > 0) {
    UpdateJobInfo* info =
        static_cast<UpdateJobInfo*>(request->GetUserData(this));

    data_consumed = ConsumeResponseData(request, info, bytes_read);
    if (data_consumed) {
      bytes_read = 0;
      while (request->Read(info->buffer, kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_consumed = ConsumeResponseData(request, info, bytes_read);
          if (!data_consumed)
            break;  // wait for async data processing, then read more
        } else {
          break;
        }
      }
    }
  }

  if (data_consumed && !request->status().is_io_pending())
    OnResponseCompleted(request);
}

bool AppCacheUpdateJob::ConsumeResponseData(URLRequest* request,
                                            UpdateJobInfo* info,
                                            int bytes_read) {
  switch (info->type) {
    case UpdateJobInfo::MANIFEST_FETCH:
      manifest_data_.append(info->buffer->data(), bytes_read);
      break;
    case UpdateJobInfo::URL_FETCH:
      // TODO(jennb): stream data to storage. will be async so need to wait
      // for callback before reading next chunk.
      // For now, schedule a task to continue reading to simulate async-ness.
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &AppCacheUpdateJob::ReadResponseData, request));
      return false;
    case UpdateJobInfo::MANIFEST_REFETCH:
      manifest_refetch_data_.append(info->buffer->data(), bytes_read);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::OnReceivedRedirect(URLRequest* request,
                                           const GURL& new_url,
                                           bool* defer_redirect) {
  // Redirect is not allowed by the update process.
  request->Cancel();
  OnResponseCompleted(request);
}

void AppCacheUpdateJob::OnResponseCompleted(URLRequest* request) {
  // TODO(jennb): think about retrying for 503s where retry-after is 0
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  switch (info->type) {
    case UpdateJobInfo::MANIFEST_FETCH:
      HandleManifestFetchCompleted(request);
      break;
    case UpdateJobInfo::URL_FETCH:
      HandleUrlFetchCompleted(request);
      break;
    case UpdateJobInfo::MANIFEST_REFETCH:
      HandleManifestRefetchCompleted(request);
      break;
    default:
      NOTREACHED();
  }

  delete request;
}

void AppCacheUpdateJob::HandleManifestFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == FETCH_MANIFEST);
  manifest_url_request_ = NULL;

  if (!request->status().is_success()) {
    LOG(ERROR) << "Request non-success, status: " << request->status().status()
        << " os_error: " << request->status().os_error();
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
    return;
  }

  int response_code = request->GetResponseCode();
  std::string mime_type;
  request->GetMimeType(&mime_type);

  if (response_code == 200 && mime_type == kManifestMimeType) {
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if (response_code == 404 || response_code == 410) {
    group_->set_obsolete(true);
    NotifyAllAssociatedHosts(OBSOLETE_EVENT);
    NotifyAllPendingMasterHosts(ERROR_EVENT);
    DeleteSoon();
  } else {
    LOG(ERROR) << "Cache failure, response code: " << response_code;
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
  }
}

void AppCacheUpdateJob::ContinueHandleManifestFetchCompleted(bool changed) {
  DCHECK(internal_state_ == FETCH_MANIFEST);

  if (!changed) {
    DCHECK(update_type_ == UPGRADE_ATTEMPT);
    internal_state_ = NO_UPDATE;
    MaybeCompleteUpdate();  // if not done, run async 6.9.4 step 7 substeps
    return;
  }

  Manifest manifest;
  if (!ParseManifest(manifest_url_, manifest_data_.data(),
                     manifest_data_.length(), manifest)) {
    LOG(ERROR) << "Failed to parse manifest: " << manifest_url_;
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
    return;
  }

  // Proceed with update process. Section 6.9.4 steps 8-20.
  internal_state_ = DOWNLOADING;
  inprogress_cache_ = new AppCache(service_, service_->NewCacheId());
  inprogress_cache_->set_owning_group(group_);
  BuildUrlFileList(manifest);
  inprogress_cache_->InitializeWithManifest(&manifest);

  // Associate all pending master hosts with the newly created cache.
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host->AssociateCache(inprogress_cache_);
    }
  }

  group_->SetUpdateStatus(AppCacheGroup::DOWNLOADING);
  NotifyAllAssociatedHosts(DOWNLOADING_EVENT);
  FetchUrls();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleUrlFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == DOWNLOADING);

  const GURL& url = request->original_url();
  pending_url_fetches_.erase(url);
  ++url_fetches_completed_;

  int response_code = request->GetResponseCode();
  AppCacheEntry& entry = url_file_list_.find(url)->second;

  if (request->status().is_success() && response_code == 200) {
    // TODO(jennb): associate storage with the new entry
    inprogress_cache_->AddEntry(url, entry);

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    LOG(ERROR) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;

    // TODO(jennb): discard any stored data for this entry
    if (entry.IsExplicit() || entry.IsFallback()) {
      internal_state_ = CACHE_FAILURE;

      // Cancel any pending URL requests.
      for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
           it != pending_url_fetches_.end(); ++it) {
        it->second->Cancel();
        delete it->second;
      }

      url_fetches_completed_ +=
          pending_url_fetches_.size() + urls_to_fetch_.size();
      pending_url_fetches_.clear();
      urls_to_fetch_.clear();
    } else if (response_code == 404 || response_code == 410) {
      // Entry is skipped.  They are dropped from the cache.
    } else if (update_type_ == UPGRADE_ATTEMPT) {
      // Copy the resource and its metadata from the newest complete cache.
      AppCache* cache = group_->newest_complete_cache();
      AppCacheEntry* copy = cache->GetEntry(url);
      if (copy)
        CopyEntryToCache(url, *copy, &entry);
    }
  }

  // Fetch another URL now that one request has completed.
  if (internal_state_ != CACHE_FAILURE)
    FetchUrls();

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == REFETCH_MANIFEST);
  manifest_url_request_ = NULL;

  int response_code = request->GetResponseCode();
  if (response_code == 304 || manifest_data_ == manifest_refetch_data_) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST);
    // TODO(jennb): add manifest_data_ to storage and put storage key in entry
    // Also store response headers from request for HTTP cache control.
    inprogress_cache_->AddOrModifyEntry(manifest_url_, entry);
    inprogress_cache_->set_update_time(base::TimeTicks::Now());

    // TODO(jennb): start of part to make async (cache storage may fail;
    // group storage may fail)
    inprogress_cache_->set_complete(true);

    // TODO(jennb): write new cache to storage here
    group_->AddCache(inprogress_cache_);
    // TODO(jennb): write new group to storage here
    inprogress_cache_ = NULL;

    if (update_type_ == CACHE_ATTEMPT) {
      NotifyAllAssociatedHosts(CACHED_EVENT);
    } else {
      NotifyAllAssociatedHosts(UPDATE_READY_EVENT);
    }
    DeleteSoon();
    // TODO(jennb): end of part that needs to be made async.
  } else {
    LOG(ERROR) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;
    ScheduleUpdateRetry(kRerunDelayMs);
    internal_state_ = CACHE_FAILURE;
    HandleCacheFailure();
  }
}

void AppCacheUpdateJob::NotifySingleHost(AppCacheHost* host,
                                         EventID event_id) {
  std::vector<int> ids(1, host->host_id());
  host->frontend()->OnEventRaised(ids, event_id);
}

void AppCacheUpdateJob::NotifyAllPendingMasterHosts(EventID event_id) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single pending master entry
  // so no need to worry about duplicate hosts being added to the notifier.
  HostNotifier host_notifier;
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host_notifier.AddHost(host);
    }
  }

  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(EventID event_id) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single cache so no need to worry
  // about duplicate hosts being added to the notifier.
  HostNotifier host_notifier;
  if (inprogress_cache_) {
    DCHECK(internal_state_ == DOWNLOADING || internal_state_ == CACHE_FAILURE);
    host_notifier.AddHosts(inprogress_cache_->associated_hosts());
  }

  AppCacheGroup::Caches old_caches = group_->old_caches();
  for (AppCacheGroup::Caches::const_iterator it = old_caches.begin();
       it != old_caches.end(); ++it) {
    host_notifier.AddHosts((*it)->associated_hosts());
  }

  AppCache* newest_cache = group_->newest_complete_cache();
  if (newest_cache)
    host_notifier.AddHosts(newest_cache->associated_hosts());

  // TODO(jennb): if progress event, also pass params lengthComputable=true,
  // total = url_file_list_.size(), loaded=url_fetches_completed_.
  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::CheckIfManifestChanged() {
  DCHECK(update_type_ == UPGRADE_ATTEMPT);
  /*
  AppCacheEntry* entry =
      group_->newest_complete_cache()->GetEntry(manifest_url_);
  */
  // TODO(jennb): load manifest data from entry (async), continues in callback
  // callback invokes ContinueCheckIfManifestChanged
  // For now, schedule a task to continue checking with fake loaded data
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &AppCacheUpdateJob::ContinueCheckIfManifestChanged,
          simulate_manifest_changed_ ? "different" : manifest_data_));
}

void AppCacheUpdateJob::ContinueCheckIfManifestChanged(
    const std::string& loaded_manifest) {
  ContinueHandleManifestFetchCompleted(manifest_data_ != loaded_manifest);
}

void AppCacheUpdateJob::BuildUrlFileList(const Manifest& manifest) {
  for (base::hash_set<std::string>::const_iterator it =
           manifest.explicit_urls.begin();
       it != manifest.explicit_urls.end(); ++it) {
    AddUrlToFileList(GURL(*it), AppCacheEntry::EXPLICIT);
  }

  const std::vector<FallbackNamespace>& fallbacks =
      manifest.fallback_namespaces;
  for (std::vector<FallbackNamespace>::const_iterator it = fallbacks.begin();
       it != fallbacks.end(); ++it) {
     AddUrlToFileList(it->second, AppCacheEntry::FALLBACK);
  }

  // Add all master entries from newest complete cache.
  if (update_type_ == UPGRADE_ATTEMPT) {
    const AppCache::EntryMap& entries =
        group_->newest_complete_cache()->entries();
    for (AppCache::EntryMap::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
      const AppCacheEntry& entry = it->second;
      if (entry.IsMaster())
        AddUrlToFileList(it->first, AppCacheEntry::MASTER);
    }
  }
}

void AppCacheUpdateJob::AddUrlToFileList(const GURL& url, int type) {
  std::pair<AppCache::EntryMap::iterator, bool> ret = url_file_list_.insert(
      AppCache::EntryMap::value_type(url, AppCacheEntry(type)));

  if (ret.second)
    urls_to_fetch_.push_back(UrlToFetch(url, false));
  else
    ret.first->second.add_types(type);  // URL already exists. Merge types.
}

void AppCacheUpdateJob::FetchUrls() {
  DCHECK(internal_state_ == DOWNLOADING);

  // Fetch each URL in the list according to section 6.9.4 step 17.1-17.3.
  // Fetch up to the concurrent limit. Other fetches will be triggered as each
  // each fetch completes.
  while (pending_url_fetches_.size() < kMaxConcurrentUrlFetches &&
         !urls_to_fetch_.empty()) {
    // Notify about progress first to ensure it starts from 0% in case any
    // entries are skipped.
    NotifyAllAssociatedHosts(PROGRESS_EVENT);

    const GURL url = urls_to_fetch_.front().first;
    bool storage_checked = urls_to_fetch_.front().second;
    urls_to_fetch_.pop_front();

    AppCache::EntryMap::iterator it = url_file_list_.find(url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    if (ShouldSkipUrlFetch(entry)) {
      ++url_fetches_completed_;
    } else if (!storage_checked && MaybeLoadFromNewestCache(url, entry)) {
      // Continues asynchronously after data is loaded from newest cache.
    } else {
      // Send URL request for the resource.
      URLRequest* request = new URLRequest(url, this);
      request->SetUserData(this, new UpdateJobInfo(UpdateJobInfo::URL_FETCH));
      request->set_context(service_->request_context());
      request->set_load_flags(
          request->load_flags() | net::LOAD_DISABLE_INTERCEPT);
      request->Start();
      pending_url_fetches_.insert(PendingUrlFetches::value_type(url, request));
    }
  }
}

bool AppCacheUpdateJob::ShouldSkipUrlFetch(const AppCacheEntry& entry) {
  if (entry.IsExplicit() || entry.IsFallback()) {
    return false;
  }

  // TODO(jennb): decide if entry should be skipped to expire it from cache
  return false;
}

bool AppCacheUpdateJob::MaybeLoadFromNewestCache(const GURL& url,
                                                 AppCacheEntry& entry) {
  if (update_type_ != UPGRADE_ATTEMPT)
    return false;

  AppCache* newest = group_->newest_complete_cache();
  AppCacheEntry* copy_me = newest->GetEntry(url);
  if (!copy_me)
    return false;

  // TODO(jennb): load HTTP headers for copy_me and wait for callback
  // In callback:
  // if HTTP caching semantics for entry allows its use,
  //   CopyEntryData(url, copy_me, entry)
  //   ++urls_fetches_completed_;
  // Else, add url back to front of urls_to_fetch and call FetchUrls().
  //   flag url somehow so that FetchUrls() doesn't end up here again.
  // For now: post a message to pretend entry could not be copied
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &AppCacheUpdateJob::SimulateFailedLoadFromNewestCache, url));
  return true;
}

// TODO(jennb): delete this after have real storage code
void AppCacheUpdateJob::SimulateFailedLoadFromNewestCache(const GURL& url) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(AppCacheUpdateJob::UrlToFetch(url, true));
  FetchUrls();
}

void AppCacheUpdateJob::CopyEntryToCache(const GURL& url,
                                         const AppCacheEntry& src,
                                         AppCacheEntry* dest) {
  DCHECK(dest);
  // TODO(jennb): copy storage key from src to dest
  inprogress_cache_->AddEntry(url, *dest);
}

bool AppCacheUpdateJob::MaybeCompleteUpdate() {
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size() ) {
    return false;
  }

  switch (internal_state_) {
    case NO_UPDATE:
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(NO_UPDATE_EVENT);
      pending_master_entries_.clear();
      DeleteSoon();
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      return false;
    case CACHE_FAILURE:
      HandleCacheFailure();
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::HandleCacheFailure() {
  // 6.9.4 cache failure steps 2-8.
  NotifyAllAssociatedHosts(ERROR_EVENT);
  pending_master_entries_.clear();

  // Discard the inprogress cache.
  // TODO(jennb): cleanup possible storage for entries in the cache
  if (inprogress_cache_) {
    inprogress_cache_->set_owning_group(NULL);
    inprogress_cache_ = NULL;
  }

  // For a CACHE_ATTEMPT, group will be discarded when this update
  // job removes its reference to the group. Nothing more to do here.

  DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  if (manifest_url_request_) {
    delete manifest_url_request_;
    manifest_url_request_ = NULL;
  }

  // TODO(jennb): code other cancel cleanup (pending url requests, storage)
}

void AppCacheUpdateJob::DeleteSoon() {
  // TODO(jennb): revisit if update should be deleting itself
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
}  // namespace appcache
