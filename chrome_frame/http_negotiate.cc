// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/http_negotiate.h"

#include <atlbase.h>
#include <atlcom.h>
#include <htiframe.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

#include "chrome_frame/bho.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"

#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

const char kUACompatibleHttpHeader[] = "x-ua-compatible";

// From the latest urlmon.h. Symbol name prepended with LOCAL_ to
// avoid conflict (and therefore build errors) for those building with
// a newer Windows SDK.
// TODO(robertshield): Remove this once we update our SDK version.
static const int LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE = 54;

static const int kHttpNegotiateBeginningTransactionIndex = 3;
static const int kHttpNegotiateOnResponseTransactionIndex = 4;

BEGIN_VTABLE_PATCHES(IHttpNegotiate)
  VTABLE_PATCH_ENTRY(kHttpNegotiateBeginningTransactionIndex,
                     HttpNegotiatePatch::BeginningTransaction)
  VTABLE_PATCH_ENTRY(kHttpNegotiateOnResponseTransactionIndex,
                     HttpNegotiatePatch::OnResponse)
END_VTABLE_PATCHES()

static const int kBindStatusCallbackStartBindingIndex = 3;

BEGIN_VTABLE_PATCHES(IBindStatusCallback)
  VTABLE_PATCH_ENTRY(kBindStatusCallbackStartBindingIndex,
                     HttpNegotiatePatch::StartBinding)
END_VTABLE_PATCHES()

static const int kInternetProtocolSinkReportProgressIndex = 4;

BEGIN_VTABLE_PATCHES(IInternetProtocolSink)
  VTABLE_PATCH_ENTRY(kInternetProtocolSinkReportProgressIndex,
                     HttpNegotiatePatch::ReportProgress)
END_VTABLE_PATCHES()

namespace {

class SimpleBindStatusCallback : public CComObjectRootEx<CComSingleThreadModel>,
                                 public IBindStatusCallback {
 public:
  BEGIN_COM_MAP(SimpleBindStatusCallback)
    COM_INTERFACE_ENTRY(IBindStatusCallback)
  END_COM_MAP()

  // IBindStatusCallback implementation
  STDMETHOD(OnStartBinding)(DWORD reserved, IBinding* binding) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetPriority)(LONG* priority) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnLowResource)(DWORD reserved) {
    return E_NOTIMPL;
  }

  STDMETHOD(OnProgress)(ULONG progress, ULONG max_progress,
                        ULONG status_code, LPCWSTR status_text) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnStopBinding)(HRESULT result, LPCWSTR error) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetBindInfo)(DWORD* bind_flags, BINDINFO* bind_info) {
    return E_NOTIMPL;
  }

  STDMETHOD(OnDataAvailable)(DWORD flags, DWORD size, FORMATETC* formatetc,
    STGMEDIUM* storage) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnObjectAvailable)(REFIID iid, IUnknown* object) {
    return E_NOTIMPL;
  }
};

// Attempts to get to the associated browser service for an active request.
HRESULT GetBrowserServiceFromProtocolSink(IInternetProtocolSink* sink,
                                          IBrowserService** browser_service) {
  DCHECK(browser_service);
  // When fetching a page for the first time (not cached), we can query the
  // sink directly for IID_IShellBrowser to get the browser service.
  HRESULT hr = DoQueryService(IID_IShellBrowser, sink, browser_service);
  if (FAILED(hr)) {
    // When the request is being served up from the cache, we have to take
    // a different route via IID_ITargetFrame2.
    ScopedComPtr<IWebBrowser2> browser2;
    hr = DoQueryService(IID_ITargetFrame2, sink, browser2.Receive());
    if (browser2) {
      hr = DoQueryService(IID_IShellBrowser, browser2, browser_service);
    }
  }

  return hr;
}

}  // end namespace

HttpNegotiatePatch::HttpNegotiatePatch() {
}

HttpNegotiatePatch::~HttpNegotiatePatch() {
}

// static
bool HttpNegotiatePatch::Initialize() {
  if (IS_PATCHED(IHttpNegotiate)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return true;
  }
  // Use our SimpleBindStatusCallback class as we need a temporary object that
  // implements IBindStatusCallback.
  CComObjectStackEx<SimpleBindStatusCallback> request;
  ScopedComPtr<IBindCtx> bind_ctx;
  HRESULT hr = CreateAsyncBindCtx(0, &request, NULL, bind_ctx.Receive());
  DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtx";
  if (bind_ctx) {
    ScopedComPtr<IUnknown> bscb_holder;
    bind_ctx->GetObjectParam(L"_BSCB_Holder_", bscb_holder.Receive());
    if (bscb_holder) {
      hr = PatchHttpNegotiate(bscb_holder);
    } else {
      NOTREACHED() << "Failed to get _BSCB_Holder_";
      hr = E_UNEXPECTED;
    }
    bind_ctx.Release();
  }

  return SUCCEEDED(hr);
}

// static
void HttpNegotiatePatch::Uninitialize() {
  vtable_patch::UnpatchInterfaceMethods(IHttpNegotiate_PatchInfo);
}

// static
HRESULT HttpNegotiatePatch::PatchHttpNegotiate(IUnknown* to_patch) {
  DCHECK(to_patch);
  DCHECK_IS_NOT_PATCHED(IHttpNegotiate);

  ScopedComPtr<IHttpNegotiate> http;
  HRESULT hr = http.QueryFrom(to_patch);
  if (FAILED(hr)) {
    hr = DoQueryService(IID_IHttpNegotiate, to_patch, http.Receive());
  }

  if (http) {
    hr = vtable_patch::PatchInterfaceMethods(http, IHttpNegotiate_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr))
        << StringPrintf("HttpNegotiate patch failed 0x%08X", hr);
  } else {
    DLOG(WARNING)
        << StringPrintf("IHttpNegotiate not supported 0x%08X", hr);
  }

  ScopedComPtr<IBindStatusCallback> bscb;
  hr = bscb.QueryFrom(to_patch);

  if (bscb) {
    hr = vtable_patch::PatchInterfaceMethods(bscb,
                                             IBindStatusCallback_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr))
        << StringPrintf("BindStatusCallback patch failed 0x%08X", hr);
  } else {
    DLOG(WARNING) << StringPrintf("IBindStatusCallback not supported 0x%08X",
                                  hr);
  }
  return hr;
}

// static
HRESULT HttpNegotiatePatch::BeginningTransaction(
    IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
    LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << " " << url << " headers:\n" << headers;

  HRESULT hr = original(me, url, headers, reserved, additional_headers);

  if (FAILED(hr)) {
    DLOG(WARNING) << __FUNCTION__ << " Delegate returned an error";
    return hr;
  }

  ScopedComPtr<IWebBrowser2> browser2;
  DoQueryService(IID_ITargetFrame2, me, browser2.Receive());
  if (browser2) {
    NavigationManager* mgr = NavigationManager::GetThreadInstance();
    if (mgr) {
      VARIANT_BOOL is_top_level = VARIANT_FALSE;
      browser2->get_TopLevelContainer(&is_top_level);
      mgr->OnBeginningTransaction(is_top_level != VARIANT_FALSE, url, headers,
                                  *additional_headers);
      DLOG(INFO) << "called OnBeginningTransaction " << is_top_level;
    } else {
      DLOG(INFO) << "No NavigationManager";
    }
  } else {
    DLOG(INFO) << "No IWebBrowser2";
  }

  static const char kLowerCaseUserAgent[] = "user-agent";

  using net::HttpUtil;

  std::string ascii_headers;
  if (*additional_headers) {
    ascii_headers = WideToASCII(*additional_headers);
    DLOG(INFO) << __FUNCTION__ << " additional headers: " << ascii_headers;
  }

  HttpUtil::HeadersIterator headers_iterator(ascii_headers.begin(),
                                             ascii_headers.end(), "\r\n");
  std::string user_agent_value;
  if (headers_iterator.AdvanceTo(kLowerCaseUserAgent)) {
    user_agent_value = headers_iterator.values();
  } else if (headers != NULL) {
    // See if there's a user-agent header specified in the original headers.
    std::string original_headers(WideToASCII(headers));
    HttpUtil::HeadersIterator original_it(original_headers.begin(),
                                          original_headers.end(), "\r\n");
    if (original_it.AdvanceTo(kLowerCaseUserAgent))
      user_agent_value = original_it.values();
  }

  // Use the default one if none was provided.
  if (user_agent_value.empty())
    user_agent_value = http_utils::GetDefaultUserAgent();

  // Now add chromeframe to it.
  user_agent_value = http_utils::AddChromeFrameToUserAgentValue(
      user_agent_value);

  // Build new headers, skip the existing user agent value from
  // existing headers.
  std::string new_headers;
  headers_iterator.Reset();
  while (headers_iterator.GetNext()) {
    std::string name(headers_iterator.name());
    if (!LowerCaseEqualsASCII(name, kLowerCaseUserAgent)) {
      new_headers += name + ": " + headers_iterator.values() + "\r\n";
    }
  }

  new_headers += "User-Agent: " + user_agent_value;
  new_headers += "\r\n\r\n";

  if (*additional_headers)
    ::CoTaskMemFree(*additional_headers);
  *additional_headers = reinterpret_cast<wchar_t*>(::CoTaskMemAlloc(
      (new_headers.length() + 1) * sizeof(wchar_t)));
  lstrcpyW(*additional_headers, ASCIIToWide(new_headers).c_str());

  return hr;
}

// static
HRESULT HttpNegotiatePatch::OnResponse(IHttpNegotiate_OnResponse_Fn original,
    IHttpNegotiate* me, DWORD response_code, LPCWSTR response_header,
    LPCWSTR request_header, LPWSTR* additional_request_headers) {
  DLOG(INFO) << __FUNCTION__ << " headers: " << std::endl << response_header;

  HRESULT hr = original(me, response_code, response_header, request_header,
                        additional_request_headers);
  return hr;
}

// static
HRESULT HttpNegotiatePatch::StartBinding(
    IBindStatusCallback_StartBinding_Fn original,
    IBindStatusCallback* me, DWORD reserved, IBinding* binding) {
  ScopedComPtr<IBinding> local_binding(binding);
  ScopedComPtr<IInternetProtocolSink> protocol_sink;

  HRESULT hr = protocol_sink.QueryFrom(local_binding);
  if (FAILED(hr) || !protocol_sink) {
    DLOG(WARNING) << "Failed to get IInternetProtocolSink from IBinding: "
                  << hr;
  } else {
    if (!IS_PATCHED(IInternetProtocolSink)) {
      hr = vtable_patch::PatchInterfaceMethods(protocol_sink,
                                               IInternetProtocolSink_PatchInfo);
      DCHECK(SUCCEEDED(hr));
      // Now that we've gotten to the protocol sink,
      // we don't need this patch anymore.
      HRESULT hr_unpatch = vtable_patch::UnpatchInterfaceMethods(
          IBindStatusCallback_PatchInfo);
      DCHECK(SUCCEEDED(hr_unpatch));
    }

    DLOG_IF(WARNING, FAILED(hr))
        << "Failed to patch IInternetProtocolSink from IBinding: " << hr;
  }

  hr = original(me, reserved, binding);
  return hr;
}

// static
HRESULT HttpNegotiatePatch::ReportProgress(
    IInternetProtocolSink_ReportProgress_Fn original, IInternetProtocolSink* me,
    ULONG status_code, LPCWSTR status_text) {
  DLOG(INFO) << __FUNCTION__
      << StringPrintf(" %i %ls", status_code, status_text);
  if (status_code == BINDSTATUS_MIMETYPEAVAILABLE ||
      status_code == BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE ||
      status_code == LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE) {
    bool render_in_chrome_frame = false;
    bool is_top_level_request = !IsSubFrameRequest(me);
    // NOTE: After switching over to using the onhttpequiv notification from
    // mshtml we can expect to see sub frames being created even before the
    // owning document has completed loading.  In particular frames whose
    // source is about:blank.

    if (is_top_level_request) {
      ScopedComPtr<IBrowserService> browser;
      GetBrowserServiceFromProtocolSink(me, browser.Receive());
      if (browser) {
        render_in_chrome_frame = CheckForCFNavigation(browser, true);
      }

      DLOG_IF(INFO, !render_in_chrome_frame) << " - browser not tagged";

      if (!render_in_chrome_frame) {
        // Check to see if we need to alter the mime type that gets reported
        // by inspecting the raw header information:
        ScopedComPtr<IWinInetHttpInfo> win_inet_http_info;
        HRESULT hr = win_inet_http_info.QueryFrom(me);

        // Try slightly harder if we couldn't QI directly.
        if (!win_inet_http_info || FAILED(hr)) {
          hr = DoQueryService(IID_IWinInetHttpInfo, me,
                              win_inet_http_info.Receive());
          DLOG_IF(WARNING, FAILED(hr)) << "Failed to get IWinInetHttpInfo";
        }

        // Note that it has been observed that getting an IWinInetHttpInfo will
        // fail if we are loading a page like about:blank that isn't loaded via
        // wininet.
        if (win_inet_http_info) {
          // We have headers: check to see if the server is requesting CF via
          // the X-UA-Compatible: chrome=1 HTTP header.
          // TODO(tommi): use HTTP_QUERY_CUSTOM instead of fetching and parsing
          // all the headers.
          std::string headers(GetRawHttpHeaders(win_inet_http_info));
          net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(),
                                            "\r\n");
          while (it.GetNext()) {
            if (LowerCaseEqualsASCII(it.name_begin(), it.name_end(),
                                     kUACompatibleHttpHeader)) {
                std::string ua_value(StringToLowerASCII(it.values()));
                if (ua_value.find("chrome=1") != std::string::npos) {
                  render_in_chrome_frame = true;
                  break;
                }
            }
          }
        }
      }
    }

    if (render_in_chrome_frame) {
      DLOG(INFO) << "- changing mime type to " << kChromeMimeType;
      status_text = kChromeMimeType;
    }
  }

  return original(me, status_code, status_text);
}
