// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8_binding.h"

#include "AtomicString.h"
#include "CString.h"
#include "MathExtras.h"
#include "PlatformString.h"
#include "StringBuffer.h"

#include <v8.h>

namespace WebCore {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource: public v8::String::ExternalStringResource {
 public:
  explicit WebCoreStringResource(const String& str)
      : impl_(str.impl()) {
    // We seem to be occasionally losing the backing string for external
    // strings: http://crbug.com/9746
    //
    // In order to verify that this is caused by a ref counting bug, we
    // artificially increase the ref count on the backing string until
    // we are done using it for external strings.
    //
    // TODO(ager): This is temporary and should be removed once we have
    // found the underlying cause of the problem.
    for (int i = 0; i < kArtificialRefIncrease; i++) {
      impl_.impl()->ref();
    }
  }

  virtual ~WebCoreStringResource() {
    // Remove the artificial ref counts added in the constructor.
    //
    // TODO(ager): This is temporary and should be removed once we have
    // found the underlying cause of the problem.
    for (int i = 0; i < kArtificialRefIncrease; i++) {
      impl_.impl()->deref();
    }
  }

  const uint16_t* data() const {
    return reinterpret_cast<const uint16_t*>(impl_.characters());
  }

  size_t length() const { return impl_.length(); }

  String webcore_string() { return impl_; }

 private:
  // The amount by which we artificially increase the reference count
  // of the backing string.
  //
  // TODO(ager): This is temporary and should be removed once we have
  // found the underlying cause of the problem.
  static const int kArtificialRefIncrease = 5;

  // A shallow copy of the string.
  // Keeps the string buffer alive until the V8 engine garbage collects it.
  String impl_;
};

String v8StringToWebCoreString(
    v8::Handle<v8::String> v8_str, bool externalize) {
  if (v8_str->IsExternal()) {
    WebCoreStringResource* str_resource = static_cast<WebCoreStringResource*>(
        v8_str->GetExternalStringResource());
    return str_resource->webcore_string();
  }

  int length = v8_str->Length();
  if (length == 0) {
    // Avoid trying to morph empty strings, as they do not have enough room to
    // contain the external reference.
    return "";
  }

  // Copy the characters from the v8::String into a WebCore::String and allocate
  // an external resource which will be attached to the v8::String.
  String result;
  const int kStringSizeToCopy = 256;
  if (length < kStringSizeToCopy) {
    uint16_t buffer[kStringSizeToCopy];
    v8_str->Write(buffer, 0, length);
    result = StringImpl::create(reinterpret_cast<UChar*>(buffer), length);
  } else {
    StringBuffer buf(length);
    v8_str->Write(reinterpret_cast<uint16_t*>(buf.characters()), 0, length);
    result = String::adopt(buf);
  }

//
//  TODO(mbelshe): Disable string morphing because it causes mystery
//     perf regressions on intl1 and intl2 page cyclers.  It works fine
//     on machines other than the buildbots.

  if (externalize) {
    WebCoreStringResource* resource = new WebCoreStringResource(result);
    if (!v8_str->MakeExternal(resource)) {
      // In case of a failure delete the external resource as it was not used.
      delete resource;
    }
  }
  return result;
}


String v8ValueToWebCoreString(v8::Handle<v8::Value> obj) {
  v8::Handle<v8::String> v8_str;
  if (obj->IsString()) {
    v8_str = v8::Handle<v8::String>::Cast(obj);
    return v8StringToWebCoreString(v8_str, true);
  } else {
    v8::TryCatch block;
    v8_str = obj->ToString();
    if (v8_str.IsEmpty())
      return "";
  }
  return v8StringToWebCoreString(v8_str, false);
}


AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> v8_str) {
  String str = v8StringToWebCoreString(v8_str, true);
  return AtomicString(str);
}


AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> v8_str) {
  String str = v8ValueToWebCoreString(v8_str);
  return AtomicString(str);
}


v8::Handle<v8::String> v8String(const String& str) {
  if (!str.length())
    return v8::String::Empty();
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

v8::Local<v8::String> v8ExternalString(const String& str) {
  if (!str.length())
    return v8::String::Empty();
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

}  // namespace WebCore
