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
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "chrome_frame/utils.h"
#include "net/http/http_util.h"
#include "net/http/http_response_headers.h"

static const LARGE_INTEGER kZero = {0};
static const ULARGE_INTEGER kUnsignedZero = {0};

STDMETHODIMP UrlmonUrlRequest::SendStream::Write(const void * buffer,
                                                 ULONG size,
                                                 ULONG* size_written) {
  DCHECK(request_);
  int size_to_write = static_cast<int>(
      std::min(static_cast<ULONG>(MAXINT), size));
  request_->delegate_->OnReadComplete(request_->id(), buffer,
                                      size_to_write);
  if (size_written)
    *size_written = size_to_write;
  return S_OK;
}

UrlmonUrlRequest::UrlmonUrlRequest()
    : pending_read_size_(0),
      headers_received_(false),
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
  HRESULT hr = E_UNEXPECTED;
  if (request_data_) {
    DCHECK(bind_context_ == NULL);
    hr = CreateAsyncBindCtx(0, this, NULL, bind_context_.Receive());
    DCHECK(SUCCEEDED(hr));
    CComObject<SimpleBindingImpl>* binding = NULL;
    CComObject<SimpleBindingImpl>::CreateInstance(&binding);
    binding->AddRef();
    hr = MonikerPatch::BindToStorageFromCache(bind_context_, NULL,
        request_data_, binding, NULL);
    binding->Release();
  } else {
    hr = StartAsyncDownload();
  }

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
  // Re-entrancy check. Thou shall not call Read() while processOnReadComplete!!
  DCHECK_EQ(0, pending_read_size_);
  if (pending_read_size_ != 0)
    return false;

  DCHECK((status_.get_state() != Status::DONE) == (binding_ != NULL));
  if (status_.get_state() == Status::ABORTING) {
    return true;
  }

  // Send cached data if available.
  CComObjectStackEx<SendStream> send_stream;
  send_stream.Initialize(this);

  size_t bytes_copied = 0;
  if (delegate_ && cached_data_.is_valid() &&
      cached_data_.Read(&send_stream, bytes_to_read, &bytes_copied)) {
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

HRESULT UrlmonUrlRequest::SetRequestData(RequestData* data) {
  request_data_ = data;
  return S_OK;
}

void UrlmonUrlRequest::StealMoniker(IMoniker** moniker) {
  // Could be called in any thread. There should be no race
  // since moniker_ is not released while we are in manager's request map.
  *moniker = moniker_.Detach();
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
  } else {
    NOTREACHED() << "Unknown HTTP method.";
    status_.set_result(URLRequestStatus::FAILED, net::ERR_METHOD_NOT_SUPPORTED);
    NotifyDelegateAndDie();
    return E_FAIL;
  }

  if (upload_data) {
    // Bypass caching proxies on POSTs and PUTs and avoid writing responses to
    // these requests to the browser's cache
    *bind_flags |= BINDF_GETNEWESTVERSION | BINDF_NOWRITECACHE |
                   BINDF_PRAGMA_NO_CACHE;

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
  size_t bytes_available = 0;
  cached_data_.Append(read_stream, &bytes_available);
  DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
      " -  Bytes read into cache: " << bytes_available;

  if (pending_read_size_ && cached_data_.is_valid()) {
    CComObjectStackEx<SendStream> send_stream;
    send_stream.Initialize(this);
    cached_data_.Read(&send_stream, pending_read_size_, &pending_read_size_);
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - size read: " << pending_read_size_;
    pending_read_size_ = 0;
  } else {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - waiting for remote read";
  }

  if (BSCF_LASTDATANOTIFICATION & flags) {
    DLOG(INFO) << StringPrintf("URL: %s Obj: %X", url().c_str(), this) <<
        " - end of data.";
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
    std::string http_headers = net::HttpUtil::AssembleRawHeaders(
        raw_headers.c_str(), raw_headers.length());
    if (http_utils::HasFrameBustingHeader(http_headers)) {
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
  if (moniker_.get() == NULL) {
    DLOG(INFO) << "Creating a new moniker for " << url();
    std::wstring wide_url = UTF8ToWide(url());
    hr = CreateURLMonikerEx(NULL, wide_url.c_str(), moniker_.Receive(),
                            URL_MK_UNIFORM);
    if (FAILED(hr)) {
      NOTREACHED() << "CreateURLMonikerEx failed. Error: " << hr;
    } else {
      hr = CreateAsyncBindCtx(0, this, NULL, bind_context_.Receive());
      DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtx failed. Error: " << hr;
    }
  } else {
    DCHECK(bind_context_.get() != NULL);
    hr = S_OK;
  }

  if (SUCCEEDED(hr)) {
    ScopedComPtr<IStream> stream;

    // BindToStorage may complete synchronously.
    // We still get all the callbacks - OnStart/StopBinding, this may result
    // in destruction of our object. It's fine but we access some members
    // below for debug info. :)
    ScopedComPtr<IHttpSecurity> self(this);
    hr = moniker_->BindToStorage(bind_context_, NULL, __uuidof(IStream),
                                 reinterpret_cast<void**>(stream.Receive()));
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
  if (bind_context_) {
    ::RevokeBindStatusCallback(bind_context_, this);
    bind_context_.Release();
  }
}

//
// UrlmonUrlRequest::Cache implementation.
//

size_t UrlmonUrlRequest::Cache::Size() const {
  return cache_.size();
}

bool UrlmonUrlRequest::Cache::Read(IStream* dest, size_t size,
                                   size_t* bytes_copied) {
  DLOG(INFO) << __FUNCTION__;
  if (!dest || !size || !is_valid()) {
    NOTREACHED();
    return false;
  }

  // Copy the data to the destination stream and remove it from our cache.
  size_t size_written = 0;
  size_t bytes_to_write = (size <= Size() ? size : Size());

  dest->Write(&cache_[0], bytes_to_write,
              reinterpret_cast<unsigned long*>(&size_written));  // NOLINT
  DCHECK(size_written == bytes_to_write);

  cache_.erase(cache_.begin(), cache_.begin() + bytes_to_write);

  if (bytes_copied)
    *bytes_copied = size_written;

  return (size_written != 0);
}

bool UrlmonUrlRequest::Cache::Append(IStream* source,
                                     size_t* bytes_copied) {
  if (!source) {
    NOTREACHED();
    return false;
  }

  HRESULT hr = S_OK;
  while (SUCCEEDED(hr)) {
    DWORD chunk_read = 0;  // NOLINT
    hr = source->Read(read_buffer_, sizeof(read_buffer_), &chunk_read);

    if (!chunk_read)
      break;

    std::copy(read_buffer_, read_buffer_ + chunk_read,
              back_inserter(cache_));

    if (bytes_copied)
      *bytes_copied += chunk_read;
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
  return true;
}

void UrlmonUrlRequestManager::UseRequestDataForUrl(RequestData* data,
                                                   const std::wstring& url) {
  DCHECK(data);
  DCHECK(request_data_for_url_.get() == NULL);
  request_data_for_url_.reset(new RequestDataForUrl(data, url));
}

void UrlmonUrlRequestManager::StartRequest(int request_id,
    const IPC::AutomationURLRequest& request_info) {
  DLOG(INFO) << __FUNCTION__;
  RequestDataForUrl* use_request = NULL;
  if (request_data_for_url_.get()) {
    if (GURL(request_data_for_url_->url()) == GURL(request_info.url)) {
      use_request = request_data_for_url_.release();
    }
  }

  bool posted = ExecuteInWorkerThread(FROM_HERE, NewRunnableMethod(this,
                    &UrlmonUrlRequestManager::StartRequestWorker,
                    request_id, request_info, use_request));
  if (!posted) {
    delete use_request;
  }
}

void UrlmonUrlRequestManager::StartRequestWorker(int request_id,
    const IPC::AutomationURLRequest& request_info,
    RequestDataForUrl* use_request) {
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  scoped_ptr<RequestDataForUrl> request_for_url(use_request);

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
  if (request_for_url.get()) {
    new_request->SetRequestData(request_for_url->request_data());
  }

  DCHECK(LookupRequest(request_id).get() == NULL);
  request_map_[request_id] = new_request;
  map_empty_.Reset();
  new_request->Start();
}

void UrlmonUrlRequestManager::ReadRequest(int request_id, int bytes_to_read) {
  DLOG(INFO) << __FUNCTION__;
  ExecuteInWorkerThread(FROM_HERE, NewRunnableMethod(this,
      &UrlmonUrlRequestManager::ReadRequestWorker, request_id, bytes_to_read));
}

void UrlmonUrlRequestManager::ReadRequestWorker(int request_id,
                                                int bytes_to_read) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id);
  // if zero, it may just have had network error.
  if (request) {
    request->Read(bytes_to_read);
  }
}

void UrlmonUrlRequestManager::EndRequest(int request_id) {
  DLOG(INFO) << __FUNCTION__;
  ExecuteInWorkerThread(FROM_HERE, NewRunnableMethod(this,
      &UrlmonUrlRequestManager::EndRequestWorker, request_id));
}

void UrlmonUrlRequestManager::EndRequestWorker(int request_id) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id);
  if (request) {
    request->Stop();
  }
}

void UrlmonUrlRequestManager::StopAll() {
  DLOG(INFO) << __FUNCTION__;
  do {
    AutoLock lock(worker_thread_access_);
    if (stopping_)
      return;

    stopping_ = true;

    if (!worker_thread_.IsRunning())
      return;

    // ExecuteInWorkerThread will check for stopping_. Hence post directly
    // to the worker thread.
    worker_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &UrlmonUrlRequestManager::StopAllWorker));
  } while (0);

  // Note we may not call worker_thread_.Stop() here. The MessageLoop's quit
  // task will be serialized after request::Stop tasks, but requests may
  // not quit immediately. CoUninitialize has a modal message loop, but it
  // does not help in this case.
  // Normally we call binding->Abort() and expect OnStopBinding() callback
  // where we inform UrlmonUrlRequestManager that request is dead.
  // The problem is that while waiting for OnStopBinding(), Quit Task may be
  // picked up and executed, thus exiting the thread.
  map_empty_.Wait();

  worker_thread_access_.Acquire();
  worker_thread_.Stop();
  worker_thread_access_.Release();
}

void UrlmonUrlRequestManager::StopAllWorker() {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  DCHECK_EQ(true, stopping_);

  std::vector<scoped_refptr<UrlmonUrlRequest> > request_list;
  // We copy the pending requests into a temporary vector as the Stop
  // function in the request could also try to delete the request from
  // the request map and the iterator could end up being invalid.
  for (RequestMap::iterator it = request_map_.begin();
       it != request_map_.end(); ++it) {
    DCHECK(it->second != NULL);
    request_list.push_back(it->second);
  }

  for (std::vector<scoped_refptr<UrlmonUrlRequest> >::size_type index = 0;
       index < request_list.size(); ++index) {
    request_list[index]->Stop();
  }
}

void UrlmonUrlRequestManager::OnResponseStarted(int request_id,
    const char* mime_type, const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  DCHECK(LookupRequest(request_id).get() != NULL);
  delegate_->OnResponseStarted(request_id, mime_type, headers, size,
      last_modified, redirect_url, redirect_status);
}

void UrlmonUrlRequestManager::OnReadComplete(int request_id, const void* buffer,
                                             int len) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  DCHECK(LookupRequest(request_id).get() != NULL);
  delegate_->OnReadComplete(request_id, buffer, len);
}

void UrlmonUrlRequestManager::OnResponseEnd(int request_id,
                                            const URLRequestStatus& status) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK_EQ(worker_thread_.thread_id(), PlatformThread::CurrentId());
  RequestMap::size_type n = request_map_.erase(request_id);
  DCHECK_EQ(1, n);

  if (request_map_.size() == 0)
    map_empty_.Signal();

  // Inform delegate unless the request has been explicitly cancelled.
  if (status.status() != URLRequestStatus::CANCELED)
    delegate_->OnResponseEnd(request_id, status);
}

scoped_refptr<UrlmonUrlRequest> UrlmonUrlRequestManager::LookupRequest(
    int request_id) {
  RequestMap::iterator it = request_map_.find(request_id);
  if (request_map_.end() != it)
    return it->second;
  return NULL;
}

UrlmonUrlRequestManager::UrlmonUrlRequestManager()
    : stopping_(false), worker_thread_("UrlMon fetch thread"),
      map_empty_(true, true), notification_window_(NULL),
      privileged_mode_(false) {
}

UrlmonUrlRequestManager::~UrlmonUrlRequestManager() {
  StopAll();
}

// Called from UI thread.
void UrlmonUrlRequestManager::StealMonikerFromRequest(int request_id,
                                                      IMoniker** moniker) {
  base::WaitableEvent done(true, false);
  bool posted = ExecuteInWorkerThread(FROM_HERE, NewRunnableMethod(this,
      &UrlmonUrlRequestManager::StealMonikerFromRequestWorker,
      request_id, moniker, &done));
  // Wait until moniker is grabbed from a request in the worker thread.
  if (posted)
    done.Wait();
}

void UrlmonUrlRequestManager::StealMonikerFromRequestWorker(int request_id,
    IMoniker** moniker, base::WaitableEvent* done) {
  if (!stopping_) {
    scoped_refptr<UrlmonUrlRequest> request = LookupRequest(request_id);
    if (request) {
      request->StealMoniker(moniker);
      request->Stop();
    }
  }

  done->Signal();
}

bool UrlmonUrlRequestManager::ExecuteInWorkerThread(
    const tracked_objects::Location& from_here, Task* task) {
  AutoLock lock(worker_thread_access_);
  if (stopping_) {
    delete task;
    return false;
  }

  if (!worker_thread_.IsRunning())
    worker_thread_.Start();

  worker_thread_.message_loop()->PostTask(from_here, task);
  return true;
}

void UrlmonUrlRequestManager::AddPrivacyDataForUrl(
    const std::string& url, const std::string& policy_ref,
    int32 flags) {
  bool fire_privacy_event = false;

  {
    AutoLock lock(privacy_info_lock_);

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
  }

  if (fire_privacy_event && IsWindow(notification_window_)) {
    PostMessage(notification_window_, WM_FIRE_PRIVACY_CHANGE_NOTIFICATION, 1,
                0);
  }
}

