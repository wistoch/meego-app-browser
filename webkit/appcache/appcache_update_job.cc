// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_update_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"

namespace appcache {

static const int kBufferSize = 4096;
static const size_t kMaxConcurrentUrlFetches = 2;
static const int kMax503Retries = 3;

// Extra info associated with requests for use during response processing.
// This info is deleted when the URLRequest is deleted.
class UpdateJobInfo :  public URLRequest::UserData {
 public:
  enum RequestType {
    MANIFEST_FETCH,
    URL_FETCH,
    MANIFEST_REFETCH,
  };

  explicit UpdateJobInfo(RequestType request_type)
      : type_(request_type),
        buffer_(new net::IOBuffer(kBufferSize)),
        retry_503_attempts_(0),
        update_job_(NULL),
        request_(NULL),
        wrote_response_info_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(write_callback_(
            this, &UpdateJobInfo::OnWriteComplete)) {
  }

  void SetUpResponseWriter(AppCacheResponseWriter* writer,
                         AppCacheUpdateJob* update,
                         URLRequest* request) {
    DCHECK(!response_writer_.get());
    response_writer_.reset(writer);
    update_job_ = update;
    request_ = request;
  }

  void OnWriteComplete(int result) {
    // A completed write may delete the URL request and this object.
    update_job_->OnWriteResponseComplete(result, request_, this);
  }

  RequestType type_;
  scoped_refptr<net::IOBuffer> buffer_;
  int retry_503_attempts_;

  // Info needed to write responses to storage and process callbacks.
  scoped_ptr<AppCacheResponseWriter> response_writer_;
  AppCacheUpdateJob* update_job_;
  URLRequest* request_;
  bool wrote_response_info_;
  net::CompletionCallbackImpl<UpdateJobInfo> write_callback_;
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
      manifest_url_request_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(manifest_info_write_callback_(
          this, &AppCacheUpdateJob::OnManifestInfoWriteComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(manifest_data_write_callback_(
          this, &AppCacheUpdateJob::OnManifestDataWriteComplete)) {
  DCHECK(group_);
  manifest_url_ = group_->manifest_url();
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  if (internal_state_ != COMPLETED)
    Cancel();

  DCHECK(!manifest_url_request_);
  DCHECK(pending_url_fetches_.empty());
  DCHECK(!inprogress_cache_);

  if (group_)
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
  if (internal_state_ == CACHE_FAILURE || internal_state_ == CANCELLED ||
      internal_state_ == COMPLETED) {
    return;
  }

  int bytes_read = 0;
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  request->Read(info->buffer_, kBufferSize, &bytes_read);
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
      while (request->Read(info->buffer_, kBufferSize, &bytes_read)) {
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
  DCHECK_GT(bytes_read, 0);
  switch (info->type_) {
    case UpdateJobInfo::MANIFEST_FETCH:
      manifest_data_.append(info->buffer_->data(), bytes_read);
      break;
    case UpdateJobInfo::URL_FETCH:
      if (!info->response_writer_.get()) {
        info->SetUpResponseWriter(
            service_->storage()->CreateResponseWriter(manifest_url_),
            this, request);
      }
      info->response_writer_->WriteData(info->buffer_, bytes_read,
                                        &info->write_callback_);
      return false;  // wait for async write completion to continue reading
    case UpdateJobInfo::MANIFEST_REFETCH:
      manifest_refetch_data_.append(info->buffer_->data(), bytes_read);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::OnWriteResponseComplete(int result,
                                                URLRequest* request,
                                                UpdateJobInfo* info) {
  DCHECK(internal_state_ == DOWNLOADING);

  if (result < 0) {
    request->Cancel();
    OnResponseCompleted(request);
    return;
  }

  if (!info->wrote_response_info_) {
    info->wrote_response_info_ = true;
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        new HttpResponseInfoIOBuffer(
            new net::HttpResponseInfo(request->response_info()));
    info->response_writer_->WriteInfo(io_buffer, &info->write_callback_);
    return;
  }

  ReadResponseData(request);
}

void AppCacheUpdateJob::OnReceivedRedirect(URLRequest* request,
                                           const GURL& new_url,
                                           bool* defer_redirect) {
  // Redirect is not allowed by the update process.
  request->Cancel();
  OnResponseCompleted(request);
}

void AppCacheUpdateJob::OnResponseCompleted(URLRequest* request) {
  // Retry for 503s where retry-after is 0.
  if (request->status().is_success() &&
      request->GetResponseCode() == 503 &&
      RetryRequest(request)) {
    return;
  }

  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  switch (info->type_) {
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

bool AppCacheUpdateJob::RetryRequest(URLRequest* request) {
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  if (info->retry_503_attempts_ >= kMax503Retries) {
    return false;
  }

  if (!request->response_headers()->HasHeaderValue("retry-after", "0"))
    return false;

  const GURL& url = request->original_url();
  URLRequest* retry = new URLRequest(url, this);
  UpdateJobInfo* retry_info = new UpdateJobInfo(info->type_);
  retry_info->retry_503_attempts_ = info->retry_503_attempts_ + 1;
  retry->SetUserData(this, retry_info);
  retry->set_context(request->context());
  retry->set_load_flags(request->load_flags());

  switch (info->type_) {
    case UpdateJobInfo::MANIFEST_FETCH:
    case UpdateJobInfo::MANIFEST_REFETCH:
      manifest_url_request_ = retry;
      manifest_data_.clear();
      break;
    case UpdateJobInfo::URL_FETCH:
      pending_url_fetches_.erase(url);
      pending_url_fetches_.insert(PendingUrlFetches::value_type(url, retry));
      break;
    default:
      NOTREACHED();
  }

  retry->Start();

  delete request;
  return true;
}

void AppCacheUpdateJob::HandleManifestFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == FETCH_MANIFEST);
  manifest_url_request_ = NULL;

  if (!request->status().is_success()) {
    LOG(INFO) << "Request non-success, status: " << request->status().status()
        << " os_error: " << request->status().os_error();
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
    return;
  }

  int response_code = request->GetResponseCode();
  std::string mime_type;
  request->GetMimeType(&mime_type);
  manifest_response_info_.reset(
      new net::HttpResponseInfo(request->response_info()));

  if ((response_code / 100 == 2) && mime_type == kManifestMimeType) {
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if (response_code == 404 || response_code == 410) {
    service_->storage()->MakeGroupObsolete(group_, this);  // async
  } else {
    LOG(INFO) << "Cache failure, response code: " << response_code;
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
  }
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success) {
  NotifyAllPendingMasterHosts(ERROR_EVENT);
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(OBSOLETE_EVENT);
    internal_state_ = COMPLETED;
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    internal_state_ = CACHE_FAILURE;
  }
  MaybeCompleteUpdate();
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
    LOG(INFO) << "Failed to parse manifest: " << manifest_url_;
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
    return;
  }

  // Proceed with update process. Section 6.9.4 steps 8-20.
  internal_state_ = DOWNLOADING;
  inprogress_cache_ = new AppCache(service_,
                                   service_->storage()->NewCacheId());
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

  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));

  if (request->status().is_success() && (response_code / 100 == 2)) {
    // Associate storage with the new entry.
    DCHECK(info->response_writer_.get());
    entry.set_response_id(info->response_writer_->response_id());

    inprogress_cache_->AddEntry(url, entry);

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    LOG(INFO) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;

    // TODO(jennb): Discard any stored data for this entry? May be unnecessary
    // if handled automatically by storage layer.

    if (entry.IsExplicit() || entry.IsFallback()) {
      internal_state_ = CACHE_FAILURE;

      // Cancel any pending URL requests.
      for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
           it != pending_url_fetches_.end(); ++it) {
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
    // Only need to store response in storage if manifest is not already an
    // an entry in the cache.
    AppCacheEntry* entry = inprogress_cache_->GetEntry(manifest_url_);
    if (entry) {
      entry->add_types(AppCacheEntry::MANIFEST);
      CompleteInprogressCache();
    } else {
      manifest_response_writer_.reset(
          service_->storage()->CreateResponseWriter(manifest_url_));
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
          new HttpResponseInfoIOBuffer(manifest_response_info_.release());
      manifest_response_writer_->WriteInfo(io_buffer,
                                           &manifest_info_write_callback_);
    }
  } else {
    LOG(INFO) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::OnManifestInfoWriteComplete(int result) {
  if (result > 0) {
    scoped_refptr<net::StringIOBuffer> io_buffer =
        new net::StringIOBuffer(manifest_data_);
    manifest_response_writer_->WriteData(io_buffer, manifest_data_.length(),
                                         &manifest_data_write_callback_);
  } else {
    // Treat storage failure as if refetch of manifest failed.
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
        manifest_response_writer_->response_id());
    inprogress_cache_->AddOrModifyEntry(manifest_url_, entry);
    CompleteInprogressCache();
  } else {
    // Treat storage failure as if refetch of manifest failed.
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::CompleteInprogressCache() {
  inprogress_cache_->set_update_time(base::TimeTicks::Now());
  inprogress_cache_->set_complete(true);

  protect_former_newest_cache_ = group_->newest_complete_cache();
  group_->AddCache(inprogress_cache_);
  protect_new_cache_.swap(inprogress_cache_);

  service_->storage()->StoreGroupAndNewestCache(group_, this);  // async
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    bool success) {
  if (success) {
    if (update_type_ == CACHE_ATTEMPT)
      NotifyAllAssociatedHosts(CACHED_EVENT);
    else
      NotifyAllAssociatedHosts(UPDATE_READY_EVENT);
    internal_state_ = COMPLETED;
    MaybeCompleteUpdate();  // will definitely complete
  } else {
    // TODO(jennb): Change storage so clients won't need to revert group state?
    // Change group back to reflect former newest group.
    group_->RestoreCacheAsNewest(protect_former_newest_cache_);
    protect_new_cache_ = NULL;

    // Treat storage failure as if manifest refetch failed.
    HandleManifestRefetchFailure();
  }
  protect_former_newest_cache_ = NULL;
}

void AppCacheUpdateJob::HandleManifestRefetchFailure() {
    ScheduleUpdateRetry(kRerunDelayMs);
    internal_state_ = CACHE_FAILURE;
    MaybeCompleteUpdate();  // will definitely complete
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
  dest->set_response_id(src.response_id());
  inprogress_cache_->AddEntry(url, *dest);
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size() ) {
    DCHECK(internal_state_ != COMPLETED);
    return;
  }

  switch (internal_state_) {
    case NO_UPDATE:
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(NO_UPDATE_EVENT);
      pending_master_entries_.clear();
      internal_state_ = COMPLETED;
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      break;
    case CACHE_FAILURE:
      // 6.9.4 cache failure steps 2-8.
      NotifyAllAssociatedHosts(ERROR_EVENT);
      pending_master_entries_.clear();
      DiscardInprogressCache();
      // For a CACHE_ATTEMPT, group will be discarded when the host(s) that
      // started this update removes its reference to the group. Nothing more
      // to do here.
      internal_state_ = COMPLETED;
      break;
    default:
      break;
  }

  // Let the stack unwind before deletion to make it less risky as this
  // method is called from multiple places in this file.
  if (internal_state_ == COMPLETED)
    DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  internal_state_ = CANCELLED;

  if (manifest_url_request_) {
    delete manifest_url_request_;
    manifest_url_request_ = NULL;
  }

  for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
       it != pending_url_fetches_.end(); ++it) {
    delete it->second;
  }

  pending_master_entries_.clear();
  DiscardInprogressCache();

  // Delete response writer to avoid any callbacks.
  if (manifest_response_writer_.get())
    manifest_response_writer_.reset();

  service_->storage()->CancelDelegateCallbacks(this);
}

void AppCacheUpdateJob::DiscardInprogressCache() {
  if (!inprogress_cache_)
    return;

  // TODO(jennb): Cleanup stored responses for entries in the cache?
  // May not be necessary if handled automatically by storage layer.

  inprogress_cache_ = NULL;
}

void AppCacheUpdateJob::DeleteSoon() {
  manifest_response_writer_.reset();
  service_->storage()->CancelDelegateCallbacks(this);

  // Break the connection with the group so the group cannot call delete
  // on this object after we've posted a task to delete ourselves.
  group_->SetUpdateStatus(AppCacheGroup::IDLE);
  protect_new_cache_ = NULL;
  group_ = NULL;

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace appcache
