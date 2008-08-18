// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBKIT_GLUE_GLUE_UTIL_H_
#define WEBKIT_GLUE_GLUE_UTIL_H_

#include <string>

#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace WebCore {
  class CString;
  class DeprecatedString;
  class KURL;
  class String;
}

namespace webkit_glue {
  std::string CStringToStdString(const WebCore::CString& str);
  WebCore::CString StdStringToCString(const std::string& str);
  std::wstring StringToStdWString(const WebCore::String& str);
  std::string16 StringToStdString16(const WebCore::String& str);

  WebCore::String StdWStringToString(const std::wstring& str);
  WebCore::String StdStringToString(const std::string& str);
  
  WebCore::DeprecatedString StdWStringToDeprecatedString(const std::wstring& str);
  std::wstring DeprecatedStringToStdWString(const WebCore::DeprecatedString& dep);

  GURL KURLToGURL(const WebCore::KURL& url);
  WebCore::KURL GURLToKURL(const GURL& url);
}

#endif  // #ifndef WEBKIT_GLUE_GLUE_UTIL_H_
