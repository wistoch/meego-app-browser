// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_UTILS_H_

#include <string>

#include <atlbase.h>
#include <atlcom.h>

#include "base/file_path.h"

extern const wchar_t kChromeFrameDllName[];

// Helper class used to register different chrome frame DLLs while running
// tests. The default constructor registers the DLL found in the build path.

// At destruction, again registers the DLL found in the build path if another
// DLL has since been registered. Triggers GTEST asserts on failure.
//
// TODO(robertshield): Ideally, make this class restore the originally
// registered chrome frame DLL (e.g. by looking in HKCR) on destruction.
class ScopedChromeFrameRegistrar {
 public:
  ScopedChromeFrameRegistrar();
  ScopedChromeFrameRegistrar(const std::wstring& path);
  virtual ~ScopedChromeFrameRegistrar();

  void RegisterChromeFrameAtPath(const std::wstring& path);
  void RegisterReferenceChromeFrameBuild();

  std::wstring GetChromeFrameDllPath() const;

  static FilePath GetChromeFrameBuildPath();
  static void RegisterAtPath(const std::wstring& path);
  static void RegisterDefaults();

 private:
  // Contains the path of the most recently registered Chrome Frame DLL.
  std::wstring new_chrome_frame_dll_path_;

  // Contains the path of the Chrome Frame DLL to be registered at destruction.
  std::wstring original_dll_path_;
};

// Callback description for onload, onloaderror, onmessage
static _ATL_FUNC_INFO g_single_param = {CC_STDCALL, VT_EMPTY, 1, {VT_VARIANT}};
// Simple class that forwards the callbacks.
template <typename T>
class DispCallback
    : public IDispEventSimpleImpl<1, DispCallback<T>, &IID_IDispatch> {
 public:
  typedef HRESULT (T::*Method)(const VARIANT* param);

  DispCallback(T* owner, Method method) : owner_(owner), method_(method) {
  }

  BEGIN_SINK_MAP(DispCallback)
    SINK_ENTRY_INFO(1, IID_IDispatch, DISPID_VALUE, OnCallback, &g_single_param)
  END_SINK_MAP()

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return owner_->AddRef();
  }
  virtual ULONG STDMETHODCALLTYPE Release() {
    return owner_->Release();
  }

  STDMETHOD(OnCallback)(VARIANT param) {
    return (owner_->*method_)(&param);
  }

  IDispatch* ToDispatch() {
    return reinterpret_cast<IDispatch*>(this);
  }

  T* owner_;
  Method method_;
};

// Kills all running processes named |process_name| that have the string
// |argument| on their command line. Useful for killing all Chrome Frame
// instances of Chrome that all have --chrome-frame in their command line.
bool KillAllNamedProcessesWithArgument(const std::wstring& process_name,
                                       const std::wstring& argument);


#endif  // CHROME_FRAME_TEST_UTILS_H_
