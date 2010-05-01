// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_moniker.h"

#include <shlguid.h>

#include "base/string_util.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/chrome_active_document.h"
#include "chrome_frame/urlmon_bind_status_callback.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "net/http/http_util.h"

static const int kMonikerBindToObject = 8;
static const int kMonikerBindToStorage = kMonikerBindToObject + 1;

base::LazyInstance<base::ThreadLocalPointer<NavigationManager> >
    NavigationManager::thread_singleton_(base::LINKER_INITIALIZED);

BEGIN_VTABLE_PATCHES(IMoniker)
  VTABLE_PATCH_ENTRY(kMonikerBindToObject, MonikerPatch::BindToObject)
  VTABLE_PATCH_ENTRY(kMonikerBindToStorage, MonikerPatch::BindToStorage)
END_VTABLE_PATCHES()

////////////////////////////

HRESULT NavigationManager::NavigateToCurrentUrlInCF(IBrowserService* browser) {
  DCHECK(browser);
  DLOG(INFO) << __FUNCTION__ << " " << url();

  MarkBrowserOnThreadForCFNavigation(browser);

  HRESULT hr = S_OK;
  ScopedComPtr<IShellBrowser> shell_browser;
  ScopedComPtr<IBindCtx> bind_context;
  hr = ::CreateAsyncBindCtxEx(NULL, 0, NULL, NULL, bind_context.Receive(), 0);

  ScopedComPtr<IMoniker> moniker;
  DCHECK(bind_context);
  if (SUCCEEDED(hr) &&
      SUCCEEDED(hr = ::CreateURLMonikerEx(NULL, url_.c_str(), moniker.Receive(),
                                          URL_MK_UNIFORM))) {
    if (SUCCEEDED(hr)) {
      // If there's a referrer, preserve it.
      std::wstring headers;
      if (!referrer_.empty()) {
        headers = StringPrintf(L"Referer: %ls\r\n\r\n",
            ASCIIToWide(referrer_).c_str());
      }

      // Pass in URL fragments if applicable.
      std::wstring fragment;
      GURL parsed_moniker_url(url_);
      if (parsed_moniker_url.has_ref()) {
        fragment = UTF8ToWide(parsed_moniker_url.ref());
      }

      hr = NavigateBrowserToMoniker(browser, moniker, headers.c_str(),
          bind_context, fragment.c_str());
      DLOG(INFO) << StringPrintf("NavigateBrowserToMoniker: 0x%08X", hr);
    }
  }

  return hr;
}

bool NavigationManager::IsTopLevelUrl(const wchar_t* url) {
  return CompareUrlsWithoutFragment(url_.c_str(), url);
}

/////////////////////////////////////////

NavigationManager* NavigationManager::GetThreadInstance() {
  return thread_singleton_.Pointer()->Get();
}

void NavigationManager::RegisterThreadInstance() {
  DCHECK(GetThreadInstance() == NULL);
  thread_singleton_.Pointer()->Set(this);
}

void NavigationManager::UnregisterThreadInstance() {
  DCHECK(GetThreadInstance() == this);
  thread_singleton_.Pointer()->Set(NULL);
}

/////////////////////////////////////////

// static
bool MonikerPatch::Initialize() {
  if (IS_PATCHED(IMoniker)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return true;
  }

  ScopedComPtr<IMoniker> moniker;
  HRESULT hr = ::CreateURLMoniker(NULL, L"http://localhost/",
                                  moniker.Receive());
  DCHECK(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    hr = vtable_patch::PatchInterfaceMethods(moniker, IMoniker_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr)) << StringPrintf("patch failed 0x%08X", hr);
  }

  return SUCCEEDED(hr);
}

// static
void MonikerPatch::Uninitialize() {
  vtable_patch::UnpatchInterfaceMethods(IMoniker_PatchInfo);
}

bool ShouldWrapCallback(IMoniker* moniker, REFIID iid, IBindCtx* bind_context) {
  CComHeapPtr<WCHAR> url;
  HRESULT hr = moniker->GetDisplayName(bind_context, NULL, &url);
  if (!url) {
    DLOG(INFO) << __FUNCTION__ << StringPrintf(
        " GetDisplayName failed. Error: 0x%x", hr);
    return false;
  }

  if (!IsEqualIID(IID_IStream, iid)) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: IID is not IStream.";
    return false;
  }

  scoped_refptr<BindContextInfo> info =
      BindContextInfo::FromBindContext(bind_context);
  DCHECK(info);
  if (info && info->chrome_request()) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: request from chrome frame.";
    return false;
  }

  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (!mgr) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " No navitagion manager to wrap";
    return false;
  }

  // TODO(ananta)
  // Use the IsSubFrameRequest function to determine if a request is a top
  // level request. Something like this.
  // ScopedComPtr<IUnknown> bscb_holder;
  // bind_context->GetObjectParam(L"_BSCB_Holder_", bscb_holder.Receive());
  // if (bscb_holder) {
  //   ScopedComPtr<IHttpNegotiate> http_negotiate;
  //   http_negotiate.QueryFrom(bscb_holder);
  //   if (http_negotiate && !IsSubFrameRequest(http_negotiate))
  //     return true;
  //  }
  // There are some cases where the IsSubFrameRequest function can return
  // incorrect results.
  bool should_wrap = mgr->IsTopLevelUrl(url);
  if (!should_wrap) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: Not top level url.";
  }
  return should_wrap;
}

// static
HRESULT MonikerPatch::BindToObject(IMoniker_BindToObject_Fn original,
                                   IMoniker* me, IBindCtx* bind_ctx,
                                   IMoniker* to_left, REFIID iid, void** obj) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(to_left == NULL);

  ExceptionBarrierReportOnlyModule barrier;

  HRESULT hr = S_OK;
  // Bind context is marked for switch when we sniff data in BSCBStorageBind
  // and determine that the renderer to be used is Chrome.
  scoped_refptr<BindContextInfo> info =
      BindContextInfo::FromBindContext(bind_ctx);
  DCHECK(info);
  if (info) {
    if (info->is_switching()) {
      // We could implement the BindToObject ourselves here but instead we
      // simply register Chrome Frame ActiveDoc as a handler for 'text/html'
      // in this bind context.  This makes urlmon instantiate CF Active doc
      // instead of mshtml.
      char* media_types[] = { "text/html" };
      CLSID classes[] = { CLSID_ChromeActiveDocument };
      hr = RegisterMediaTypeClass(bind_ctx, arraysize(media_types), media_types,
                                  classes, 0);
    } else {
      // In case the binding begins with BindToObject we do not need
      // to cache the data in the sniffing code.
      info->set_no_cache(true);
    }
  }

  hr = original(me, bind_ctx, to_left, iid, obj);
  return hr;
}

// static
HRESULT MonikerPatch::BindToStorage(IMoniker_BindToStorage_Fn original,
                                    IMoniker* me, IBindCtx* bind_ctx,
                                    IMoniker* to_left, REFIID iid, void** obj) {
  DCHECK(to_left == NULL);

  HRESULT hr = S_OK;
  CComObject<BSCBStorageBind>* callback = NULL;
  if (ShouldWrapCallback(me, iid, bind_ctx)) {
    hr = CComObject<BSCBStorageBind>::CreateInstance(&callback);
    callback->AddRef();
    hr = callback->Initialize(me, bind_ctx);
    DCHECK(SUCCEEDED(hr));

    // Report all crashes in the exception handler if we wrap the callback.
    // Note that this avoids having the VEH report a crash if an SEH earlier in
    // the chain handles the exception.
    ExceptionBarrier barrier;
    hr = original(me, bind_ctx, to_left, iid, obj);
  } else {
    // If we don't wrap, only report a crash if the crash is in our own module.
    ExceptionBarrierReportOnlyModule barrier;
    hr = original(me, bind_ctx, to_left, iid, obj);
  }

  // If the binding terminates before the data could be played back
  // now is the chance. Sometimes OnStopBinding happens after this returns
  // and then it's too late.
  if ((S_OK == hr) && callback)
    callback->MayPlayBack(BSCF_LASTDATANOTIFICATION);
  return hr;
}

