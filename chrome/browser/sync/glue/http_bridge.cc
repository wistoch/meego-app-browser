// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/sync/glue/http_bridge.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "net/base/cookie_monster.h"
#include "net/base/load_flags.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/webkit_glue.h"

namespace browser_sync {

HttpBridge::RequestContext* HttpBridgeFactory::GetRequestContext() {
  if (!request_context_) {
    request_context_ =
        new HttpBridge::RequestContext(Profile::GetDefaultRequestContext());
    request_context_->AddRef();
  }
  return request_context_;
}

HttpBridgeFactory::~HttpBridgeFactory() {
  if (request_context_) {
    // Clean up request context on IO thread.
    ChromeThread::GetMessageLoop(ChromeThread::IO)->ReleaseSoon(FROM_HERE,
        request_context_);
    request_context_ = NULL;
  }
}

sync_api::HttpPostProviderInterface* HttpBridgeFactory::Create() {
  // TODO(timsteele): We want the active profile request context.
  HttpBridge* http = new HttpBridge(GetRequestContext(),
      ChromeThread::GetMessageLoop(ChromeThread::IO));
  http->AddRef();
  return http;
}

void HttpBridgeFactory::Destroy(sync_api::HttpPostProviderInterface* http) {
  static_cast<HttpBridge*>(http)->Release();
}

HttpBridge::RequestContext::RequestContext(
    const URLRequestContext* baseline_context) {

  // Create empty, in-memory cookie store.
  cookie_store_ = new net::CookieMonster();

  // We don't use a cache for bridged loads, but we do want to share proxy info.
  host_resolver_ = baseline_context->host_resolver();
  proxy_service_ = baseline_context->proxy_service();
  ssl_config_service_ = baseline_context->ssl_config_service();
  http_transaction_factory_ =
      net::HttpNetworkLayer::CreateFactory(host_resolver_, proxy_service_,
                                           ssl_config_service_);

  // TODO(timsteele): We don't currently listen for pref changes of these
  // fields or CookiePolicy; I'm not sure we want to strictly follow the
  // default settings, since for example if the user chooses to block all
  // cookies, sync will start failing. Also it seems like accept_lang/charset
  // should be tied to whatever the sync servers expect (if anything). These
  // fields should probably just be settable by sync backend; though we should
  // figure out if we need to give the user explicit control over policies etc.
  accept_language_ = baseline_context->accept_language();
  accept_charset_ = baseline_context->accept_charset();

  // We default to the browser's user agent. This can (and should) be overridden
  // with set_user_agent.
  user_agent_ = webkit_glue::GetUserAgent(GURL());
}

HttpBridge::RequestContext::~RequestContext() {
  delete cookie_store_;
  delete http_transaction_factory_;
}

HttpBridge::HttpBridge(HttpBridge::RequestContext* context,
                       MessageLoop* io_loop)
    : context_for_request_(context),
      url_poster_(NULL),
      created_on_loop_(MessageLoop::current()),
      io_loop_(io_loop),
      request_completed_(false),
      request_succeeded_(false),
      http_response_code_(-1),
      http_post_completed_(false, false),
      use_io_loop_for_testing_(false) {
  context_for_request_->AddRef();
}

HttpBridge::~HttpBridge() {
  io_loop_->ReleaseSoon(FROM_HERE, context_for_request_);
}

void HttpBridge::SetUserAgent(const char* user_agent) {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(!request_completed_);
  context_for_request_->set_user_agent(user_agent);
}

void HttpBridge::SetURL(const char* url, int port) {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(!request_completed_);
  DCHECK(url_for_request_.is_empty())
      << "HttpBridge::SetURL called more than once?!";
  GURL temp(url);
  GURL::Replacements replacements;
  std::string port_str = IntToString(port);
  replacements.SetPort(port_str.c_str(),
                       url_parse::Component(0, port_str.length()));
  url_for_request_ = temp.ReplaceComponents(replacements);
}

void HttpBridge::SetPostPayload(const char* content_type,
                                int content_length,
                                const char* content) {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(!request_completed_);
  DCHECK(content_type_.empty()) << "Bridge payload already set.";
  DCHECK_GE(content_length, 0) << "Content length < 0";
  content_type_ = content_type;
  if (!content || (content_length == 0)) {
    DCHECK_EQ(content_length, 0);
    request_content_ = " ";  // TODO(timsteele): URLFetcher requires non-empty
                             // content for POSTs whereas CURL does not, for now
                             // we hack this to support the sync backend.
  } else {
    request_content_.assign(content, content_length);
  }
}

void HttpBridge::AddCookieForRequest(const char* cookie) {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(!request_completed_);
  DCHECK(url_for_request_.is_valid()) << "Valid URL not set.";
  if (!url_for_request_.is_valid()) return;

  if (!context_for_request_->cookie_store()->SetCookie(url_for_request_,
                                                       cookie)) {
    DLOG(WARNING) << "Cookie " << cookie
                  << " could not be added for url: " << url_for_request_ << ".";
  }
}

bool HttpBridge::MakeSynchronousPost(int* os_error_code, int* response_code) {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(!request_completed_);
  DCHECK(url_for_request_.is_valid()) << "Invalid URL for request";
  DCHECK(!content_type_.empty()) << "Payload not set";
  DCHECK(context_for_request_->is_user_agent_set()) << "User agent not set";

  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
                                &HttpBridge::CallMakeAsynchronousPost));

  if (!http_post_completed_.Wait())  // Block until network request completes.
    NOTREACHED();                    // See OnURLFetchComplete.

  DCHECK(request_completed_);
  *os_error_code = os_error_code_;
  *response_code = http_response_code_;
  return request_succeeded_;
}

void HttpBridge::MakeAsynchronousPost() {
  DCHECK_EQ(MessageLoop::current(), io_loop_);
  DCHECK(!request_completed_);

  url_poster_ = new URLFetcher(url_for_request_, URLFetcher::POST, this);
  url_poster_->set_request_context(context_for_request_);
  url_poster_->set_upload_data(content_type_, request_content_);

  if (use_io_loop_for_testing_)
    url_poster_->set_io_loop(io_loop_);

  url_poster_->Start();
}

int HttpBridge::GetResponseContentLength() const {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(request_completed_);
  return response_content_.size();
}

const char* HttpBridge::GetResponseContent() const {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(request_completed_);
  return response_content_.c_str();
}

int HttpBridge::GetResponseCookieCount() const {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(request_completed_);
  return response_cookies_.size();
}

const char* HttpBridge::GetResponseCookieAt(int cookie_number) const {
  DCHECK_EQ(MessageLoop::current(), created_on_loop_);
  DCHECK(request_completed_);
  bool valid_number = (cookie_number >= 0) &&
      (static_cast<size_t>(cookie_number) < response_cookies_.size());
  DCHECK(valid_number);
  if (!valid_number)
    return NULL;
  return response_cookies_[cookie_number].c_str();
}

void HttpBridge::OnURLFetchComplete(const URLFetcher *source, const GURL &url,
                                    const URLRequestStatus &status,
                                    int response_code,
                                    const ResponseCookies &cookies,
                                    const std::string &data) {
  DCHECK_EQ(MessageLoop::current(), io_loop_);

  request_completed_ = true;
  request_succeeded_ = (URLRequestStatus::SUCCESS == status.status());
  http_response_code_ = response_code;
  os_error_code_ = status.os_error();

  // TODO(timsteele): For now we need this "fixup" to match up with what the
  // sync backend expects. This seems to be non-standard and shouldn't be done
  // here in HttpBridge, and it breaks part of the unittest.
  for (size_t i = 0; i < cookies.size(); ++i) {
    net::CookieMonster::ParsedCookie parsed_cookie(cookies[i]);
    std::string cookie = " \t \t \t \t \t";
    cookie += parsed_cookie.Name() + "\t";
    cookie += parsed_cookie.Value();
    response_cookies_.push_back(cookie);
  }

  response_content_ = data;

  // End of the line for url_poster_. It lives only on the io_loop.
  // We defer deletion because we're inside a callback from a component of the
  // URLFetcher, so it seems most natural / "polite" to let the stack unwind.
  io_loop_->DeleteSoon(FROM_HERE, url_poster_);
  url_poster_ = NULL;

  // Wake the blocked syncer thread in MakeSynchronousPost.
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
  http_post_completed_.Signal();
}

}  // namespace browser_sync

#endif  // CHROME_PERSONALIZATION
