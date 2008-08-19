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

#include <string>

#include "net/base/mime_util.h"
#include "net/base/platform_mime_util.h"

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"

using std::string;
using std::wstring;

namespace net {

// Singleton utility class for mime types.
class MimeUtil : public PlatformMimeUtil {
 public:
  bool GetMimeTypeFromExtension(const std::wstring& ext,
                                std::string* mime_type) const;

  bool GetMimeTypeFromFile(const std::wstring& file_path,
                           std::string* mime_type) const;

  bool IsSupportedImageMimeType(const char* mime_type) const;
  bool IsSupportedNonImageMimeType(const char* mime_type) const;
  bool IsSupportedJavascriptMimeType(const char* mime_type) const;

  bool IsViewSourceMimeType(const char* mime_type) const;

  bool IsSupportedMimeType(const std::string& mime_type) const;

  bool MatchesMimeType(const std::string &mime_type_pattern,
                       const std::string &mime_type) const;

private:
  friend struct DefaultSingletonTraits<MimeUtil>;
  MimeUtil() {
    InitializeMimeTypeMaps();
  }

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  typedef base::hash_set<std::string> MimeMappings;
  MimeMappings image_map_;
  MimeMappings non_image_map_;
  MimeMappings javascript_map_;
  MimeMappings view_source_map_;
}; // class MimeUtil

struct MimeInfo {
  const char* mime_type;
  const char* extensions;  // comma separated list
};

static const MimeInfo primary_mappings[] = {
  { "text/html", "html,htm" },
  { "text/css", "css" },
  { "text/xml", "xml" },
  { "image/gif", "gif" },
  { "image/jpeg", "jpeg,jpg" },
  { "image/png", "png" },
  { "application/xhtml+xml", "xhtml,xht" }
};

static const MimeInfo secondary_mappings[] = {
  { "application/octet-stream", "exe,com,bin" },
  { "application/gzip", "gz" },
  { "application/pdf", "pdf" },
  { "application/postscript", "ps,eps,ai" },
  { "application/x-javascript", "js" },
  { "image/bmp", "bmp" },
  { "image/x-icon", "ico" },
  { "image/jpeg", "jfif,pjpeg,pjp" },
  { "image/tiff", "tiff,tif" },
  { "image/x-xbitmap", "xbm" },
  { "image/svg+xml", "svg,svgz" },
  { "message/rfc822", "eml" },
  { "text/plain", "txt,text" },
  { "text/html", "shtml,ehtml" },
  { "application/rss+xml", "rss" },
  { "application/rdf+xml", "rdf" },
  { "text/xml", "xsl,xbl" },
  { "application/vnd.mozilla.xul+xml", "xul" },
  { "application/x-shockwave-flash", "swf,swl" }
};

static const char* FindMimeType(const MimeInfo* mappings,
                                size_t mappings_len,
                                const char* ext) {
  size_t ext_len = strlen(ext);

  for (size_t i = 0; i < mappings_len; ++i) {
    const char* extensions = mappings[i].extensions;
    for (;;) {
      size_t end_pos = strcspn(extensions, ",");
      if (end_pos == ext_len &&
          base::strncasecmp(extensions, ext, ext_len) == 0)
        return mappings[i].mime_type;
      extensions += end_pos;
      if (!*extensions)
        break;
      extensions += 1;  // skip over comma
    }
  }
  return NULL;
}

bool MimeUtil::GetMimeTypeFromExtension(const wstring& ext,
                                        string* result) const {
  // We implement the same algorithm as Mozilla for mapping a file extension to
  // a mime type.  That is, we first check a hard-coded list (that cannot be
  // overridden), and then if not found there, we defer to the system registry.
  // Finally, we scan a secondary hard-coded list to catch types that we can
  // deduce but that we also want to allow the OS to override.

  string ext_utf8 = WideToUTF8(ext);
  const char* mime_type;

  mime_type = FindMimeType(primary_mappings, arraysize(primary_mappings),
                           ext_utf8.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  if (GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(secondary_mappings, arraysize(secondary_mappings),
                           ext_utf8.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  return false;
}

bool MimeUtil::GetMimeTypeFromFile(const wstring& file_path,
                                   string* result) const {
  // TODO(ericroman): this doesn't work properly with paths like
  // /home/foo/.ssh/known_hosts
  wstring::size_type dot = file_path.find_last_of('.');
  if (dot == wstring::npos)
    return false;
  return GetMimeTypeFromExtension(file_path.substr(dot + 1), result);
}

// From WebKit's WebCore/platform/MIMETypeRegistry.cpp:

static const char* supported_image_types[] = {
  "image/jpeg",
  "image/jpg",
  "image/png",
  "image/gif",
  "image/bmp",
  "image/x-icon",    // ico
  "image/x-xbitmap"  // xbm
};

// Note: does not include javascript types list (see supported_javascript_types)
static const char* supported_non_image_types[] = {
  "text/html",
  "text/xml",
  "text/xsl",
  "text/plain",
  "text/",
  "image/svg+xml", // SVG is text-based XML, even though it has an image/ type
  "application/xml",
  "application/xhtml+xml",
  "application/rss+xml",
  "application/atom+xml",
  "multipart/x-mixed-replace"
};

//  Mozilla 1.8 and WinIE 7 both accept text/javascript and text/ecmascript.
//  Mozilla 1.8 accepts application/javascript, application/ecmascript, and application/x-javascript, but WinIE 7 doesn't.
//  WinIE 7 accepts text/javascript1.1 - text/javascript1.3, text/jscript, and text/livescript, but Mozilla 1.8 doesn't.
//  Mozilla 1.8 allows leading and trailing whitespace, but WinIE 7 doesn't.
//  Mozilla 1.8 and WinIE 7 both accept the empty string, but neither accept a whitespace-only string.
//  We want to accept all the values that either of these browsers accept, but not other values.
static const char* supported_javascript_types[] = {
  "text/javascript",
  "text/ecmascript",
  "application/javascript",
  "application/ecmascript",
  "application/x-javascript",
  "text/javascript1.1",
  "text/javascript1.2",
  "text/javascript1.3",
  "text/jscript",
  "text/livescript"
};

static const char* view_source_types[] = {
  "text/xml",
  "text/xsl",
  "application/xml",
  "application/rss+xml",
  "application/atom+xml",
  "image/svg+xml"
};

void MimeUtil::InitializeMimeTypeMaps() {
  for (int i = 0; i < arraysize(supported_image_types); ++i)
    image_map_.insert(supported_image_types[i]);

  // Initialize the supported non-image types
  for (int i = 0; i < arraysize(supported_non_image_types); ++i)
    non_image_map_.insert(supported_non_image_types[i]);
  for (int i = 0; i < arraysize(supported_javascript_types); ++i)
    non_image_map_.insert(supported_javascript_types[i]);

  for (int i = 0; i < arraysize(supported_javascript_types); ++i)
    javascript_map_.insert(supported_javascript_types[i]);

  for (int i = 0; i < arraysize(view_source_types); ++i)
    view_source_map_.insert(view_source_types[i]);
}

bool MimeUtil::IsSupportedImageMimeType(const char* mime_type) const {
  return image_map_.find(mime_type) != image_map_.end();
}

bool MimeUtil::IsSupportedNonImageMimeType(const char* mime_type) const {
  return non_image_map_.find(mime_type) != non_image_map_.end();
}

bool MimeUtil::IsSupportedJavascriptMimeType(const char* mime_type) const {
  return javascript_map_.find(mime_type) != javascript_map_.end();
}

bool MimeUtil::IsViewSourceMimeType(const char* mime_type) const {
  return view_source_map_.find(mime_type) != view_source_map_.end();
}

// Mirrors WebViewImpl::CanShowMIMEType()
bool MimeUtil::IsSupportedMimeType(const std::string& mime_type) const {
  if (mime_type.compare(0, 5, "text/") == 0 ||
      (mime_type.compare(0, 6, "image/") == 0 &&
       IsSupportedImageMimeType(mime_type.c_str())) ||
      IsSupportedNonImageMimeType(mime_type.c_str()))
    return true;
  return false;
}

bool MimeUtil::MatchesMimeType(const std::string &mime_type_pattern,
                               const std::string &mime_type) const {
  // verify caller is passing lowercase
  DCHECK(mime_type_pattern == StringToLowerASCII(mime_type_pattern));
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  // This comparison handles absolute maching and also basic
  // wildcards.  The plugin mime types could be:
  //      application/x-foo
  //      application/*
  //      application/*+xml
  //      *
  if (mime_type_pattern.empty())
    return false;

  const std::string::size_type star = mime_type_pattern.find('*');

  if (star == std::string::npos)
    return mime_type_pattern == mime_type;

  // Test length to prevent overlap between |left| and |right|.
  if (mime_type.length() < mime_type_pattern.length() - 1)
    return false;

  const std::string left(mime_type_pattern.substr(0, star));
  const std::string right(mime_type_pattern.substr(star + 1));

  if (mime_type.find(left) != 0)
    return false;

  if (!right.empty() &&
      mime_type.rfind(right) != mime_type.length() - right.length())
    return false;

  return true;
}

//----------------------------------------------------------------------------
// Wrappers for the singleton
//----------------------------------------------------------------------------

static MimeUtil* GetMimeUtil() {
  return Singleton<MimeUtil>::get();
}

bool GetMimeTypeFromExtension(const std::wstring& ext, std::string* mime_type) {
  return GetMimeUtil()->GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const std::wstring& file_path, std::string* mime_type) {
  return GetMimeUtil()->GetMimeTypeFromFile(file_path, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* extension) {
  return GetMimeUtil()->GetPreferredExtensionForMimeType(mime_type, extension);
}

bool IsSupportedImageMimeType(const char* mime_type) {
  return GetMimeUtil()->IsSupportedImageMimeType(mime_type);
}

bool IsSupportedNonImageMimeType(const char* mime_type) {
  return GetMimeUtil()->IsSupportedNonImageMimeType(mime_type);
}

bool IsSupportedJavascriptMimeType(const char* mime_type) {
  return GetMimeUtil()->IsSupportedJavascriptMimeType(mime_type);
}

bool IsViewSourceMimeType(const char* mime_type) {
  return GetMimeUtil()->IsViewSourceMimeType(mime_type);
}

bool IsSupportedMimeType(const std::string& mime_type) {
  return GetMimeUtil()->IsSupportedMimeType(mime_type);
}

bool MatchesMimeType(const std::string &mime_type_pattern,
                     const std::string &mime_type) {
  return GetMimeUtil()->MatchesMimeType(mime_type_pattern, mime_type);
}

}  // namespace net
