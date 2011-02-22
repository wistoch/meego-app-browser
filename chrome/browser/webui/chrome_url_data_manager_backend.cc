// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/chrome_url_data_manager_backend.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted_memory.h"
#include "base/string_util.h"
#include "chrome/browser/appcache/view_appcache_internals_job_factory.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/shared_resources_data_source.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/view_blob_internals_job_factory.h"
#include "chrome/browser/net/view_http_cache_job_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "grit/platform_locale_settings.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job.h"

static ChromeURLDataManagerBackend* GetBackend(net::URLRequest* request) {
  return static_cast<ChromeURLRequestContext*>(request->context())->
      GetChromeURLDataManagerBackend();
}

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob {
 public:
  explicit URLRequestChromeJob(net::URLRequest* request);

  // net::URLRequestJob implementation.
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.
  void DataAvailable(RefCountedMemory* bytes);

  void SetMimeType(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

 private:
  virtual ~URLRequestChromeJob();

  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most net::URLRequestJob implementations.)
  void StartAsync();

  // Do the actual copy from data_ (the data we're serving) into |buf|.
  // Separate from ReadRawData so we can handle async I/O.
  void CompleteRead(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<RefCountedMemory> data_;
  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // The backend is owned by ChromeURLRequestContext and always outlives us.
  ChromeURLDataManagerBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeJob);
};

// URLRequestChromeFileJob is a net::URLRequestJob that acts like a file:// URL
class URLRequestChromeFileJob : public net::URLRequestFileJob {
 public:
  URLRequestChromeFileJob(net::URLRequest* request, const FilePath& path);

 private:
  virtual ~URLRequestChromeFileJob();

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeFileJob);
};

void ChromeURLDataManagerBackend::URLToRequest(const GURL& url,
                                               std::string* source_name,
                                               std::string* path) {
  DCHECK(url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme));

  if (!url.is_valid()) {
    NOTREACHED();
    return;
  }

  // Our input looks like: chrome://source_name/extra_bits?foo .
  // So the url's "host" is our source, and everything after the host is
  // the path.
  source_name->assign(url.host());

  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url_parse::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

bool ChromeURLDataManagerBackend::URLToFilePath(const GURL& url,
                                                FilePath* file_path) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string relative_path;

  // Remove Query and Ref from URL.
  GURL stripped_url;
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  stripped_url = url.ReplaceComponents(replacements);

  URLToRequest(stripped_url, &source_name, &relative_path);

  FileSourceMap::const_iterator i(file_sources_.find(source_name));
  if (i == file_sources_.end())
    return false;

  // Check that |relative_path| is not an absolute path (otherwise AppendASCII()
  // will DCHECK). The awkward use of StringType is because on some systems
  // FilePath expects a std::string, but on others a std::wstring.
  FilePath p(FilePath::StringType(relative_path.begin(), relative_path.end()));
  if (p.IsAbsolute())
    return false;

  *file_path = i->second.AppendASCII(relative_path);

  return true;
}

ChromeURLDataManagerBackend::ChromeURLDataManagerBackend()
    : next_request_id_(0) {
  FilePath inspector_dir;
  if (PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir))
    AddFileSource(chrome::kChromeUIDevToolsHost, inspector_dir);
  AddDataSource(new SharedResourcesDataSource());
}

ChromeURLDataManagerBackend::~ChromeURLDataManagerBackend() {
  for (DataSourceMap::iterator i = data_sources_.begin();
       i != data_sources_.end(); ++i) {
    i->second->backend_ = NULL;
  }
  data_sources_.clear();
}

// static
void ChromeURLDataManagerBackend::Register() {
  net::URLRequest::RegisterProtocolFactory(
      chrome::kChromeDevToolsScheme,
      &ChromeURLDataManagerBackend::Factory);
  net::URLRequest::RegisterProtocolFactory(
      chrome::kChromeUIScheme,
      &ChromeURLDataManagerBackend::Factory);
}

void ChromeURLDataManagerBackend::AddDataSource(
    ChromeURLDataManager::DataSource* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DataSourceMap::iterator i = data_sources_.find(source->source_name());
  if (i != data_sources_.end())
    i->second->backend_ = NULL;
  data_sources_[source->source_name()] = source;
  source->backend_ = this;
}

void ChromeURLDataManagerBackend::AddFileSource(const std::string& source_name,
                                                const FilePath& file_path) {
  DCHECK(file_sources_.count(source_name) == 0);
  file_sources_[source_name] = file_path;
}

bool ChromeURLDataManagerBackend::HasPendingJob(
    URLRequestChromeJob* job) const {
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool ChromeURLDataManagerBackend::StartRequest(const GURL& url,
                                               URLRequestChromeJob* job) {
  // Parse the URL into a request for a source and path.
  std::string source_name;
  std::string path;
  URLToRequest(url, &source_name, &path);

  // Look up the data source for the request.
  DataSourceMap::iterator i = data_sources_.find(source_name);
  if (i == data_sources_.end())
    return false;

  ChromeURLDataManager::DataSource* source = i->second;

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  // TODO(eroman): would be nicer if the mimetype were set at the same time
  // as the data blob. For now do it here, since NotifyHeadersComplete() is
  // going to get called once we return.
  job->SetMimeType(source->GetMimeType(path));

  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
      job->request()->context());

  // Forward along the request to the data source.
  MessageLoop* target_message_loop = source->MessageLoopForRequestPath(path);
  if (!target_message_loop) {
    // The DataSource is agnostic to which thread StartDataRequest is called
    // on for this path.  Call directly into it from this thread, the IO
    // thread.
    source->StartDataRequest(path, context->is_off_the_record(), request_id);
  } else {
    // The DataSource wants StartDataRequest to be called on a specific thread,
    // usually the UI thread, for this path.
    target_message_loop->PostTask(
        FROM_HERE,
        NewRunnableMethod(source,
                          &ChromeURLDataManager::DataSource::StartDataRequest,
                          path, context->is_off_the_record(), request_id));
  }
  return true;
}

void ChromeURLDataManagerBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void ChromeURLDataManagerBackend::DataAvailable(RequestID request_id,
                                                RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    // We acquire a reference to the job so that it doesn't disappear under the
    // feet of any method invoked here (we could trigger a callback).
    scoped_refptr<URLRequestChromeJob> job(i->second);
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

// static
net::URLRequestJob* ChromeURLDataManagerBackend::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  // Try first with a file handler
  FilePath path;
  ChromeURLDataManagerBackend* backend = GetBackend(request);
  if (backend->URLToFilePath(request->url(), &path))
    return new URLRequestChromeFileJob(request, path);

  // Next check for chrome://view-http-cache/*, which uses its own job type.
  if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
    return ViewHttpCacheJobFactory::CreateJobForRequest(request);

  // Next check for chrome://appcache-internals/, which uses its own job type.
  if (ViewAppCacheInternalsJobFactory::IsSupportedURL(request->url()))
    return ViewAppCacheInternalsJobFactory::CreateJobForRequest(request);

  // Next check for chrome://blob-internals/, which uses its own job type.
  if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url()))
    return ViewBlobInternalsJobFactory::CreateJobForRequest(request);

  // Fall back to using a custom handler
  return new URLRequestChromeJob(request);
}

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request)
    : net::URLRequestJob(request),
      data_offset_(0),
      pending_buf_size_(0),
      backend_(GetBackend(request)) {
}

URLRequestChromeJob::~URLRequestChromeJob() {
  CHECK(!backend_->HasPendingJob(this));
}

void URLRequestChromeJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestChromeJob::StartAsync));
}

void URLRequestChromeJob::Kill() {
  backend_->RemoveRequest(this);
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

void URLRequestChromeJob::DataAvailable(RefCountedMemory* bytes) {
  if (bytes) {
    // The request completed, and we have all the data.
    // Clear any IO pending status.
    SetStatus(net::URLRequestStatus());

    data_ = bytes;
    int bytes_read;
    if (pending_buf_.get()) {
      CHECK(pending_buf_->data());
      CompleteRead(pending_buf_, pending_buf_size_, &bytes_read);
      pending_buf_ = NULL;
      NotifyReadComplete(bytes_read);
    }
  } else {
    // The request failed.
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_FAILED));
  }
}

bool URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  if (!data_.get()) {
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    DCHECK(!pending_buf_.get());
    CHECK(buf->data());
    pending_buf_ = buf;
    pending_buf_size_ = buf_size;
    return false;  // Tell the caller we're still waiting for data.
  }

  // Otherwise, the data is available.
  CompleteRead(buf, buf_size, bytes_read);
  return true;
}

void URLRequestChromeJob::CompleteRead(net::IOBuffer* buf, int buf_size,
                                       int* bytes_read) {
  int remaining = static_cast<int>(data_->size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  if (buf_size > 0) {
    memcpy(buf->data(), data_->front() + data_offset_, buf_size);
    data_offset_ += buf_size;
  }
  *bytes_read = buf_size;
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (backend_->StartRequest(request_->url(), this)) {
    NotifyHeadersComplete();
  } else {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
  }
}

URLRequestChromeFileJob::URLRequestChromeFileJob(net::URLRequest* request,
                                                 const FilePath& path)
    : net::URLRequestFileJob(request, path) {
}

URLRequestChromeFileJob::~URLRequestChromeFileJob() {}