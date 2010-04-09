// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_url_request.h"

#include <wininet.h>
#include <urlmon.h>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "chrome_frame/utils.h"
#include "net/http/http_util.h"
#include "net/http/http_response_headers.h"

UrlmonUrlRequest::UrlmonUrlRequest()
    : pending_read_size_(0),
      headers_received_(false),
      calling_delegate_(0),
      thread_(NULL),
      parent_window_(NULL),
      privileged_mode_(false) {
  DLOG(INFO) << StringPrintf("Created request. Obj: %X", this);
}

UrlmonUrlRequest::~UrlmonUrlRequest() {
  DLOG(INFO) << StringPrintf("Deleted request. Obj: %X", this);
}

bool UrlmonUrlRequest::Start() {
  thread_ = PlatformThread::CurrentId();
  status_.Start();
  // The UrlmonUrlRequest instance can get destroyed in the context of
  // StartAsyncDownload if BindToStorage finishes synchronously with an error.
  // Grab a reference to protect against this.
  scoped_refptr<UrlmonUrlRequest> ref(this);
  HRESULT hr = StartAsyncDownload();
  if (FAILED(hr) && status_.get_state() != UrlmonUrlRequest::Status::DONE) {
    status_.Done();
    status_.set_result(URLRequestStatus::FAILED, HresultToNetError(hr));
    NotifyDelegateAndDie();
  }
  return true;
}

void UrlmonUrlRequest::Stop() {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  DCHECK((status_.get_state() != Status::DONE) == (binding_ != NULL));
  Status::State state = status_.get_state();
  delegate_ = NULL;
  switch (state) {
    case Status::WORKING:
      status_.Cancel();
      if (binding_) {
        binding_->Abort();
      }
      break;

    case Status::ABORTING:
      status_.Cancel();
      break;

    case Status::DONE:
      status_.Cancel();
      NotifyDelegateAndDie();
      break;
  }
}

bool UrlmonUrlRequest::Read(int bytes_to_read) {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  DCHECK_GE(bytes_to_read, 0);
  DCHECK_EQ(0, calling_delegate_);
  // Re-entrancy check. Thou shall not call Read() while process OnReadComplete!
  DCHECK_EQ(0, pending_read_size_);
  if (pending_read_size_ != 0)
    return false;

  DCHECK((status_.get_state() != Status::DONE) == (binding_ != NULL));
  if (status_.get_state() == Status::ABORTING) {
    return true;
  }

  // Send cached data if available.
  if (delegate_ && cached_data_.is_valid()) {
    size_t bytes_copied = SendDataToDelegate(bytes_to_read);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X - bytes read from cache: %d",
        url().c_str(), this, bytes_copied);
    return true;
  }

  if (status_.get_state() == Status::WORKING) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        "- Read pending for: " << bytes_to_read;
    pending_read_size_ = bytes_to_read;
  } else {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X. Response finished.",
        url().c_str(), this);
    NotifyDelegateAndDie();
  }

  return true;
}

HRESULT UrlmonUrlRequest::UseBindCtx(IMoniker* moniker, LPBC bc) {
  DCHECK(bind_context_ == NULL);
  DCHECK(moniker_ == NULL);
  bind_context_ = bc;
  moniker_ = moniker;
  return S_OK;
}

void UrlmonUrlRequest::StealMoniker(IMoniker** moniker, IBindCtx** bctx) {
  // Could be called in any thread. There should be no race
  // since moniker_ is not released while we are in manager's request map.
  DLOG(INFO) << __FUNCTION__ << " id: " << id();
  DLOG_IF(WARNING, moniker == NULL) << __FUNCTION__ << " no moniker";
  *moniker = moniker_.Detach();
  *bctx = bind_context_.Detach();
}

size_t UrlmonUrlRequest::SendDataToDelegate(size_t bytes_to_read) {
  // We can optimize a bit by setting this string as a class member
  // and avoid frequent memory reallocations.
  std::string data;
  size_t bytes_copied;

  size_t bytes = std::min(size_t(bytes_to_read), cached_data_.size());
  cached_data_.Read(WriteInto(&data, 1 + bytes), bytes, &bytes_copied);
  ++calling_delegate_;
  delegate_->OnReadComplete(id(), data);
  --calling_delegate_;
  return bytes_copied;
}

STDMETHODIMP UrlmonUrlRequest::OnStartBinding(DWORD reserved,
                                              IBinding *binding) {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  binding_ = binding;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetPriority(LONG *priority) {
  if (!priority)
    return E_POINTER;
  *priority = THREAD_PRIORITY_NORMAL;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnLowResource(DWORD reserved) {
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnProgress(ULONG progress, ULONG max_progress,
    ULONG status_code, LPCWSTR status_text) {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());

  if (status_.get_state() != Status::WORKING) {
    return S_OK;
  }

  switch (status_code) {
    case BINDSTATUS_REDIRECTING: {
      DLOG(INFO) << "URL: " << url() << " redirected to " << status_text;
      // Fetch the redirect status as they aren't all equal (307 in particular
      // retains the HTTP request verb).
      int http_code = GetHttpResponseStatus();
      status_.SetRedirected(http_code, WideToUTF8(status_text));
      // Abort. We will inform Chrome in OnStopBinding callback.
      binding_->Abort();
      return E_ABORT;
    }

    case BINDSTATUS_COOKIE_SENT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_READ);
      break;

    case BINDSTATUS_COOKIE_SUPPRESSED:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_SUPPRESS);
      break;

    case BINDSTATUS_COOKIE_STATE_ACCEPT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_ACCEPT);
      break;

    case BINDSTATUS_COOKIE_STATE_REJECT:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_REJECT);
      break;

    case BINDSTATUS_COOKIE_STATE_LEASH:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_LEASH);
      break;

    case BINDSTATUS_COOKIE_STATE_DOWNGRADE:
      delegate_->AddPrivacyDataForUrl(url(), "", COOKIEACTION_DOWNGRADE);
      break;

    case BINDSTATUS_COOKIE_STATE_UNKNOWN:
      NOTREACHED() << L"Unknown cookie state received";
      break;

    default:
      DLOG(INFO) << " Obj: " << std::hex << this << " OnProgress(" << url()
          << StringPrintf(L") code: %i status: %ls", status_code, status_text);
      break;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnStopBinding(HRESULT result, LPCWSTR error) {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " - Request stopped, Result: " << std::hex << result;
  DCHECK(status_.get_state() == Status::WORKING ||
         status_.get_state() == Status::ABORTING);

  Status::State state = status_.get_state();

  // Mark we a are done.
  status_.Done();

  // We always return INET_E_TERMINATED_BIND from OnDataAvailable
  if (result == INET_E_TERMINATED_BIND)
    result = S_OK;

  if (state == Status::WORKING) {
    status_.set_result(result);

    // Special case. If the last request was a redirect and the current OS
    // error value is E_ACCESSDENIED, that means an unsafe redirect was
    // attempted. In that case, correct the OS error value to be the more
    // specific ERR_UNSAFE_REDIRECT error value.
    if (result == E_ACCESSDENIED) {
      int http_code = GetHttpResponseStatus();
      if (300 <= http_code && http_code < 400) {
        status_.set_result(URLRequestStatus::FAILED,
                          net::ERR_UNSAFE_REDIRECT);
      }
    }

    // The code below seems easy but it is not. :)
    // The network policy in Chrome network is that error code/end_of_stream
    // should be returned only as a result of read (or start) request.
    // Here is the possible cases:
    // cached_data|pending_read
    //     FALSE  |FALSE    => EndRequest if no headers, otherwise wait for Read
    //     FALSE  |TRUE     => EndRequest.
    //     TRUE   |FALSE    => Wait for Read.
    //     TRUE   |TRUE     => Something went wrong!!

    // we cannot have pending read and data_avail at the same time.
    DCHECK(!(pending_read_size_ > 0 && cached_data_.is_valid()));

    if (cached_data_.is_valid()) {
      ReleaseBindings();
      return S_OK;
    }

    if (headers_received_ && pending_read_size_ == 0) {
      ReleaseBindings();
      return S_OK;
    }

    // No headers or there is a pending read from Chrome.
    NotifyDelegateAndDie();
    return S_OK;
  }

  // Status::ABORTING
  if (status_.was_redirected()) {
    // Just release bindings here. Chrome will issue EndRequest(request_id)
    // after processing headers we had provided.
    std::string headers = GetHttpHeaders();
    OnResponse(0, UTF8ToWide(headers).c_str(), NULL, NULL);
    ReleaseBindings();
    return S_OK;
  }

  // Stop invoked.
  NotifyDelegateAndDie();
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetBindInfo(DWORD* bind_flags,
                                           BINDINFO* bind_info) {
  if ((bind_info == NULL) || (bind_info->cbSize == 0) || (bind_flags == NULL))
    return E_INVALIDARG;

  *bind_flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;

  bool upload_data = false;

  if (LowerCaseEqualsASCII(method(), "get")) {
    bind_info->dwBindVerb = BINDVERB_GET;
  } else if (LowerCaseEqualsASCII(method(), "post")) {
    bind_info->dwBindVerb = BINDVERB_POST;
    upload_data = true;
  } else if (LowerCaseEqualsASCII(method(), "put")) {
    bind_info->dwBindVerb = BINDVERB_PUT;
    upload_data = true;
  } else if (LowerCaseEqualsASCII(method(), "head")) {
    std::wstring verb(ASCIIToWide(StringToUpperASCII(method())));
    bind_info->dwBindVerb = BINDVERB_CUSTOM;
    bind_info->szCustomVerb = reinterpret_cast<wchar_t*>(
        ::CoTaskMemAlloc((verb.length() + 1) * sizeof(wchar_t)));
    lstrcpyW(bind_info->szCustomVerb, verb.c_str());
  } else {
    NOTREACHED() << "Unknown HTTP method.";
    status_.set_result(URLRequestStatus::FAILED, net::ERR_METHOD_NOT_SUPPORTED);
    NotifyDelegateAndDie();
    return E_FAIL;
  }

  if (upload_data) {
    // Bypass caching proxies on POSTs and PUTs and avoid writing responses to
    // these requests to the browser's cache
    *bind_flags |= BINDF_GETNEWESTVERSION | BINDF_PRAGMA_NO_CACHE;

    // Initialize the STGMEDIUM.
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;
    bind_info->szCustomVerb = NULL;

    if (get_upload_data(&bind_info->stgmedData.pstm) == S_OK) {
      bind_info->stgmedData.tymed = TYMED_ISTREAM;
      DLOG(INFO) << " Obj: " << std::hex << this << " " << method()
                 << " request with " << Int64ToString(post_data_len())
                 << " bytes";
    } else {
      DLOG(INFO) << " Obj: " << std::hex << this
          << "POST request with no data!";
    }
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnDataAvailable(DWORD flags, DWORD size,
                                               FORMATETC* formatetc,
                                               STGMEDIUM* storage) {
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X - Bytes available: %d",
                             url().c_str(), this, size);

  if (!storage || (storage->tymed != TYMED_ISTREAM)) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  IStream* read_stream = storage->pstm;
  if (!read_stream) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  HRESULT hr = S_OK;
  if (BSCF_FIRSTDATANOTIFICATION & flags) {
    DCHECK(!cached_data_.is_valid());
  }

  // Always read data into cache. We have to read all the data here at this
  // time or it won't be available later. Since the size of the data could
  // be more than pending read size, it's not straightforward (or might even
  // be impossible) to implement a true data pull model.
  size_t cached = cached_data_.size();
  cached_data_.Append(read_stream);
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " -  Bytes read into cache: " << cached_data_.size() - cached;

  if (pending_read_size_ && cached_data_.is_valid()) {
    size_t bytes_copied = SendDataToDelegate(pending_read_size_);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - size read: " << bytes_copied;
    pending_read_size_ = 0;
  } else {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - waiting for remote read";
  }

  if (BSCF_LASTDATANOTIFICATION & flags) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - end of data.";

    // Always return INET_E_TERMINATED_BIND to allow bind context reuse
    // if DownloadToHost is suddenly requested.
    return INET_E_TERMINATED_BIND;
  }

  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnObjectAvailable(REFIID iid, IUnknown* object) {
  // We are calling BindToStorage on the moniker we should always get called
  // back on OnDataAvailable and should never get OnObjectAvailable
  NOTREACHED();
  return E_NOTIMPL;
}

STDMETHODIMP UrlmonUrlRequest::BeginningTransaction(const wchar_t* url,
    const wchar_t* current_headers, DWORD reserved,
    wchar_t** additional_headers) {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  if (!additional_headers) {
    NOTREACHED();
    return E_POINTER;
  }

  DLOG(INFO) << "URL: " << url << " Obj: " << std::hex << this <<
      " - Request headers: \n" << current_headers;

  if (status_.get_state() == Status::ABORTING) {
    // At times the BINDSTATUS_REDIRECTING notification which is sent to the
    // IBindStatusCallback interface does not have an accompanying HTTP
    // redirect status code, i.e. the attempt to query the HTTP status code
    // from the binding returns 0, 200, etc which are invalid redirect codes.
    // We don't want urlmon to follow redirects. We return E_ABORT in our
    // IBindStatusCallback::OnProgress function and also abort the binding.
    // However urlmon still tries to establish a transaction with the
    // redirected URL which confuses the web server.
    // Fix is to abort the attempted transaction.
    DLOG(WARNING) << __FUNCTION__
                  << ": Aborting connection to URL:"
                  << url
                  << " as the binding has been aborted";
    return E_ABORT;
  }

  HRESULT hr = S_OK;

  std::string new_headers;
  if (post_data_len() > 0) {
    // Tack on the Content-Length header since when using an IStream type
    // STGMEDIUM, it looks like it doesn't get set for us :(
    new_headers = StringPrintf("Content-Length: %s\r\n",
                               Int64ToString(post_data_len()).c_str());
  }

  if (!extra_headers().empty()) {
    // TODO(robertshield): We may need to sanitize headers on POST here.
    new_headers += extra_headers();
  }

  if (!referrer().empty()) {
    // Referrer is famously misspelled in HTTP:
    new_headers += StringPrintf("Referer: %s\r\n", referrer().c_str());
  }

  if (!new_headers.empty()) {
    *additional_headers = reinterpret_cast<wchar_t*>(
        CoTaskMemAlloc((new_headers.size() + 1) * sizeof(wchar_t)));

    if (*additional_headers == NULL) {
      NOTREACHED();
      hr = E_OUTOFMEMORY;
    } else {
      lstrcpynW(*additional_headers, ASCIIToWide(new_headers).c_str(),
                new_headers.size());
    }
  }

  return hr;
}

STDMETHODIMP UrlmonUrlRequest::OnResponse(DWORD dwResponseCode,
    const wchar_t* response_headers, const wchar_t* request_headers,
    wchar_t** additional_headers) {
  DLOG(INFO) << __FUNCTION__ << " " << url() << std::endl << " headers: " <<
      std::endl << response_headers;
  DLOG(INFO) << __FUNCTION__
      << StringPrintf(" this=0x%08X, tid=%i", this, ::GetCurrentThreadId());
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  DCHECK(binding_ != NULL);

  std::string raw_headers = WideToUTF8(response_headers);

  delegate_->AddPrivacyDataForUrl(url(), "", 0);

  // Security check for frame busting headers. We don't honor the headers
  // as-such, but instead simply kill requests which we've been asked to
  // look for if they specify a value for "X-Frame-Options" other than
  // "ALLOWALL" (the others are "deny" and "sameorigin"). This puts the onus
  // on the user of the UrlRequest to specify whether or not requests should
  // be inspected. For ActiveDocuments, the answer is "no", since WebKit's
  // detection/handling is sufficient and since ActiveDocuments cannot be
  // hosted as iframes. For NPAPI and ActiveX documents, the Initialize()
  // function of the PluginUrlRequest object allows them to specify how they'd
  // like requests handled. Both should set enable_frame_busting_ to true to
  // avoid CSRF attacks. Should WebKit's handling of this ever change, we will
  // need to re-visit how and when frames are killed to better mirror a policy
  // which may do something other than kill the sub-document outright.

  // NOTE(slightlyoff): We don't use net::HttpResponseHeaders here because
  //    of lingering ICU/base_noicu issues.
  if (enable_frame_busting_) {
    if (http_utils::HasFrameBustingHeader(raw_headers)) {
      DLOG(ERROR) << "X-Frame-Options header other than ALLOWALL " <<
          "detected, navigation canceled";
      return E_FAIL;
    }
  }

  DLOG(INFO) << "Calling OnResponseStarted";

  // Inform the delegate.
  headers_received_ = true;
  delegate_->OnResponseStarted(id(),
                    "",                   // mime_type
                    raw_headers.c_str(),  // headers
                    0,                    // size
                    base::Time(),         // last_modified
                    status_.get_redirection().utf8_url,
                    status_.get_redirection().http_code);
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::GetWindow(const GUID& guid_reason,
                                         HWND* parent_window) {
  if (!parent_window) {
    return E_INVALIDARG;
  }

#ifndef NDEBUG
  wchar_t guid[40] = {0};
  ::StringFromGUID2(guid_reason, guid, arraysize(guid));

  DLOG(INFO) << " Obj: " << std::hex << this << " GetWindow: " <<
      (guid_reason == IID_IAuthenticate ? L" - IAuthenticate" :
       (guid_reason == IID_IHttpSecurity ? L"IHttpSecurity" :
        (guid_reason == IID_IWindowForBindingUI ? L"IWindowForBindingUI" :
                                                  guid)));
#endif
  // We should return a non-NULL HWND as parent. Otherwise no dialog is shown.
  // TODO(iyengar): This hits when running the URL request tests.
  DLOG_IF(ERROR, !::IsWindow(parent_window_))
      << "UrlmonUrlRequest::GetWindow - no window!";
  *parent_window = parent_window_;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::Authenticate(HWND* parent_window,
                                            LPWSTR* user_name,
                                            LPWSTR* password) {
  if (!parent_window)
    return E_INVALIDARG;

  if (privileged_mode_)
    return E_ACCESSDENIED;

  DCHECK(::IsWindow(parent_window_));
  *parent_window = parent_window_;
  return S_OK;
}

STDMETHODIMP UrlmonUrlRequest::OnSecurityProblem(DWORD problem) {
  // Urlmon notifies the client of authentication problems, certificate
  // errors, etc by querying the object implementing the IBindStatusCallback
  // interface for the IHttpSecurity interface. If this interface is not
  // implemented then Urlmon checks for the problem codes defined below
  // and performs actions as defined below:-
  // It invokes the ReportProgress method of the protocol sink with
  // these problem codes and eventually invokes the ReportResult method
  // on the protocol sink which ends up in a call to the OnStopBinding
  // method of the IBindStatusCallBack interface.

  // MSHTML's implementation of the IBindStatusCallback interface does not
  // implement the IHttpSecurity interface. However it handles the
  // OnStopBinding call with a HRESULT of 0x800c0019 and navigates to
  // an interstitial page which presents the user with a choice of whether
  // to abort the navigation.

  // In our OnStopBinding implementation we stop the navigation and inform
  // Chrome about the result. Ideally Chrome should behave in a manner similar
  // to IE, i.e. display the SSL error interstitial page and if the user
  // decides to proceed anyway we would turn off SSL warnings for that
  // particular navigation and allow IE to download the content.
  // We would need to return the certificate information to Chrome for display
  // purposes. Currently we only return a dummy certificate to Chrome.
  // At this point we decided that it is a lot of work at this point and
  // decided to go with the easier option of implementing the IHttpSecurity
  // interface and replicating the checks performed by Urlmon. This
  // causes Urlmon to display a dialog box on the same lines as IE6.
  DLOG(INFO) << __FUNCTION__ << " Security problem : " << problem;

  // On IE6 the default IBindStatusCallback interface does not implement the
  // IHttpSecurity interface and thus causes IE to put up a certificate error
  // dialog box. We need to emulate this behavior for sites with mismatched
  // certificates to work.
  if (GetIEVersion() == IE_6)
    return S_FALSE;

  HRESULT hr = E_ABORT;

  switch (problem) {
    case ERROR_INTERNET_SEC_CERT_REV_FAILED: {
      hr = RPC_E_RETRY;
      break;
    }

    case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
    case ERROR_INTERNET_SEC_CERT_CN_INVALID:
    case ERROR_INTERNET_INVALID_CA: {
      hr = S_FALSE;
      break;
    }

    default: {
      NOTREACHED() << "Unhandled security problem : " << problem;
      break;
    }
  }
  return hr;
}

HRESULT UrlmonUrlRequest::StartAsyncDownload() {
  DLOG(INFO) << __FUNCTION__
      << StringPrintf(" this=0x%08X, tid=%i", this, ::GetCurrentThreadId());
  HRESULT hr = E_FAIL;
  DCHECK((moniker_ && bind_context_) || (!moniker_ && !bind_context_));

  if (!moniker_.get()) {
    std::wstring wide_url = UTF8ToWide(url());
    hr = CreateURLMonikerEx(NULL, wide_url.c_str(), moniker_.Receive(),
                            URL_MK_UNIFORM);
    if (FAILED(hr)) {
      NOTREACHED() << "CreateURLMonikerEx failed. Error: " << hr;
      return hr;
    }
  }

  if (bind_context_.get() == NULL)  {
    hr = ::CreateAsyncBindCtxEx(NULL, 0, this, NULL,
                                bind_context_.Receive(), 0);
    DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtxEx failed. Error: " << hr;
  } else {
    // Use existing bind context.
    hr = ::RegisterBindStatusCallback(bind_context_, this, NULL, 0);
    DCHECK(SUCCEEDED(hr)) << "RegisterBindStatusCallback failed. Error: " << hr;
  }

  if (SUCCEEDED(hr)) {
    ScopedComPtr<IStream> stream;

    // BindToStorage may complete synchronously.
    // We still get all the callbacks - OnStart/StopBinding, this may result
    // in destruction of our object. It's fine but we access some members
    // below for debug info. :)
    ScopedComPtr<IHttpSecurity> self(this);

    // Inform our moniker patch this binding should nto be tortured.
    // TODO(amit): factor this out.
    hr = bind_context_->RegisterObjectParam(L"_CHROMEFRAME_REQUEST_", self);
    DCHECK(SUCCEEDED(hr));

    hr = moniker_->BindToStorage(bind_context_, NULL, __uuidof(IStream),
                                 reinterpret_cast<void**>(stream.Receive()));

    bind_context_->RevokeObjectParam(L"_CHROMEFRAME_REQUEST_");

    if (hr == S_OK) {
      DCHECK(binding_ != NULL || status_.get_state() == Status::DONE);
    }

    if (FAILED(hr)) {
      // TODO(joshia): Look into. This currently fails for:
      // http://user2:secret@localhost:1337/auth-basic?set-cookie-if-challenged
      // when running the UrlRequest unit tests.
      DLOG(ERROR) <<
          StringPrintf("IUrlMoniker::BindToStorage failed. Error: 0x%08X.", hr)
          << std::endl << url();
      DCHECK(hr == MK_E_SYNTAX);
    }
  }

  DLOG_IF(ERROR, FAILED(hr))
      << StringPrintf(L"StartAsyncDownload failed: 0x%08X", hr);

  return hr;
}

void UrlmonUrlRequest::NotifyDelegateAndDie() {
  DCHECK_EQ(thread_, PlatformThread::CurrentId());
  DLOG(INFO) << __FUNCTION__;
  PluginUrlRequestDelegate* delegate = delegate_;
  delegate_ = NULL;
  ReleaseBindings();
  bind_context_.Release();
  if (delegate) {
    URLRequestStatus result = status_.get_result();
    delegate->OnResponseEnd(id(), result);
  }
}

int UrlmonUrlRequest::GetHttpResponseStatus() const {
  DLOG(INFO) << __FUNCTION__;
  if (binding_ == NULL) {
    DLOG(WARNING) << "GetHttpResponseStatus - no binding_";
    return 0;
  }

  int http_status = 0;

  ScopedComPtr<IWinInetHttpInfo> info;
  if (SUCCEEDED(info.QueryFrom(binding_))) {
    char status[10] = {0};
    DWORD buf_size = sizeof(status);
    DWORD flags = 0;
    DWORD reserved = 0;
    if (SUCCEEDED(info->QueryInfo(HTTP_QUERY_STATUS_CODE, status, &buf_size,
                                  &flags, &reserved))) {
      http_status = StringToInt(status);
    } else {
      NOTREACHED() << "Failed to get HTTP status";
    }
  } else {
    NOTREACHED() << "failed to get IWinInetHttpInfo from binding_";
  }

  return http_status;
}

std::string UrlmonUrlRequest::GetHttpHeaders() const {
  if (binding_ == NULL) {
    DLOG(WARNING) << "GetHttpResponseStatus - no binding_";
    return std::string();
  }

  ScopedComPtr<IWinInetHttpInfo> info;
  if (FAILED(info.QueryFrom(binding_))) {
    DLOG(WARNING) << "Failed to QI for IWinInetHttpInfo";
    return std::string();
  }

  return GetRawHttpHeaders(info);
}

void UrlmonUrlRequest::ReleaseBindings() {
  binding_.Release();
  // Do not release bind_context here!
  // We may get DownloadToHost request and therefore we want the bind_context
  // to be available.
  if (bind_context_) {
    ::RevokeBindStatusCallback(bind_context_, this);
  }
}

//
// UrlmonUrlRequest::Cache implementation.
//

UrlmonUrlRequest::Cache::~Cache() {
  while (cache_.size()) {
    uint8* t = cache_.front();
    cache_.pop_front();
    delete [] t;
  }

  while (pool_.size()) {
    uint8* t = pool_.front();
    pool_.pop_front();
    delete [] t;
  }
}

void UrlmonUrlRequest::Cache::GetReadBuffer(void** src, size_t* bytes_avail) {
  DCHECK_LT(read_offset_, BUF_SIZE);
  *src = NULL;
  *bytes_avail = 0;
  if (cache_.size()) {
    if (cache_.size() == 1)
      *bytes_avail = write_offset_ - read_offset_;
    else
      *bytes_avail = BUF_SIZE - read_offset_;

    // Return non-NULL pointer only if there is some data
    if (*bytes_avail)
      *src = cache_.front() + read_offset_;
  }
}

void UrlmonUrlRequest::Cache::BytesRead(size_t bytes) {
  DCHECK_LT(read_offset_, BUF_SIZE);
  DCHECK_LE(read_offset_ + bytes, BUF_SIZE);
  DCHECK_LE(bytes, size_);

  size_ -= bytes;
  read_offset_ += bytes;
  if (read_offset_ == BUF_SIZE) {
    uint8* p = cache_.front();
    cache_.pop_front();
    // check if pool_ became too large
    pool_.push_front(p);
    read_offset_ = 0;
  }
}

bool UrlmonUrlRequest::Cache::Read(void* dest, size_t bytes,
                                   size_t* bytes_copied) {
  void* src;
  size_t src_size;

  DLOG(INFO) << __FUNCTION__;
  *bytes_copied = 0;
  while (bytes) {
    GetReadBuffer(&src, &src_size);
    if (src_size == 0)
      break;

    size_t bytes_to_copy = std::min(src_size, bytes);
    memcpy(dest, src, bytes_to_copy);

    BytesRead(bytes_to_copy);
    dest = reinterpret_cast<uint8*>(dest) + bytes_to_copy;
    bytes -= bytes_to_copy;
    *bytes_copied += bytes_to_copy;
  }

  return true;
}


void UrlmonUrlRequest::Cache::GetWriteBuffer(void** dest, size_t* bytes_avail) {
  if (cache_.size() == 0 || write_offset_ == BUF_SIZE) {

    if (pool_.size()) {
      cache_.push_back(pool_.front());
      pool_.pop_front();
    } else {
      cache_.push_back(new uint8[BUF_SIZE]);
    }

    write_offset_ = 0;
  }

  *dest = cache_.back() + write_offset_;
  *bytes_avail = BUF_SIZE - write_offset_;
}

void UrlmonUrlRequest::Cache::BytesWritten(size_t bytes) {
  DCHECK_LE(write_offset_ + bytes, BUF_SIZE);
  write_offset_ += bytes;
  size_ += bytes;
}

bool UrlmonUrlRequest::Cache::Append(IStream* source) {
  if (!source) {
    NOTREACHED();
    return false;
  }

  HRESULT hr = S_OK;
  while (SUCCEEDED(hr)) {
    void* dest = 0;
    size_t bytes = 0;
    DWORD chunk_read = 0;  // NOLINT
    GetWriteBuffer(&dest, &bytes);
    hr = source->Read(dest, bytes, &chunk_read);
    BytesWritten(chunk_read);

    if (hr == S_OK && chunk_read == 0) {
      // implied EOF
      break;
    }

    if (hr == S_FALSE) {
      // EOF
      break;
    }
  }

  return SUCCEEDED(hr);
}

net::Error UrlmonUrlRequest::HresultToNetError(HRESULT hr) {
  // Useful reference:
  // http://msdn.microsoft.com/en-us/library/ms775145(VS.85).aspx

  net::Error ret = net::ERR_UNEXPECTED;

  switch (hr) {
    case S_OK:
      ret = net::OK;
      break;

    case MK_E_SYNTAX:
      ret = net::ERR_INVALID_URL;
      break;

    case INET_E_CANNOT_CONNECT:
      ret = net::ERR_CONNECTION_FAILED;
      break;

    case INET_E_DOWNLOAD_FAILURE:
    case INET_E_CONNECTION_TIMEOUT:
    case E_ABORT:
      ret = net::ERR_CONNECTION_ABORTED;
      break;

    case INET_E_DATA_NOT_AVAILABLE:
      ret = net::ERR_EMPTY_RESPONSE;
      break;

    case INET_E_RESOURCE_NOT_FOUND:
      // To behave more closely to the chrome network stack, we translate this
      // error value as tunnel connection failed.  This error value is tested
      // in the ProxyTunnelRedirectTest and UnexpectedServerAuthTest tests.
      ret = net::ERR_TUNNEL_CONNECTION_FAILED;
      break;

    case INET_E_INVALID_URL:
    case INET_E_UNKNOWN_PROTOCOL:
    case INET_E_REDIRECT_FAILED:
      ret = net::ERR_INVALID_URL;
      break;

    case INET_E_INVALID_CERTIFICATE:
      ret = net::ERR_CERT_INVALID;
      break;

    case E_ACCESSDENIED:
      ret = net::ERR_ACCESS_DENIED;
      break;

    default:
      DLOG(WARNING)
          << StringPrintf("TODO: translate HRESULT 0x%08X to net::Error", hr);
      break;
  }
  return ret;
}


bool UrlmonUrlRequestManager::IsThreadSafe() {
  return false;
}

void UrlmonUrlRequestManager::SetInfoForUrl(const std::wstring& url,
                                            IMoniker* moniker, LPBC bind_ctx) {
  url_info_.Set(url, moniker, bind_ctx);
}

void UrlmonUrlRequestManager::StartRequest(int request_id,
    const IPC::AutomationURLRequest& request_info) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(0, calling_delegate_);

  if (stopping_)
    return;

  DCHECK(LookupRequest(request_id).get() == NULL);

  CComObject<UrlmonUrlRequest>* new_request = NULL;
  CComObject<UrlmonUrlRequest>::CreateInstance(&new_request);

  new_request->Initialize(static_cast<PluginUrlRequestDelegate*>(this),
      request_id,
      request_info.url,
      request_info.method,
      request_info.referrer,
      request_info.extra_request_headers,
      request_info.upload_data,
      enable_frame_busting_);
  new_request->set_parent_window(notification_window_);
  new_request->set_privileged_mode(privileged_mode_);

  // Shall we use previously fetched data?
  if (url_info_.IsForUrl(request_info.url)) {
    new_request->UseBindCtx(url_info_.moniker_, url_info_.bind_ctx_);
    url_info_.Clear();
  }

  request_map_[request_id] = new_request;
  new_request->Start();
}

void UrlmonUrlRequestManager::ReadRequest(int request_id, int bytes_to_read) {
  DLOG(INFO) << __FUNCTION__ << " id: " << request_id;
  DCHECK_EQ(0, calling_delegate_);
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id);
  // if zero, it may just have had network error.
  if (request) {
    request->Read(bytes_to_read);
  }
}

void UrlmonUrlRequestManager::DownloadRequestInHost(int request_id) {
  DLOG(INFO) << __FUNCTION__ << " " << request_id;
  if (IsWindow(notification_window_)) {
    scoped_refptr<UrlmonUrlRequest> request(LookupRequest(request_id));
    if (request) {
      ScopedComPtr<IMoniker> moniker;
      ScopedComPtr<IBindCtx> bind_context;
      request->StealMoniker(moniker.Receive(), bind_context.Receive());
      DLOG_IF(ERROR, moniker == NULL) << __FUNCTION__ << " No moniker!";
      if (moniker) {
        // We use SendMessage and not PostMessage to make sure that if the
        // notification window does not handle the message we won't leak
        // the moniker.
        ::SendMessage(notification_window_, WM_DOWNLOAD_IN_HOST,
            reinterpret_cast<WPARAM>(bind_context.get()),
            reinterpret_cast<LPARAM>(moniker.get()));
      }
    }
  } else {
    NOTREACHED()
        << "Cannot handle download if we don't have anyone to hand it to.";
  }
}

bool UrlmonUrlRequestManager::GetCookiesForUrl(int tab_handle,
                                               const GURL& url,
                                               int cookie_id) {
  DWORD cookie_size = 0;
  bool success = true;
  std::string cookie_string;

  int32 cookie_action = COOKIEACTION_READ;
  BOOL result = InternetGetCookieA(url.spec().c_str(), NULL, NULL,
                                   &cookie_size);
  DWORD error = 0;
  if (cookie_size) {
    scoped_array<char> cookies(new char[cookie_size + 1]);
    if (!InternetGetCookieA(url.spec().c_str(), NULL, cookies.get(),
                            &cookie_size)) {
      success = false;
      error = GetLastError();
      NOTREACHED() << "InternetGetCookie failed. Error: " << error;
    } else {
      cookie_string = cookies.get();
    }
  } else {
    success = false;
    error = GetLastError();
    DLOG(INFO) << "InternetGetCookie failed. Error: " << error;
  }

  if (delegate_) {
    delegate_->SendIPCMessage(
        new AutomationMsg_GetCookiesHostResponse(0, tab_handle, success,
                                                 url, cookie_string,
                                                 cookie_id));
  }

  if (!success && !error)
    cookie_action = COOKIEACTION_SUPPRESS;

  AddPrivacyDataForUrl(url.spec(), "", cookie_action);
  return true;
}

bool UrlmonUrlRequestManager::SetCookiesForUrl(int tab_handle,
                                               const GURL& url,
                                               const std::string& cookie) {
  std::string name;
  std::string data;

  size_t name_end = cookie.find('=');
  if (std::string::npos != name_end) {
    net::CookieMonster::ParsedCookie parsed_cookie = cookie;
    name = parsed_cookie.Name();
    // Verify if the cookie is being deleted. The cookie format is as below
    // value[; expires=date][; domain=domain][; path=path][; secure]
    // If the first semicolon appears immediately after the name= string,
    // it means that the cookie is being deleted, in which case we should
    // pass the data as is to the InternetSetCookie function.
    if (!parsed_cookie.Value().empty()) {
      name.clear();
      data = cookie;
    } else {
      data = cookie.substr(name_end + 1);
    }
  } else {
    data = cookie;
  }

  int32 flags = INTERNET_COOKIE_EVALUATE_P3P;

  InternetCookieState cookie_state = static_cast<InternetCookieState>(
      InternetSetCookieExA(url.spec().c_str(), name.c_str(), data.c_str(),
                           flags, NULL));

  int32 cookie_action = MapCookieStateToCookieAction(cookie_state);
  AddPrivacyDataForUrl(url.spec(), "", cookie_action);
  return true;
}

void UrlmonUrlRequestManager::EndRequest(int request_id) {
  DLOG(INFO) << __FUNCTION__ << " id: " << request_id;
  DCHECK_EQ(0, calling_delegate_);
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id);
  if (request) {
    request_map_.erase(request_id);
    request->Stop();
  }
}

void UrlmonUrlRequestManager::StopAll() {
  DLOG(INFO) << __FUNCTION__;
  if (stopping_)
    return;

  stopping_ = true;
  for (RequestMap::iterator it = request_map_.begin();
       it != request_map_.end(); ++it) {
    DCHECK(it->second != NULL);
    it->second->Stop();
  }

  request_map_.empty();
}

void UrlmonUrlRequestManager::OnResponseStarted(int request_id,
    const char* mime_type, const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(LookupRequest(request_id).get() != NULL);
  ++calling_delegate_;
  delegate_->OnResponseStarted(request_id, mime_type, headers, size,
      last_modified, redirect_url, redirect_status);
  --calling_delegate_;
}

void UrlmonUrlRequestManager::OnReadComplete(int request_id,
                                             const std::string& data) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(LookupRequest(request_id).get() != NULL);
  ++calling_delegate_;
  delegate_->OnReadComplete(request_id, data);
  --calling_delegate_;
}

void UrlmonUrlRequestManager::OnResponseEnd(int request_id,
                                            const URLRequestStatus& status) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(status.status() != URLRequestStatus::CANCELED);
  RequestMap::size_type n = request_map_.erase(request_id);
  DCHECK_EQ(1, n);
  ++calling_delegate_;
  delegate_->OnResponseEnd(request_id, status);
  --calling_delegate_;
}

scoped_refptr<UrlmonUrlRequest> UrlmonUrlRequestManager::LookupRequest(
    int request_id) {
  RequestMap::iterator it = request_map_.find(request_id);
  if (request_map_.end() != it)
    return it->second;
  return NULL;
}

UrlmonUrlRequestManager::UrlmonUrlRequestManager()
    : stopping_(false), calling_delegate_(0), notification_window_(NULL),
      privileged_mode_(false) {
}

UrlmonUrlRequestManager::~UrlmonUrlRequestManager() {
  StopAll();
}

void UrlmonUrlRequestManager::AddPrivacyDataForUrl(
    const std::string& url, const std::string& policy_ref,
    int32 flags) {
  bool fire_privacy_event = false;

  if (privacy_info_.privacy_records.size() == 0)
    flags |= PRIVACY_URLISTOPLEVEL;

  if (!privacy_info_.privacy_impacted) {
    if (flags & (COOKIEACTION_ACCEPT | COOKIEACTION_REJECT |
                 COOKIEACTION_DOWNGRADE)) {
      privacy_info_.privacy_impacted = true;
      fire_privacy_event = true;
    }
  }

  PrivacyInfo::PrivacyEntry& privacy_entry =
      privacy_info_.privacy_records[UTF8ToWide(url)];

  privacy_entry.flags |= flags;
  privacy_entry.policy_ref = UTF8ToWide(policy_ref);

  if (fire_privacy_event && IsWindow(notification_window_)) {
    PostMessage(notification_window_, WM_FIRE_PRIVACY_CHANGE_NOTIFICATION, 1,
                0);
  }
}

