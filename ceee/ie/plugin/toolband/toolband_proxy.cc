// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ceee/ie/plugin/toolband/toolband_proxy.h"

#include <atlbase.h>
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/win/registry.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/plugin/toolband/resource.h"

#include "toolband.h"  // NOLINT

extern "C" {
// Declare the entrypoint function(s) generated by MIDL for our proxy/stubs.
HRESULT STDAPICALLTYPE ToolbandProxyDllGetClassObject(REFCLSID clsid,
                                                      REFIID iid,
                                                      void** ppv);
}  // extern "C"

namespace {

struct InterfaceInfo {
  const wchar_t* name;
  const IID* iid;
  const IID* async_iid;
};

#define DECLARE_SYNC_INTERFACE(itf) \
  { L#itf, &IID_##itf, NULL },
#define DECLARE_ASYNC_INTERFACE(itf) \
  { L#itf, &IID_##itf, &IID_Async##itf }, \
  DECLARE_SYNC_INTERFACE(Async##itf)

// If you add new executor interfaces to toolband.idl, make sure to add
// their IIDs here, or you will not be able to marshal them.
const InterfaceInfo kInterfaceInfo[] = {
  DECLARE_SYNC_INTERFACE(ICeeeWindowExecutor)
  DECLARE_ASYNC_INTERFACE(ICeeeTabExecutor)
  DECLARE_SYNC_INTERFACE(ICeeeCookieExecutor)
  DECLARE_SYNC_INTERFACE(ICeeeInfobarExecutor)
};

#ifndef NDEBUG
void CheckAsyncIidRegistered(const IID& iid, const IID& async_iid) {
  std::wstring iid_str;
  bool success = com::GuidToString(iid, &iid_str);
  DCHECK(success);
  std::wstring key_name = base::StringPrintf(
      L"Interface\\%ls\\AsynchronousInterface", iid_str);

  base::win::RegKey key;
  if (key.Open(HKEY_CLASSES_ROOT, key_name.c_str(), KEY_READ)) {
    // It's registered, the rest of this block is debug checking that
    // the correct IID is indeed registered for the async interface.
    std::wstring async_iid_str;
    DCHECK(key.ReadValue(NULL, &async_iid_str));
    IID read_async_iid;
    DCHECK(SUCCEEDED(::IIDFromString(async_iid_str.c_str(), &read_async_iid)));
    DCHECK(read_async_iid == async_iid);
  } else {
    LOG(WARNING) << "Sync->Async IID not registered. Key=" << key_name;
  }
}
#endif  // NDEBUG

}  // namespace

bool RegisterProxyStubs(std::vector<DWORD>* cookies) {
  bool succeeded = true;

  for (size_t i = 0; i < arraysize(kInterfaceInfo); ++i) {
    CComPtr<IUnknown> factory;
    const InterfaceInfo& info = kInterfaceInfo[i];
    HRESULT hr = ToolbandProxyDllGetClassObject(*info.iid, IID_IUnknown,
        reinterpret_cast<void**>(&factory));

    DWORD cookie = 0;
    if (SUCCEEDED(hr)) {
      hr = ::CoRegisterClassObject(*info.iid, factory, CLSCTX_INPROC_SERVER,
          REGCLS_MULTIPLEUSE, &cookie);
    }
    if (SUCCEEDED(hr)) {
      // Proxy/stubs have their own IID as class id.
      hr = ::CoRegisterPSClsid(*info.iid, *info.iid);
      if (SUCCEEDED(hr) && cookies != NULL) {
        cookies->push_back(cookie);
      }
    }

#ifndef NDEBUG
    // If there's a corresponding Async interface, check whether
    // it's registered. This is a debugging aid only.
    if (info.async_iid != NULL)
      CheckAsyncIidRegistered(*info.iid, *info.async_iid);
#endif  // NDEBUG

    if (FAILED(hr)) {
      succeeded = false;
      LOG(ERROR) << "Failed to register proxy " << com::LogHr(hr);
    }
  }

  VLOG(1) << "Registered toolband proxy/stubs in thread "
      << ::GetCurrentThreadId();

  return succeeded;
}

void UnregisterProxyStubs(const std::vector<DWORD>& cookies) {
  for (size_t i = 0; i < cookies.size(); ++i) {
    HRESULT hr = ::CoRevokeClassObject(cookies[i]);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to revoke class object " << com::LogHr(hr);
    }
  }
}

bool RegisterAsyncProxies(bool reg) {
  for (size_t i = 0; i < arraysize(kInterfaceInfo); ++i) {
    // Register the iid->async iid mapping for async interfaces.
    if (kInterfaceInfo[i].async_iid != NULL) {
      const InterfaceInfo& info = kInterfaceInfo[i];
      std::wstring iid_str;
      bool success = com::GuidToString(*info.iid, &iid_str);
      DCHECK(success) << "Failed to stringify GUID";
      std::wstring async_iid_str;
      success = com::GuidToString(*info.async_iid, &async_iid_str);
      DCHECK(success) << "Failed to stringify GUID";

      _ATL_REGMAP_ENTRY entries[] = {
        { L"IID", iid_str.c_str() },
        { L"ASYNC_IID", async_iid_str.c_str() },
        { L"NAME", info.name },
        { NULL, NULL },
      };

      HRESULT hr = _pAtlModule->UpdateRegistryFromResource(
           MAKEINTRESOURCE(IDR_TOOLBAND_PROXY), reg, entries);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to register async interface for " <<
            info.name << com::LogHr(hr);
        return false;
      }
    }
  }

  return true;
}
