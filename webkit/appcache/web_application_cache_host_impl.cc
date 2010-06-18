// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/web_application_cache_host_impl.h"

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebURLRequest;
using WebKit::WebURL;
using WebKit::WebURLResponse;

namespace appcache {

static IDMap<WebApplicationCacheHostImpl> all_hosts;

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromId(int id) {
  return all_hosts.Lookup(id);
}

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromFrame(
    WebFrame* frame) {
  if (!frame)
    return NULL;
  WebDataSource* data_source = frame->dataSource();
  if (!data_source)
    return NULL;
  return static_cast<WebApplicationCacheHostImpl*>
      (data_source->applicationCacheHost());
}

WebApplicationCacheHostImpl::WebApplicationCacheHostImpl(
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : client_(client),
      backend_(backend),
      ALLOW_THIS_IN_INITIALIZER_LIST(host_id_(all_hosts.Add(this))),
      has_status_(false),
      status_(UNCACHED),
      has_cached_status_(false),
      cached_status_(UNCACHED),
      is_scheme_supported_(false),
      is_get_method_(false),
      is_new_master_entry_(MAYBE) {
  DCHECK(client && backend && (host_id_ != kNoHostId));

  backend_->RegisterHost(host_id_);
}

WebApplicationCacheHostImpl::~WebApplicationCacheHostImpl() {
  backend_->UnregisterHost(host_id_);
  all_hosts.Remove(host_id_);
}

void WebApplicationCacheHostImpl::OnCacheSelected(int64 selected_cache_id,
                                                  appcache::Status status) {
  status_ = status;
  has_status_ = true;
}

void WebApplicationCacheHostImpl::OnStatusChanged(appcache::Status status) {
  if (has_status_)
    status_ = status;
}

void WebApplicationCacheHostImpl::OnEventRaised(appcache::EventID event_id) {
  DCHECK(event_id != PROGRESS_EVENT);  // See OnProgressEventRaised.
  // Most events change the status. Clear out what we know so that the latest
  // status will be obtained from the backend.
  has_status_ = false;
  has_cached_status_ = false;
  client_->notifyEventListener(static_cast<EventID>(event_id));
}

void WebApplicationCacheHostImpl::OnProgressEventRaised(
    const GURL& url, int num_total, int num_complete) {
  // TODO(michaeln): Widen the webkit api to accept the additional data.
  // Also send the 'final' event once webkit layout tests have been updated.
  // See https://bugs.webkit.org/show_bug.cgi?id=37602
  if (num_complete < num_total)
    client_->notifyEventListener(WebApplicationCacheHost::ProgressEvent);
}

void WebApplicationCacheHostImpl::OnLogMessage(
    LogLevel log_level, const std::string& message) {
  // TODO(michaeln): Widen the webkit api with this addition.
  // client_->notifyLogMessage(log_level, message);
}

void WebApplicationCacheHostImpl::willStartMainResourceRequest(
    WebURLRequest& request) {
  request.setAppCacheHostID(host_id_);
  std::string method = request.httpMethod().utf8();
  is_get_method_ = (method == kHttpGETMethod);
  DCHECK(method == StringToUpperASCII(method));
}

void WebApplicationCacheHostImpl::willStartSubResourceRequest(
    WebURLRequest& request) {
  request.setAppCacheHostID(host_id_);
}

void WebApplicationCacheHostImpl::selectCacheWithoutManifest() {
  // Reset any previous status values we've received from the backend
  // since we're now selecting a new cache.
  has_status_ = false;
  has_cached_status_ = false;
  is_new_master_entry_ = NO;
  backend_->SelectCache(host_id_, document_url_,
                        document_response_.appCacheID(),
                        GURL());
}

bool WebApplicationCacheHostImpl::selectCacheWithManifest(
    const WebURL& manifest_url) {
  // Reset any previous status values we've received from the backend
  // since we're now selecting a new cache.
  has_status_ = false;
  has_cached_status_ = false;

  GURL manifest_gurl(manifest_url);
  if (manifest_gurl.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    manifest_gurl = manifest_gurl.ReplaceComponents(replacements);
  }

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.appCacheID() == kNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        (manifest_gurl.GetOrigin() == document_url_.GetOrigin())) {
      is_new_master_entry_ = YES;
    } else {
      is_new_master_entry_ = NO;
      manifest_gurl = GURL();
    }
    backend_->SelectCache(host_id_, document_url_,
                          kNoCacheId, manifest_gurl);
    return true;
  }

  DCHECK(is_new_master_entry_ = NO);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  GURL document_manifest_gurl(document_response_.appCacheManifestURL());
  if (document_manifest_gurl != manifest_gurl) {
    backend_->MarkAsForeignEntry(host_id_, document_url_,
                                 document_response_.appCacheID());
    has_cached_status_ = true;
    cached_status_ = UNCACHED;
    return false;  // the navigation will be restarted
  }

  // Its a 'master' entry thats already in the cache.
  backend_->SelectCache(host_id_, document_url_,
                        document_response_.appCacheID(),
                        manifest_gurl);
  return true;
}

void WebApplicationCacheHostImpl::didReceiveResponseForMainResource(
    const WebURLResponse& response) {
  document_response_ = response;
  document_url_ = document_response_.url();
  if (document_url_.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    document_url_ = document_url_.ReplaceComponents(replacements);
  }
  is_scheme_supported_ =  IsSchemeSupported(document_url_);
  if ((document_response_.appCacheID() != kNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = NO;
}

void WebApplicationCacheHostImpl::didReceiveDataForMainResource(
    const char* data, int len) {
  if (is_new_master_entry_ == NO)
    return;
  // TODO(michaeln): write me
}

void WebApplicationCacheHostImpl::didFinishLoadingMainResource(bool success) {
  if (is_new_master_entry_ == NO)
    return;
  // TODO(michaeln): write me
}

WebApplicationCacheHost::Status WebApplicationCacheHostImpl::status() {
  // We're careful about the status value to avoid race conditions.
  //
  // Generally the webappcachehost sends an async stream of messages to the
  // backend, and receives an asyncronous stream of events from the backend.
  // In the backend, all operations are serialized and as state changes
  // 'events' are streamed out to relevant parties. In particular the
  // 'SelectCache' message is async. Regular page loading and navigation
  // involves two non-blocking ipc calls: RegisterHost + SelectCache.
  //
  // However, the page can call the scriptable API in advance of a cache
  // selection being complete (and/or in advance of the webappcachehost having
  // received the event about completion). In that case, we force an end-to-end
  // fetch of the 'status' value, and cache that value seperately from the
  // value we receive via the async event stream. We'll use that cached value
  // until cache selection is complete.
  if (has_status_)
    return static_cast<WebApplicationCacheHost::Status>(status_);

  if (!has_cached_status_) {
    cached_status_ = backend_->GetStatus(host_id_);
    has_cached_status_ = true;
  }
  return static_cast<WebApplicationCacheHost::Status>(cached_status_);
}

bool WebApplicationCacheHostImpl::startUpdate() {
  return backend_->StartUpdate(host_id_);
}

bool WebApplicationCacheHostImpl::swapCache() {
  // Cache status will change when cache is swapped. Clear out any saved idea
  // of status so that backend will be queried for actual status.
  has_status_ = false;
  has_cached_status_ = false;
  return backend_->SwapCache(host_id_);
}

}  // appcache namespace
