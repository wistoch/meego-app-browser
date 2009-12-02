// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_transaction.h"

#include "base/compiler_specific.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction.h"
#include "net/http/http_util.h"
#include "net/http/partial_data.h"

using base::Time;

namespace net {

// disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

//-----------------------------------------------------------------------------

struct HeaderNameAndValue {
  const char* name;
  const char* value;
};

// If the request includes one of these request headers, then avoid caching
// to avoid getting confused.
static const HeaderNameAndValue kPassThroughHeaders[] = {
  { "if-unmodified-since", NULL },  // causes unexpected 412s
  { "if-match", NULL },             // causes unexpected 412s
  { "if-range", NULL },
  { NULL, NULL }
};

struct ValidationHeaderInfo {
  const char* request_header_name;
  const char* related_response_header_name;
};

static const ValidationHeaderInfo kValidationHeaders[] = {
  { "if-modified-since", "last-modified" },
  { "if-none-match", "etag" },
};

// If the request includes one of these request headers, then avoid reusing
// our cached copy if any.
static const HeaderNameAndValue kForceFetchHeaders[] = {
  { "cache-control", "no-cache" },
  { "pragma", "no-cache" },
  { NULL, NULL }
};

// If the request includes one of these request headers, then force our
// cached copy (if any) to be revalidated before reusing it.
static const HeaderNameAndValue kForceValidateHeaders[] = {
  { "cache-control", "max-age=0" },
  { NULL, NULL }
};

static bool HeaderMatches(const HttpUtil::HeadersIterator& h,
                          const HeaderNameAndValue* search) {
  for (; search->name; ++search) {
    if (!LowerCaseEqualsASCII(h.name_begin(), h.name_end(), search->name))
      continue;

    if (!search->value)
      return true;

    HttpUtil::ValuesIterator v(h.values_begin(), h.values_end(), ',');
    while (v.GetNext()) {
      if (LowerCaseEqualsASCII(v.value_begin(), v.value_end(), search->value))
        return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

HttpCache::Transaction::Transaction(HttpCache* cache, bool enable_range_support)
    : request_(NULL),
      cache_(cache->AsWeakPtr()),
      entry_(NULL),
      network_trans_(NULL),
      callback_(NULL),
      mode_(NONE),
      reading_(false),
      invalid_range_(false),
      enable_range_support_(enable_range_support),
      truncated_(false),
      read_offset_(0),
      effective_load_flags_(0),
      final_upload_progress_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          network_info_callback_(this, &Transaction::OnNetworkInfoAvailable)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          network_read_callback_(this, &Transaction::OnNetworkReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          cache_read_callback_(new CancelableCompletionCallback<Transaction>(
              this, &Transaction::OnCacheReadCompleted))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          cache_write_callback_(new CancelableCompletionCallback<Transaction>(
              this, &Transaction::OnCacheWriteCompleted))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          entry_ready_callback_(new CancelableCompletionCallback<Transaction>(
                this, &Transaction::OnCacheEntryReady))) {
  COMPILE_ASSERT(HttpCache::Transaction::kNumValidationHeaders ==
                     ARRAYSIZE_UNSAFE(kValidationHeaders),
                 Invalid_number_of_validation_headers);
}

HttpCache::Transaction::~Transaction() {
  if (cache_) {
    if (entry_) {
      bool cancel_request = reading_ && enable_range_support_;
      if (cancel_request) {
        if (partial_.get()) {
          entry_->disk_entry->CancelSparseIO();
        } else {
          cancel_request &= (response_.headers->response_code() == 200);
        }
      }

      cache_->DoneWithEntry(entry_, this, cancel_request);
    } else {
      cache_->RemovePendingTransaction(this);
    }
  }

  // If there is an outstanding callback, mark it as cancelled so running it
  // does nothing.
  cache_read_callback_->Cancel();
  cache_write_callback_->Cancel();
  entry_ready_callback_->Cancel();

  // We could still have a cache read or write in progress, so we just null the
  // cache_ pointer to signal that we are dead.  See DoCacheReadCompleted.
  cache_.reset();
}

int HttpCache::Transaction::Start(const HttpRequestInfo* request,
                                  CompletionCallback* callback,
                                  LoadLog* load_log) {
  DCHECK(request);
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (!cache_)
    return ERR_UNEXPECTED;

  SetRequest(load_log, request);

  int rv;

  if (!ShouldPassThrough()) {
    cache_key_ = cache_->GenerateCacheKey(request);

    // requested cache access mode
    if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
      mode_ = READ;
    } else if (effective_load_flags_ & LOAD_BYPASS_CACHE) {
      mode_ = WRITE;
    } else {
      mode_ = READ_WRITE;
    }

    // Downgrade to UPDATE if the request has been externally conditionalized.
    if (external_validation_.initialized) {
      if (mode_ & WRITE) {
        // Strip off the READ_DATA bit (and maybe add back a READ_META bit
        // in case READ was off).
        mode_ = UPDATE;
      } else {
        mode_ = NONE;
      }
    }
  }

  // if must use cache, then we must fail.  this can happen for back/forward
  // navigations to a page generated via a form post.
  if (!(mode_ & READ) && effective_load_flags_ & LOAD_ONLY_FROM_CACHE)
    return ERR_CACHE_MISS;

  if (mode_ == NONE) {
    if (partial_.get())
      partial_->RestoreHeaders(&custom_request_->extra_headers);
    rv = BeginNetworkRequest();
  } else {
    rv = AddToEntry();
  }

  // setting this here allows us to check for the existance of a callback_ to
  // determine if we are still inside Start.
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (!cache_)
    return ERR_UNEXPECTED;

  int rv = RestartNetworkRequest();

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartWithCertificate(
    X509Certificate* client_cert,
    CompletionCallback* callback) {
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (!cache_)
    return ERR_UNEXPECTED;

  int rv = RestartNetworkRequestWithCertificate(client_cert);

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  DCHECK(auth_response_.headers);
  DCHECK(callback);

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  if (!cache_)
    return ERR_UNEXPECTED;

  // Clear the intermediate response since we are going to start over.
  auth_response_ = HttpResponseInfo();

  int rv = RestartNetworkRequestWithAuth(username, password);

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

bool HttpCache::Transaction::IsReadyToRestartForAuth() {
  if (!network_trans_.get())
    return false;
  return network_trans_->IsReadyToRestartForAuth();
}

int HttpCache::Transaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(callback);

  DCHECK(!callback_);

  if (!cache_)
    return ERR_UNEXPECTED;

  // If we have an intermediate auth response at this point, then it means the
  // user wishes to read the network response (the error page).  If there is a
  // previous response in the cache then we should leave it intact.
  if (auth_response_.headers && mode_ != NONE) {
    DCHECK(mode_ & WRITE);
    DoneWritingToEntry(mode_ == READ_WRITE);
    mode_ = NONE;
  }

  reading_ = true;
  int rv;

  switch (mode_) {
    case READ_WRITE:
      DCHECK(partial_.get());
      if (!network_trans_.get()) {
        // We are just reading from the cache, but we may be writing later.
        rv = ReadFromEntry(buf, buf_len);
        break;
      }
    case NONE:
    case WRITE:
      DCHECK(network_trans_.get());
      rv = ReadFromNetwork(buf, buf_len);
      break;
    case READ:
      rv = ReadFromEntry(buf, buf_len);
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }

  if (rv == ERR_IO_PENDING) {
    DCHECK(!callback_);
    callback_ = callback;
  }
  return rv;
}

const HttpResponseInfo* HttpCache::Transaction::GetResponseInfo() const {
  // Null headers means we encountered an error or haven't a response yet
  if (auth_response_.headers)
    return &auth_response_;
  return (response_.headers || response_.ssl_info.cert ||
          response_.cert_request_info) ? &response_ : NULL;
}

LoadState HttpCache::Transaction::GetLoadState() const {
  if (network_trans_.get())
    return network_trans_->GetLoadState();
  if (entry_ || !request_)
    return LOAD_STATE_IDLE;
  return LOAD_STATE_WAITING_FOR_CACHE;
}

uint64 HttpCache::Transaction::GetUploadProgress() const {
  if (network_trans_.get())
    return network_trans_->GetUploadProgress();
  return final_upload_progress_;
}

int HttpCache::Transaction::AddToEntry() {
  ActiveEntry* entry = NULL;

  if (!cache_)
    return ERR_UNEXPECTED;

  if (mode_ == WRITE) {
    cache_->DoomEntry(cache_key_);
  } else {
    entry = cache_->FindActiveEntry(cache_key_);
    if (!entry) {
      LoadLog::BeginEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY);
      entry = cache_->OpenEntry(cache_key_);
      LoadLog::EndEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY);
      if (!entry) {
        if (mode_ == READ_WRITE) {
          mode_ = WRITE;
        } else if (mode_ == UPDATE) {
          // There is no cache entry to update; proceed without caching.
          mode_ = NONE;
          return BeginNetworkRequest();
        } else {
          if (cache_->mode() == PLAYBACK)
            DLOG(INFO) << "Playback Cache Miss: " << request_->url;

          // entry does not exist, and not permitted to create a new entry, so
          // we must fail.
          return HandleResult(ERR_CACHE_MISS);
        }
      }
    }
  }

  if (mode_ == WRITE) {
    DCHECK(!entry);
    LoadLog::BeginEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY);
    entry = cache_->CreateEntry(cache_key_);
    LoadLog::EndEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY);
    if (!entry) {
      DLOG(WARNING) << "unable to create cache entry";
      mode_ = NONE;
      if (partial_.get())
        partial_->RestoreHeaders(&custom_request_->extra_headers);
      return BeginNetworkRequest();
    }
  }


  LoadLog::BeginEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_WAITING);
  return cache_->AddTransactionToEntry(entry, this);
}

int HttpCache::Transaction::EntryAvailable(ActiveEntry* entry) {
  LoadLog::EndEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_WAITING);

  // We now have access to the cache entry.
  //
  //  o if we are the writer for the transaction, then we can start the network
  //    transaction.
  //
  //  o if we are a reader for the transaction, then we can start reading the
  //    cache entry.
  //
  //  o if we can read or write, then we should check if the cache entry needs
  //    to be validated and then issue a network request if needed or just read
  //    from the cache if the cache entry is already valid.
  //
  //  o if we are set to UPDATE, then we are handling an externally
  //    conditionalized request (if-modified-since / if-none-match). We read
  //    the cache entry, and check if the request headers define a validation
  //    request.
  //
  int rv;
  entry_ = entry;
  switch (mode_) {
    case READ:
      rv = BeginCacheRead();
      break;
    case WRITE:
      if (partial_.get())
        partial_->RestoreHeaders(&custom_request_->extra_headers);
      rv = BeginNetworkRequest();
      break;
    case READ_WRITE:
      rv = BeginPartialCacheValidation();
      break;
    case UPDATE:
      rv = BeginExternallyConditionalizedRequest();
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }
  return rv;
}

bool HttpCache::Transaction::AddTruncatedFlag() {
  DCHECK(mode_ & WRITE);

  // Don't set the flag for sparse entries.
  if (partial_.get() && !truncated_)
    return true;

  // Double check that there is something worth keeping.
  if (!entry_->disk_entry->GetDataSize(kResponseContentIndex))
    return false;

  truncated_ = true;
  WriteResponseInfoToEntry(true);
  return true;
}

void HttpCache::Transaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

int HttpCache::Transaction::HandleResult(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  if (callback_)
    DoCallback(rv);
  return rv;
}

void HttpCache::Transaction::SetRequest(LoadLog* load_log,
                                        const HttpRequestInfo* request) {
  load_log_ = load_log;
  request_ = request;
  effective_load_flags_ = request_->load_flags;

  switch (cache_->mode()) {
    case NORMAL:
      break;
    case RECORD:
      // When in record mode, we want to NEVER load from the cache.
      // The reason for this is beacuse we save the Set-Cookie headers
      // (intentionally).  If we read from the cache, we replay them
      // prematurely.
      effective_load_flags_ |= LOAD_BYPASS_CACHE;
      break;
    case PLAYBACK:
      // When in playback mode, we want to load exclusively from the cache.
      effective_load_flags_ |= LOAD_ONLY_FROM_CACHE;
      break;
    case DISABLE:
      effective_load_flags_ |= LOAD_DISABLE_CACHE;
      break;
  }

  // Some headers imply load flags.  The order here is significant.
  //
  //   LOAD_DISABLE_CACHE   : no cache read or write
  //   LOAD_BYPASS_CACHE    : no cache read
  //   LOAD_VALIDATE_CACHE  : no cache read unless validation
  //
  // The former modes trump latter modes, so if we find a matching header we
  // can stop iterating kSpecialHeaders.
  //
  static const struct {
    const HeaderNameAndValue* search;
    int load_flag;
  } kSpecialHeaders[] = {
    { kPassThroughHeaders, LOAD_DISABLE_CACHE },
    { kForceFetchHeaders, LOAD_BYPASS_CACHE },
    { kForceValidateHeaders, LOAD_VALIDATE_CACHE },
  };

  std::string new_extra_headers;
  bool range_found = false;
  bool external_validation_error = false;

  // scan request headers to see if we have any that would impact our load flags
  HttpUtil::HeadersIterator it(request_->extra_headers.begin(),
                               request_->extra_headers.end(),
                               "\r\n");
  while (it.GetNext()) {
    if (!LowerCaseEqualsASCII(it.name(), "range")) {
      new_extra_headers.append(it.name_begin(), it.values_end());
      new_extra_headers.append("\r\n");
    } else {
      if (enable_range_support_) {
        range_found = true;
      } else {
        effective_load_flags_ |= LOAD_DISABLE_CACHE;
        continue;
      }
    }
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSpecialHeaders); ++i) {
      if (HeaderMatches(it, kSpecialHeaders[i].search)) {
        effective_load_flags_ |= kSpecialHeaders[i].load_flag;
        break;
      }
    }

    // Check for conditionalization headers which may correspond with a
    // cache validation request.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kValidationHeaders); ++i) {
      const ValidationHeaderInfo& info = kValidationHeaders[i];
      if (LowerCaseEqualsASCII(it.name_begin(), it.name_end(),
                               info.request_header_name)) {
        if (!external_validation_.values[i].empty() || it.values().empty())
          external_validation_error = true;
        external_validation_.values[i] = it.values();
        external_validation_.initialized = true;
        break;
      }
    }
  }

  // We don't support ranges and validation headers.
  if (range_found && external_validation_.initialized) {
    LOG(WARNING) << "Byte ranges AND validation headers found.";
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  // If there is more than one validation header, we can't treat this request as
  // a cache validation, since we don't know for sure which header the server
  // will give us a response for (and they could be contradictory).
  if (external_validation_error) {
    LOG(WARNING) << "Multiple or malformed validation headers found.";
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  if (range_found && !(effective_load_flags_ & LOAD_DISABLE_CACHE)) {
    partial_.reset(new PartialData);
    if (partial_->Init(request_->extra_headers)) {
      // We will be modifying the actual range requested to the server, so
      // let's remove the header here.
      custom_request_.reset(new HttpRequestInfo(*request_));
      request_ = custom_request_.get();
      custom_request_->extra_headers = new_extra_headers;
      partial_->SetHeaders(new_extra_headers);
    } else {
      // The range is invalid or we cannot handle it properly.
      LOG(INFO) << "Invalid byte range found.";
      effective_load_flags_ |= LOAD_DISABLE_CACHE;
      partial_.reset(NULL);
    }
  }
}

bool HttpCache::Transaction::ShouldPassThrough() {
  // We may have a null disk_cache if there is an error we cannot recover from,
  // like not enough disk space, or sharing violations.
  if (!cache_->disk_cache_.get())
    return true;

  // When using the record/playback modes, we always use the cache
  // and we never pass through.
  if (cache_->mode() == RECORD || cache_->mode() == PLAYBACK)
    return false;

  if (effective_load_flags_ & LOAD_DISABLE_CACHE)
    return true;

  if (request_->method == "GET")
    return false;

  if (request_->method == "POST" &&
      request_->upload_data && request_->upload_data->identifier())
    return false;

  // TODO(darin): add support for caching HEAD responses
  return true;
}

int HttpCache::Transaction::BeginCacheRead() {
  DCHECK(mode_ == READ);

  // Read response headers.
  int rv = ReadResponseInfoFromEntry();
  if (rv != OK)
    return HandleResult(rv);

  // We don't support any combination of LOAD_ONLY_FROM_CACHE and byte ranges.
  if (response_.headers->response_code() == 206 || partial_.get()) {
    NOTREACHED();
    return HandleResult(ERR_CACHE_MISS);
  }

  // We don't have the whole resource.
  if (truncated_)
    return HandleResult(ERR_CACHE_MISS);

  return HandleResult(rv);
}

int HttpCache::Transaction::BeginCacheValidation() {
  DCHECK(mode_ == READ_WRITE);

  if ((effective_load_flags_ & LOAD_PREFERRING_CACHE ||
       !RequiresValidation()) && !partial_.get()) {
    cache_->ConvertWriterToReader(entry_);
    mode_ = READ;
  } else {
    // Make the network request conditional, to see if we may reuse our cached
    // response.  If we cannot do so, then we just resort to a normal fetch.
    // Our mode remains READ_WRITE for a conditional request.  We'll switch to
    // either READ or WRITE mode once we hear back from the server.
    if (!ConditionalizeRequest())
      mode_ = WRITE;
    return BeginNetworkRequest();
  }
  return HandleResult(OK);
}

int HttpCache::Transaction::BeginPartialCacheValidation() {
  DCHECK(mode_ == READ_WRITE);

  int rv = ReadResponseInfoFromEntry();
  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
    return HandleResult(rv);
  }

  if (response_.headers->response_code() != 206 && !partial_.get() &&
      !truncated_)
    return BeginCacheValidation();

  if (!enable_range_support_)
    return BeginCacheValidation();

  bool byte_range_requested = partial_.get() != NULL;
  if (byte_range_requested) {
    // Balanced in ValidateEntryHeadersAndContinue.
    entry_ready_callback_->AddRef();
    if (OK != entry_->disk_entry->ReadyForSparseIO(entry_ready_callback_))
      return ERR_IO_PENDING;
  } else {
    // The request is not for a range, but we have stored just ranges.
    partial_.reset(new PartialData());
    partial_->SetHeaders(request_->extra_headers);
    if (!custom_request_.get()) {
      custom_request_.reset(new HttpRequestInfo(*request_));
      request_ = custom_request_.get();
    }
  }

  return ValidateEntryHeadersAndContinue(byte_range_requested);
}

int HttpCache::Transaction::ValidateEntryHeadersAndContinue(
    bool byte_range_requested) {
  DCHECK(mode_ == READ_WRITE);

  if (byte_range_requested) {
    // Balance the AddRef from BeginPartialCacheValidation.
    entry_ready_callback_->Release();
  }

  if (!cache_)
    return HandleResult(ERR_UNEXPECTED);

  if (!partial_->UpdateFromStoredHeaders(response_.headers, entry_->disk_entry,
                                         truncated_)) {
    // The stored data cannot be used. Get rid of it and restart this request.
    // We need to also reset the |truncated_| flag as a new entry is created.
    DoomPartialEntry(!byte_range_requested);
    mode_ = WRITE;
    truncated_ = false;
    return AddToEntry();
  }

  if (!partial_->IsRequestedRangeOK()) {
    // The stored data is fine, but the request may be invalid.
    invalid_range_ = true;
  }

  return ContinuePartialCacheValidation();
}

int HttpCache::Transaction::ContinuePartialCacheValidation() {
  DCHECK(mode_ == READ_WRITE);
  int rv = partial_->PrepareCacheValidation(entry_->disk_entry,
                                            &custom_request_->extra_headers);

  if (!rv) {
    // Don't invoke the callback before telling the cache we're done.
    return rv;
  }

  if (rv < 0) {
    DCHECK(rv != ERR_IO_PENDING);
    return HandleResult(rv);
  }

  if (reading_ && partial_->IsCurrentRangeCached()) {
    rv = ReadFromEntry(read_buf_, read_buf_len_);

    // We are supposed to hanlde errors here.
    if (rv < 0 && rv != ERR_IO_PENDING)
      HandleResult(rv);
    return rv;
  }

  return BeginCacheValidation();
}

int HttpCache::Transaction::BeginExternallyConditionalizedRequest() {
  DCHECK_EQ(UPDATE, mode_);
  DCHECK(external_validation_.initialized);

  // Read the cached response.
  int rv = ReadResponseInfoFromEntry();
  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
    return HandleResult(rv);
  }

  for (size_t i = 0;  i < ARRAYSIZE_UNSAFE(kValidationHeaders); i++) {
    if (external_validation_.values[i].empty())
      continue;
    // Retrieve either the cached response's "etag" or "last-modified" header.
    std::string validator;
    response_.headers->EnumerateHeader(
        NULL,
        kValidationHeaders[i].related_response_header_name,
        &validator);

    if (response_.headers->response_code() != 200 || truncated_ ||
        validator.empty() || validator != external_validation_.values[i]) {
      // The externally conditionalized request is not a validation request
      // for our existing cache entry. Proceed with caching disabled.
      DoneWritingToEntry(true);
    }
  }

  return BeginNetworkRequest();
}

int HttpCache::Transaction::BeginNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(!network_trans_.get());

  // Create a network transaction.
  int rv = cache_->network_layer_->CreateTransaction(&network_trans_);
  if (rv != OK)
    return rv;

  rv = network_trans_->Start(request_, &network_info_callback_, load_log_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());

  int rv = network_trans_->RestartIgnoringLastError(&network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithCertificate(
    X509Certificate* client_cert) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());

  int rv = network_trans_->RestartWithCertificate(client_cert,
                                                  &network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithAuth(
    const std::wstring& username,
    const std::wstring& password) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());

  int rv = network_trans_->RestartWithAuth(username, password,
                                           &network_info_callback_);
  if (rv != ERR_IO_PENDING)
    OnNetworkInfoAvailable(rv);
  return rv;
}

bool HttpCache::Transaction::RequiresValidation() {
  // TODO(darin): need to do more work here:
  //  - make sure we have a matching request method
  //  - watch out for cached responses that depend on authentication
  // In playback mode, nothing requires validation.
  if (cache_->mode() == net::HttpCache::PLAYBACK)
    return false;

  if (effective_load_flags_ & LOAD_VALIDATE_CACHE)
    return true;

  if (response_.headers->response_code() == 206 && !enable_range_support_)
    return true;

  if (response_.headers->RequiresValidation(
          response_.request_time, response_.response_time, Time::Now()))
    return true;

  // Since Vary header computation is fairly expensive, we save it for last.
  if (response_.vary_data.is_valid() &&
          !response_.vary_data.MatchesRequest(*request_, *response_.headers))
    return true;

  return false;
}

bool HttpCache::Transaction::ConditionalizeRequest() {
  DCHECK(response_.headers);

  if (!enable_range_support_ && response_.headers->response_code() != 200) {
    // This only makes sense for cached 200 responses.
    return false;
  }

  // This only makes sense for cached 200 or 206 responses.
  if (response_.headers->response_code() != 200 &&
      response_.headers->response_code() != 206)
    return false;

  // Just use the first available ETag and/or Last-Modified header value.
  // TODO(darin): Or should we use the last?

  std::string etag_value;
  response_.headers->EnumerateHeader(NULL, "etag", &etag_value);

  std::string last_modified_value;
  response_.headers->EnumerateHeader(NULL, "last-modified",
                                     &last_modified_value);

  if (etag_value.empty() && last_modified_value.empty())
    return false;

  if (!partial_.get()) {
    // Need to customize the request, so this forces us to allocate :(
    custom_request_.reset(new HttpRequestInfo(*request_));
    request_ = custom_request_.get();
  }
  DCHECK(custom_request_.get());

  if (!etag_value.empty()) {
    if (partial_.get() && !partial_->IsCurrentRangeCached()) {
      // We don't want to switch to WRITE mode if we don't have this block of a
      // byte-range request because we may have other parts cached.
      custom_request_->extra_headers.append("If-Range: ");
    } else {
      custom_request_->extra_headers.append("If-None-Match: ");
    }
    custom_request_->extra_headers.append(etag_value);
    custom_request_->extra_headers.append("\r\n");
    if (partial_.get() && partial_->IsCurrentRangeCached())
      return true;
  }

  if (!last_modified_value.empty()) {
    if (partial_.get() && !partial_->IsCurrentRangeCached()) {
      custom_request_->extra_headers.append("If-Range: ");
    } else {
      custom_request_->extra_headers.append("If-Modified-Since: ");
    }
    custom_request_->extra_headers.append(last_modified_value);
    custom_request_->extra_headers.append("\r\n");
  }

  return true;
}

// We just received some headers from the server. We may have asked for a range,
// in which case partial_ has an object. This could be the first network request
// we make to fulfill the original request, or we may be already reading (from
// the net and / or the cache). If we are not expecting a certain response, we
// just bypass the cache for this request (but again, maybe we are reading), and
// delete partial_ (so we are not able to "fix" the headers that we return to
// the user). This results in either a weird response for the caller (we don't
// expect it after all), or maybe a range that was not exactly what it was asked
// for.
//
// If the server is simply telling us that the resource has changed, we delete
// the cached entry and restart the request as the caller intended (by returning
// false from this method). However, we may not be able to do that at any point,
// for instance if we already returned the headers to the user.
//
// WARNING: Whenever this code returns false, it has to make sure that the next
// time it is called it will return true so that we don't keep retrying the
// request.
bool HttpCache::Transaction::ValidatePartialResponse(
    const HttpResponseHeaders* headers, bool* partial_content) {
  int response_code = headers->response_code();
  bool partial_response = enable_range_support_ ? response_code == 206 : false;
  *partial_content = false;

  if (!entry_)
    return true;

  if (invalid_range_) {
    // We gave up trying to match this request with the stored data. If the
    // server is ok with the request, delete the entry, otherwise just ignore
    // this request
    if (partial_response || response_code == 200 || response_code == 304) {
      DoomPartialEntry(true);
      mode_ = NONE;
    } else {
      IgnoreRangeRequest();
    }
    return true;
  }

  if (!partial_.get()) {
    // We are not expecting 206 but we may have one.
    if (partial_response)
      IgnoreRangeRequest();

    return true;
  }

  // TODO(rvargas): Do we need to consider other results here?.
  bool failure = response_code == 200 || response_code == 416;

  if (partial_->IsCurrentRangeCached()) {
    // We asked for "If-None-Match: " so a 206 means a new object.
    if (partial_response)
      failure = true;

    if (response_code == 304 && partial_->ResponseHeadersOK(headers))
      return true;
  } else {
    // We asked for "If-Range: " so a 206 means just another range.
    if (partial_response && partial_->ResponseHeadersOK(headers)) {
      *partial_content = true;
      return true;
    }

    // 304 is not expected here, but we'll spare the entry.
  }

  if (failure) {
    // We cannot truncate this entry, it has to be deleted.
    DoomPartialEntry(false);
    mode_ = NONE;
    if (!reading_ && !partial_->IsLastRange()) {
      // We'll attempt to issue another network request, this time without us
      // messing up the headers.
      partial_->RestoreHeaders(&custom_request_->extra_headers);
      partial_.reset();
      truncated_ = false;
      return false;
    }
    LOG(WARNING) << "Failed to revalidate partial entry";
    partial_.reset();
    return true;
  }

  IgnoreRangeRequest();
  return true;
}

void HttpCache::Transaction::IgnoreRangeRequest() {
  // We have a problem. We may or may not be reading already (in which case we
  // returned the headers), but we'll just pretend that this request is not
  // using the cache and see what happens. Most likely this is the first
  // response from the server (it's not changing its mind midway, right?).
  if (mode_ & WRITE) {
    DoneWritingToEntry(mode_ != WRITE);
  } else if (mode_ & READ && entry_) {
    cache_->DoneReadingFromEntry(entry_, this);
  }

  partial_.reset(NULL);
  entry_ = NULL;
  mode_ = NONE;
}

int HttpCache::Transaction::ReadFromNetwork(IOBuffer* data, int data_len) {
  int rv = network_trans_->Read(data, data_len, &network_read_callback_);
  read_buf_ = data;
  read_buf_len_ = data_len;
  if (rv >= 0)
    rv = DoNetworkReadCompleted(rv);
  return rv;
}

int HttpCache::Transaction::ReadFromEntry(IOBuffer* data, int data_len) {
  DCHECK(entry_);
  int rv;
  cache_read_callback_->AddRef();  // Balanced in OnCacheReadCompleted.
  if (partial_.get()) {
    rv = partial_->CacheRead(entry_->disk_entry, data, data_len,
                            cache_read_callback_);
  } else {
    rv = entry_->disk_entry->ReadData(kResponseContentIndex, read_offset_,
                                      data, data_len, cache_read_callback_);
  }
  read_buf_ = data;
  read_buf_len_ = data_len;
  if (rv != ERR_IO_PENDING)
    cache_read_callback_->Release();

  if (rv >= 0)
    rv = DoCacheReadCompleted(rv);

  return rv;
}

int HttpCache::Transaction::ReadResponseInfoFromEntry() {
  DCHECK(entry_);

  LoadLog::BeginEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_READ_INFO);
  bool read_ok =
      HttpCache::ReadResponseInfo(entry_->disk_entry, &response_, &truncated_);
  LoadLog::EndEvent(load_log_, LoadLog::TYPE_HTTP_CACHE_READ_INFO);

  return read_ok ? OK : ERR_CACHE_READ_FAILURE;
}

int HttpCache::Transaction::WriteToEntry(int index, int offset,
                                         IOBuffer* data, int data_len,
                                         CompletionCallback* callback) {
  if (!entry_)
    return data_len;

  int rv = 0;
  if (!partial_.get() || !data_len) {
    rv = entry_->disk_entry->WriteData(index, offset, data, data_len, callback,
                                       true);
  } else {
    rv = partial_->CacheWrite(entry_->disk_entry, data, data_len, callback);
  }

  if (rv != ERR_IO_PENDING && rv != data_len) {
    DLOG(ERROR) << "failed to write response data to cache";
    DoneWritingToEntry(false);

    // We want to ignore errors writing to disk and just keep reading from
    // the network.
    rv = data_len;
  }
  return rv;
}

void HttpCache::Transaction::WriteResponseInfoToEntry(bool truncated) {
  if (!entry_)
    return;

  // Do not cache no-store content (unless we are record mode).  Do not cache
  // content with cert errors either.  This is to prevent not reporting net
  // errors when loading a resource from the cache.  When we load a page over
  // HTTPS with a cert error we show an SSL blocking page.  If the user clicks
  // proceed we reload the resource ignoring the errors.  The loaded resource
  // is then cached.  If that resource is subsequently loaded from the cache,
  // no net error is reported (even though the cert status contains the actual
  // errors) and no SSL blocking page is shown.  An alternative would be to
  // reverse-map the cert status to a net error and replay the net error.
  if ((cache_->mode() != RECORD &&
      response_.headers->HasHeaderValue("cache-control", "no-store")) ||
      net::IsCertStatusError(response_.ssl_info.cert_status)) {
    DoneWritingToEntry(false);
    return;
  }

  // When writing headers, we normally only write the non-transient
  // headers; when in record mode, record everything.
  bool skip_transient_headers = (cache_->mode() != RECORD);

  if (truncated) {
    DCHECK_EQ(200, response_.headers->response_code());
  }

  if (!HttpCache::WriteResponseInfo(entry_->disk_entry, &response_,
                                    skip_transient_headers, truncated)) {
    DLOG(ERROR) << "failed to write response info to cache";
    DoneWritingToEntry(false);
  }
}

int HttpCache::Transaction::AppendResponseDataToEntry(
    IOBuffer* data, int data_len, CompletionCallback* callback) {
  if (!entry_ || !data_len)
    return data_len;

  int current_size = entry_->disk_entry->GetDataSize(kResponseContentIndex);
  return WriteToEntry(kResponseContentIndex, current_size, data, data_len,
                      callback);
}

void HttpCache::Transaction::TruncateResponseData() {
  if (!entry_)
    return;

  // Truncate the stream.
  int rv = WriteToEntry(kResponseContentIndex, 0, NULL, 0, NULL);
  DCHECK(rv != ERR_IO_PENDING);
}

void HttpCache::Transaction::DoneWritingToEntry(bool success) {
  if (!entry_)
    return;

  if (cache_->mode() == RECORD)
    DLOG(INFO) << "Recorded: " << request_->method << request_->url
               << " status: " << response_.headers->response_code();

  cache_->DoneWritingToEntry(entry_, success);
  entry_ = NULL;
  mode_ = NONE;  // switch to 'pass through' mode
}

void HttpCache::Transaction::DoomPartialEntry(bool delete_object) {
  cache_->DoneWithEntry(entry_, this, false);
  cache_->DoomEntry(cache_key_);
  entry_ = NULL;
  if (delete_object)
    partial_.reset(NULL);
}

int HttpCache::Transaction::DoNetworkReadCompleted(int result) {
  DCHECK(mode_ & WRITE || mode_ == NONE);

  if (!cache_)
    return HandleResult(ERR_UNEXPECTED);

  cache_write_callback_->AddRef();  // Balanced in DoCacheWriteCompleted.

  result = AppendResponseDataToEntry(read_buf_, result, cache_write_callback_);
  if (result == ERR_IO_PENDING)
    return result;

  return DoCacheWriteCompleted(result);
}

int HttpCache::Transaction::DoPartialNetworkReadCompleted(int result) {
  partial_->OnNetworkReadCompleted(result);

  if (result == 0) {  // End of file.
    if (mode_ == READ_WRITE) {
      // We need to move on to the next range.
      network_trans_.reset();
      result = ContinuePartialCacheValidation();
      if (result != OK)
        // Any error was already handled.
        return result;
    }
    DoneWritingToEntry(true);
  }
  return HandleResult(result);
}

int HttpCache::Transaction::DoCacheReadCompleted(int result) {
  DCHECK(cache_);

  if (!cache_)
    return HandleResult(ERR_UNEXPECTED);

  if (partial_.get())
    return DoPartialCacheReadCompleted(result);

  if (result > 0) {
    read_offset_ += result;
  } else if (result == 0) {  // End of file.
    cache_->DoneReadingFromEntry(entry_, this);
    entry_ = NULL;
  }
  return HandleResult(result);
}

int HttpCache::Transaction::DoPartialCacheReadCompleted(int result) {
  partial_->OnCacheReadCompleted(result);

  if (result == 0) {  // End of file.
    if (partial_.get() && mode_ == READ_WRITE) {
      // We need to move on to the next range.
      result = ContinuePartialCacheValidation();
      if (result != OK || !entry_) {
        // Any error was already handled.
        return result;
      }
      cache_->ConvertWriterToReader(entry_);
    }
    cache_->DoneReadingFromEntry(entry_, this);
    entry_ = NULL;
  }
  return HandleResult(result);
}

int HttpCache::Transaction::DoCacheWriteCompleted(int result) {
  DCHECK(cache_);
  // Balance the AddRef from DoNetworkReadCompleted.
  cache_write_callback_->Release();
  if (!cache_)
    return HandleResult(ERR_UNEXPECTED);

  if (result < 0)
    return HandleResult(result);

  if (partial_.get())
    return DoPartialNetworkReadCompleted(result);

  if (result == 0)  // End of file.
    DoneWritingToEntry(true);

  return HandleResult(result);
}

void HttpCache::Transaction::OnNetworkInfoAvailable(int result) {
  DCHECK(result != ERR_IO_PENDING);

  if (!cache_) {
    HandleResult(ERR_UNEXPECTED);
    return;
  }

  if (result == OK) {
    const HttpResponseInfo* new_response = network_trans_->GetResponseInfo();
    if (new_response->headers->response_code() == 401 ||
        new_response->headers->response_code() == 407) {
      auth_response_ = *new_response;
    } else {
      bool partial_content;
      if (!ValidatePartialResponse(new_response->headers, &partial_content) &&
          !auth_response_.headers && callback_) {
        // Something went wrong with this request and we have to restart it.
        // If there is no callback we'll return OK to the caller so we cannot
        // restart the request. If we have an authentication response, we are
        // exposed to weird things hapenning if the user cancels the
        // authentication before we receive the new response.
        network_trans_.reset();
        response_ = HttpResponseInfo();
        BeginNetworkRequest();
        return;
      }
      if (partial_content && mode_ == READ_WRITE && !truncated_ &&
          response_.headers->response_code() == 200) {
        // We have stored the full entry, but it changed and the server is
        // sending a range. We have to delete the old entry.
        DoneWritingToEntry(false);
      }

      // Are we expecting a response to a conditional query?
      if (mode_ == READ_WRITE || mode_ == UPDATE) {
        if (new_response->headers->response_code() == 304 || partial_content) {
          // Update cached response based on headers in new_response.
          // TODO(wtc): should we update cached certificate
          // (response_.ssl_info), too?
          response_.headers->Update(*new_response->headers);
          response_.response_time = new_response->response_time;
          response_.request_time = new_response->request_time;

          if (response_.headers->HasHeaderValue("cache-control", "no-store")) {
            cache_->DoomEntry(cache_key_);
          } else {
            // If we are already reading, we already updated the headers for
            // this request; doing it again will change Content-Length.
            if (!reading_)
              WriteResponseInfoToEntry(false);
          }

          if (mode_ == UPDATE) {
            DCHECK(!partial_content);
            // We got a "not modified" response and already updated the
            // corresponding cache entry above.
            //
            // By closing the cached entry now, we make sure that the
            // 304 rather than the cached 200 response, is what will be
            // returned to the user.
            DoneWritingToEntry(true);
          } else if (entry_ && !partial_content) {
            DCHECK_EQ(READ_WRITE, mode_);
            if (!partial_.get() || partial_->IsLastRange()) {
              cache_->ConvertWriterToReader(entry_);
              mode_ = READ;
            }
            // We no longer need the network transaction, so destroy it.
            final_upload_progress_ = network_trans_->GetUploadProgress();
            network_trans_.reset();
          }
        } else {
          mode_ = WRITE;
        }
      }

      if (!(mode_ & READ)) {
        // We change the value of Content-Length for partial content.
        if (partial_content && partial_.get())
          partial_->FixContentLength(new_response->headers);

        response_ = *new_response;
        WriteResponseInfoToEntry(truncated_);

        // Truncate response data.
        TruncateResponseData();

        // If this response is a redirect, then we can stop writing now.  (We
        // don't need to cache the response body of a redirect.)
        if (response_.headers->IsRedirect(NULL))
          DoneWritingToEntry(true);
      }
      if (reading_ && partial_.get()) {
        if (network_trans_.get()) {
          result = ReadFromNetwork(read_buf_, read_buf_len_);
        } else {
          result = ReadFromEntry(read_buf_, read_buf_len_);
        }
        if (result >= 0 || result == net::ERR_IO_PENDING)
          return;
      } else if (mode_ != NONE && partial_.get()) {
        // We are about to return the headers for a byte-range request to the
        // user, so let's fix them.
        partial_->FixResponseHeaders(response_.headers);
      }
    }
  } else if (IsCertificateError(result)) {
    const HttpResponseInfo* response = network_trans_->GetResponseInfo();
    // If we get a certificate error, then there is a certificate in ssl_info,
    // so GetResponseInfo() should never returns NULL here.
    DCHECK(response);
    response_.ssl_info = response->ssl_info;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    const HttpResponseInfo* response = network_trans_->GetResponseInfo();
    DCHECK(response);
    response_.cert_request_info = response->cert_request_info;
  }
  HandleResult(result);
}

void HttpCache::Transaction::OnNetworkReadCompleted(int result) {
  DoNetworkReadCompleted(result);
}

void HttpCache::Transaction::OnCacheReadCompleted(int result) {
  cache_read_callback_->Release();  // Balance the AddRef from ReadFromEntry.
  DoCacheReadCompleted(result);
}

void HttpCache::Transaction::OnCacheWriteCompleted(int result) {
  DoCacheWriteCompleted(result);
}

void HttpCache::Transaction::OnCacheEntryReady(int result) {
  DCHECK_EQ(OK, result);
  ValidateEntryHeadersAndContinue(true);
}

}  // namespace net
