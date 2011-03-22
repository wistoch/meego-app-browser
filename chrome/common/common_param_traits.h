// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are shared by more than one child process.

#ifndef CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#define CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#pragma once

#include "base/file_util.h"
#include "base/ref_counted.h"
#include "chrome/common/content_settings.h"
#include "ipc/ipc_message_utils.h"
#include "printing/native_metafile.h"
// !!! WARNING: DO NOT ADD NEW WEBKIT DEPENDENCIES !!!
//
// That means don't add #includes to any file in 'webkit/' or
// 'third_party/WebKit/'. Chrome Frame and NACL build parts of base/ and
// chrome/common/ for a mini-library that doesn't depend on webkit.
//
// TODO(erg): The following headers are historical and only work because
// their definitions are inlined, which also needs to be fixed.
#include "webkit/glue/window_open_disposition.h"

// Forward declarations.
class SkBitmap;
class DictionaryValue;
class ListValue;
struct ThumbnailScore;
struct WebApplicationInfo;

namespace printing {
struct PageRange;
struct PrinterCapsAndDefaults;
}  // namespace printing

namespace webkit_glue {
struct PasswordForm;
}  // namespace webkit_glue

namespace IPC {

template <>
struct ParamTraits<SkBitmap> {
  typedef SkBitmap param_type;
  static void Write(Message* m, const param_type& p);

  // Note: This function expects parameter |r| to be of type &SkBitmap since
  // r->SetConfig() and r->SetPixels() are called.
  static bool Read(const Message* m, void** iter, param_type* r);

  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContentSetting> {
  typedef ContentSetting param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContentSettingsType> {
  typedef ContentSettingsType param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    if (value < 0 || value >= static_cast<int>(CONTENT_SETTINGS_NUM_TYPES))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<ContentSettings> {
  typedef ContentSettings param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WindowOpenDisposition> {
  typedef WindowOpenDisposition param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<WebApplicationInfo> {
  typedef WebApplicationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct ParamTraits<ThumbnailScore> {
  typedef ThumbnailScore param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<webkit_glue::PasswordForm> {
  typedef webkit_glue::PasswordForm param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::PageRange> {
  typedef printing::PageRange param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::NativeMetafile> {
  typedef printing::NativeMetafile param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::PrinterCapsAndDefaults> {
  typedef printing::PrinterCapsAndDefaults param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_COMMON_PARAM_TRAITS_H_
