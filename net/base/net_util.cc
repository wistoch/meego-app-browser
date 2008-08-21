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

#include <algorithm>
#include <unicode/ucnv.h>
#include <unicode/uidna.h>
#include <unicode/ulocdata.h>
#include <unicode/uniset.h>
#include <unicode/uscript.h>
#include <unicode/uset.h>

#ifdef OS_WIN
#include <windows.h>
#include <wininet.h>
#endif

#include "net/base/net_util.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_escape.h"
#include "base/string_piece.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/time_format.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "net/base/escape.h"
#include "net/base/net_module.h"
#include "net/base/net_resources.h"
#include "net/base/base64.h"
#include "unicode/datefmt.h"

namespace {

// what we prepend to get a file URL
static const wchar_t kFileURLPrefix[] = L"file:///";

// The general list of blocked ports. Will be blocked unless a specific
// protocol overrides it. (Ex: ftp can use ports 20 and 21)
static const int kRestrictedPorts[] = {
  1,    // tcpmux
  7,    // echo
  9,    // discard
  11,   // systat
  13,   // daytime
  15,   // netstat
  17,   // qotd
  19,   // chargen
  20,   // ftp data
  21,   // ftp access
  22,   // ssh
  23,   // telnet
  25,   // smtp
  37,   // time
  42,   // name
  43,   // nicname
  53,   // domain
  77,   // priv-rjs
  79,   // finger
  87,   // ttylink
  95,   // supdup
  101,  // hostriame
  102,  // iso-tsap
  103,  // gppitnp
  104,  // acr-nema
  109,  // pop2
  110,  // pop3
  111,  // sunrpc
  113,  // auth
  115,  // sftp
  117,  // uucp-path
  119,  // nntp
  123,  // NTP
  135,  // loc-srv /epmap
  139,  // netbios
  143,  // imap2
  179,  // BGP
  389,  // ldap
  465,  // smtp+ssl
  512,  // print / exec
  513,  // login
  514,  // shell
  515,  // printer
  526,  // tempo
  530,  // courier
  531,  // chat
  532,  // netnews
  540,  // uucp
  556,  // remotefs
  563,  // nntp+ssl
  587,  // stmp?
  601,  // ??
  636,  // ldap+ssl
  993,  // ldap+ssl
  995,  // pop3+ssl
  2049, // nfs
  4045, // lockd
  6000, // X11
};

// FTP overrides the following restricted ports.
static const int kAllowedFtpPorts[] = {
  21,   // ftp data
  22,   // ssh
};

template<typename STR>
STR GetSpecificHeaderT(const STR& headers, const STR& name) {
  // We want to grab the Value from the "Key: Value" pairs in the headers,
  // which should look like this (no leading spaces, \n-separated) (we format
  // them this way in url_request_inet.cc):
  //    HTTP/1.1 200 OK\n
  //    ETag: "6d0b8-947-24f35ec0"\n
  //    Content-Length: 2375\n
  //    Content-Type: text/html; charset=UTF-8\n
  //    Last-Modified: Sun, 03 Sep 2006 04:34:43 GMT\n
  if (headers.empty())
    return STR();

  STR match;
  match.push_back('\n');
  match.append(name);
  match.push_back(':');

  typename STR::const_iterator begin =
      search(headers.begin(), headers.end(), match.begin(), match.end(),
             CaseInsensitiveCompareASCII<typename STR::value_type>());

  if (begin == headers.end())
    return STR();

  begin += match.length();

  typename STR::const_iterator end = find(begin, headers.end(), '\n');

  STR ret;
  TrimWhitespace(STR(begin, end), TRIM_ALL, &ret);
  return ret;
}

// TODO(jungshik): We have almost identical hex-decoding code else where.
// Consider refactoring and moving it somewhere(base?). Bug 1224311
inline bool IsHexDigit(unsigned char c) {
  return ('0' <= c && c <= '9' || 'A' <= c && c <= 'F' || 'a' <= c && c <= 'f');
}

inline unsigned char HexToInt(unsigned char c) {
  DCHECK(IsHexDigit(c));
  static unsigned char kOffset[4] = {0, 0x30u, 0x37u, 0x57u};
  return c - kOffset[c / 0x20];
}

// Similar to Base64Decode. Decodes a Q-encoded string to a sequence
// of bytes. If input is invalid, return false.
bool QPDecode(const std::string& input, std::string* output) {
  std::string temp;
  temp.reserve(input.size());
  std::string::const_iterator it = input.begin();
  while (it != input.end()) {
    if (*it == '_') {
      temp.push_back(' ');
    } else if (*it == '=') {
      if (input.end() - it < 3) {
        return false;
      }
      if (IsHexDigit(static_cast<unsigned char>(*(it + 1))) &&
          IsHexDigit(static_cast<unsigned char>(*(it + 2)))) {
        unsigned char ch = HexToInt(*(it + 1)) * 16 + HexToInt(*(it + 2));
        temp.push_back(static_cast<char>(ch));
        ++it;
        ++it;
      } else {
        return false;
      }
    } else if (0x20 < *it && *it < 0x7F) {
      // In a Q-encoded word, only printable ASCII characters
      // represent themselves. Besides, space, '=', '_' and '?' are
      // not allowed, but they're already filtered out.
      DCHECK(*it != 0x3D && *it != 0x5F && *it != 0x3F);
      temp.push_back(*it);
    } else {
      return false;
    }
    ++it;
  }
  output->swap(temp);
  return true;
}

enum RFC2047EncodingType {Q_ENCODING, B_ENCODING};
bool DecodeBQEncoding(const std::string& part, RFC2047EncodingType enc_type,
                       const std::string& charset, std::string* output) {
  std::string decoded;
  if (enc_type == B_ENCODING) {
    if (!net::Base64Decode(part, &decoded)) {
      return false;
    }
  } else {
    if (!QPDecode(part, &decoded)) {
      return false;
    }
  }

  UErrorCode err = U_ZERO_ERROR;
  UConverter* converter(ucnv_open(charset.c_str(), &err));
  if (U_FAILURE(err)) {
    return false;
  }

  // A single byte in a legacy encoding can be expanded to 3 bytes in UTF-8.
  // A 'two-byte character' in a legacy encoding can be expanded to 4 bytes
  // in UTF-8. Therefore, the expansion ratio is 3 at most.
  int length = static_cast<int>(decoded.length());
  char* buf = WriteInto(output, length * 3);
  length = ucnv_toAlgorithmic(UCNV_UTF8, converter, buf, length * 3,
      decoded.data(), length, &err);
  ucnv_close(converter);
  if (U_FAILURE(err)) {
    return false;
  }
  output->resize(length);
  return true;
}

bool DecodeWord(const std::string& encoded_word,
                bool *is_rfc2047,
                std::string* output) {
  // TODO(jungshik) : Revisit this later. Do we want to pass through non-ASCII
  // strings which can be mozibake?  WinHTTP converts a raw 8bit string
  // UTF-16 assuming it's in the OS default encoding.
  if (!IsStringASCII(encoded_word)) {
    // Try falling back to the NativeMB encoding if the raw input is not UTF-8.
    if (IsStringUTF8(encoded_word.c_str())) {
      *output = encoded_word;
    } else {
      *output = WideToUTF8(base::SysNativeMBToWide(encoded_word));
    }
    *is_rfc2047 = false;
    return true;
  }

  // RFC 2047 : one of encoding methods supported by Firefox and relatively
  // widely used by web servers.
  // =?charset?<E>?<encoded string>?= where '<E>' is either 'B' or 'Q'.
  // We don't care about the length restriction (72 bytes) because
  // many web servers generate encoded words longer than the limit.
  std::string tmp;
  *is_rfc2047 = true;
  int part_index = 0;
  std::string charset;
  StringTokenizer t(encoded_word, "?");
  RFC2047EncodingType enc_type = Q_ENCODING;
  while (*is_rfc2047 && t.GetNext()) {
    std::string part = t.token();
    switch (part_index) {
      case 0:
        if (part != "=") {
          *is_rfc2047 = false;
          break;
        }
        ++part_index;
        break;
      case 1:
        // Do we need charset validity check here?
        charset = part;
        ++part_index;
        break;
      case 2:
        if (part.size() > 1 ||
            part.find_first_of("bBqQ") == std::string::npos) {
          *is_rfc2047 = false;
          break;
        }
        if (part[0] == 'b' || part[0] == 'B') {
          enc_type = B_ENCODING;
        }
        ++part_index;
        break;
      case 3:
        *is_rfc2047 = DecodeBQEncoding(part, enc_type, charset, &tmp);
        if (!*is_rfc2047) {
          // Last minute failure. Invalid B/Q encoding. Rather than
          // passing it through, return now.
          return false;
        }
        ++part_index;
        break;
      case 4:
        if (part != "=") {
          // Another last minute failure !
          // Likely to be a case of two encoded-words in a row or
          // an encoded word followed by a non-encoded word. We can be
          // generous, but it does not help much in terms of compatibility,
          // I believe. Return immediately.
          *is_rfc2047 = false;
          return false;
        }
        ++part_index;
        break;
      default:
        *is_rfc2047 = false;
        return false;
    }
  }

  if (*is_rfc2047) {
    if (*(encoded_word.end() - 1) == '=') {
      output->swap(tmp);
      return true;
    }
    // encoded_word ending prematurelly with '?' or extra '?'
    *is_rfc2047 = false;
    return false;
  }

  // We're not handling 'especial' characters quoted with '\', but
  // it should be Ok because we're not an email client but a
  // web browser.

  // What IE6/7 does: %-escaped UTF-8. We could extend this to
  // support a rudimentary form of RFC 2231 with charset label, but
  // it'd gain us little in terms of compatibility.
  tmp = UnescapeURLComponent(encoded_word, UnescapeRule::SPACES);
  if (IsStringUTF8(tmp.c_str())) {
    output->swap(tmp);
    return true;
    // We can try either the OS default charset or 'origin charset' here,
    // As far as I can tell, IE does not support it. However, I've seen
    // web servers emit %-escaped string in a legacy encoding (usually
    // origin charset).
    // TODO(jungshik) : Test IE further and consider adding a fallback here.
  }
  return false;
}

bool DecodeParamValue(const std::string& input, std::string* output) {
  std::string tmp;
  // Tokenize with whitespace characters.
  StringTokenizer t(input, " \t\n\r");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  bool is_previous_token_rfc2047 = true;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      // If the previous non-delimeter token is not RFC2047-encoded,
      // put in a space in its place. Otheriwse, skip over it.
      if (!is_previous_token_rfc2047) {
        tmp.push_back(' ');
      }
      continue;
    }
    // We don't support a single multibyte character split into
    // adjacent encoded words. Some broken mail clients emit headers
    // with that problem, but most web servers usually encode a filename
    // in a single encoded-word. Firefox/Thunderbird do not support
    // it, either.
    std::string decoded;
    if (!DecodeWord(t.token(), &is_previous_token_rfc2047, &decoded))
      return false;
    tmp.append(decoded);
  }
  output->swap(tmp);
  return true;
}

// TODO(mpcomplete): This is a quick and dirty implementation for now.  I'm
// sure this doesn't properly handle all (most?) cases.
template<typename STR>
STR GetHeaderParamValueT(const STR& header, const STR& param_name) {
  // This assumes args are formatted exactly like "bla; arg1=value; arg2=value".
  typename STR::const_iterator param_begin =
      search(header.begin(), header.end(), param_name.begin(), param_name.end(),
             CaseInsensitiveCompareASCII<typename STR::value_type>());

  if (param_begin == header.end())
    return STR();
  param_begin += param_name.length();

  STR whitespace;
  whitespace.push_back(' ');
  whitespace.push_back('\t');
  const typename STR::size_type equals_offset =
      header.find_first_not_of(whitespace, param_begin - header.begin());
  if (equals_offset == STR::npos || header.at(equals_offset) != '=')
    return STR();

  param_begin = header.begin() + equals_offset + 1;
  if (param_begin == header.end())
    return STR();

  typename STR::const_iterator param_end;
  if (*param_begin == '"') {
    param_end = find(param_begin+1, header.end(), '"');
    if (param_end == header.end())
      return STR();  // poorly formatted param?

    ++param_begin;  // skip past the quote.
  } else {
    param_end = find(param_begin+1, header.end(), ';');
  }

  return STR(param_begin, param_end);
}

// Does some simple normalization of scripts so we can allow certain scripts
// to exist together.
// TODO(brettw) bug 880223: we should allow some other languages to be
// oombined such as Chinese and Latin. We will probably need a more
// complicated system of language pairs to have more fine-grained control.
UScriptCode NormalizeScript(UScriptCode code) {
  switch (code) {
    case USCRIPT_KATAKANA:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
    case USCRIPT_HANGUL:  // This one is arguable.
      return USCRIPT_HAN;
    default:
      return code;
  }
}

bool IsIDNComponentInSingleScript(const char16* str, int str_len) {
  UScriptCode first_script = USCRIPT_INVALID_CODE;
  bool is_first = true;

  int i = 0;
  while (i < str_len) {
    unsigned code_point;
    U16_NEXT(str, i, str_len, code_point);

    UErrorCode err = U_ZERO_ERROR;
    UScriptCode cur_script = uscript_getScript(code_point, &err);
    if (err != U_ZERO_ERROR)
      return false;  // Report mixed on error.
    cur_script = NormalizeScript(cur_script);

    // TODO(brettw) We may have to check for USCRIPT_INHERENT as well.
    if (is_first && cur_script != USCRIPT_COMMON) {
      first_script = cur_script;
      is_first = false;
    } else {
      if (cur_script != USCRIPT_COMMON && cur_script != first_script)
        return false;
    }
  }
  return true;
}

// Check if the script of a language can be 'safely' mixed with
// Latin letters in the ASCII range.
bool IsCompatibleWithASCIILetters(const std::string& lang) {
  // For now, just list Chinese, Japanese and Korean (positive list).
  // An alternative is negative-listing (languages using Greek and
  // Cyrillic letters), but it can be more dangerous.
  return !lang.substr(0,2).compare("zh") ||
         !lang.substr(0,2).compare("ja") ||
         !lang.substr(0,2).compare("ko");
}

// Returns true if the given Unicode host component is safe to display to the
// user.
bool IsIDNComponentSafe(const char16* str,
                        int str_len,
                        const std::wstring& languages) {
  // Most common cases (non-IDN) do not reach here so that we don't
  // need a fast return path.
  // TODO(jungshik) : Check if there's any character inappropriate
  // (although allowed) for domain names.
  // See http://www.unicode.org/reports/tr39/#IDN_Security_Profiles and
  // http://www.unicode.org/reports/tr39/data/xidmodifications.txt
  // For now, we borrow the list from Mozilla and tweaked it slightly.
  // (e.g. Characters like U+00A0, U+3000, U+3002 are omitted because
  //  they're gonna be canonicalized to U+0020 and full stop before
  //  reaching here.)
  // The original list is available at
  // http://kb.mozillazine.org/Network.IDN.blacklist_chars and
  // at http://mxr.mozilla.org/seamonkey/source/modules/libpref/src/init/all.js#703

  UErrorCode status = U_ZERO_ERROR;
#ifdef U_WCHAR_IS_UTF16
  UnicodeSet dangerous_characters(UnicodeString(
      L"[[\\ \u00bc\u00bd\u01c3\u0337\u0338"
      L"\u05c3\u05f4\u06d4\u0702\u115f\u1160][\u2000-\u200b]"
      L"[\u2024\u2027\u2028\u2029\u2039\u203a\u2044\u205f]"
      L"[\u2154-\u2156][\u2159-\u215b][\u215f\u2215\u23ae"
      L"\u29f6\u29f8\u2afb\u2afd][\u2ff0-\u2ffb][\u3014"
      L"\u3015\u3033\u3164\u321d\u321e\u33ae\u33af\u33c6\u33df\ufe14"
      L"\ufe15\ufe3f\ufe5d\ufe5e\ufeff\uff0e\uff06\uff61\uffa0\ufff9]"
      L"[\ufffa-\ufffd]]"), status);
#else
  UnicodeSet dangerous_characters(UnicodeString(
      "[[\\ \\u0020\\u00bc\\u00bd\\u01c3\\u0337\\u0338"
      "\\u05c3\\u05f4\\u06d4\\u0702\\u115f\\u1160][\\u2000-\\u200b]"
      "[\\u2024\\u2027\\u2028\\u2029\\u2039\\u203a\\u2044\\u205f]"
      "[\\u2154-\\u2156][\\u2159-\\u215b][\\u215f\\u2215\\u23ae"
      "\\u29f6\\u29f8\\u2afb\\u2afd][\\u2ff0-\\u2ffb][\\u3014"
      "\\u3015\\u3033\\u3164\\u321d\\u321e\\u33ae\\u33af\\u33c6\\u33df\\ufe14"
      "\\ufe15\\ufe3f\\ufe5d\\ufe5e\\ufeff\\uff0e\\uff06\\uff61\\uffa0\\ufff9]"
      "[\\ufffa-\\ufffd]]", -1, US_INV), status);
#endif
  DCHECK(U_SUCCESS(status));
  UnicodeSet component_characters;
  component_characters.addAll(UnicodeString(str, str_len));
  if (dangerous_characters.containsSome(component_characters))
    return false;

  // If the language list is empty, the result is completely determined
  // by whether a component is a single script or not. This will block
  // even "safe" script mixing cases like <Chinese, Latin-ASCII> that are
  // allowed with |languages| (while it blocks Chinese + Latin letters with
  // an accent as should be the case), but we want to err on the safe side
  // when |languages| is empty.
  if (languages.empty())
    return IsIDNComponentInSingleScript(str, str_len);

  // |common_characters| is made up of  ASCII numbers, hyphen, plus and
  // underscore that are used across scripts and allowed in domain names.
  // (sync'd with characters allowed in url_canon_host with square
  // brackets excluded.) See kHostCharLookup[] array in url_canon_host.cc.
  UnicodeSet common_characters(UNICODE_STRING_SIMPLE("[[0-9]\\-_+\\ ]"),
                               status);
  DCHECK(U_SUCCESS(status));
  // Subtract common characters because they're always allowed so that
  // we just have to check if a language-specific set contains
  // the remainder.
  component_characters.removeAll(common_characters);

  USet *lang_set = uset_open(1, 0);  // create an empty set
  UnicodeSet ascii_letters(0x61, 0x7a);  // [a-z]
  bool safe = false;
  std::string languages_list(WideToASCII(languages));
  StringTokenizer t(languages_list, ",");
  while (t.GetNext()) {
    std::string lang = t.token();
    status = U_ZERO_ERROR;
    // TODO(jungshik) Cache exemplar sets for locales.
    ULocaleData* uld = ulocdata_open(lang.c_str(), &status);
    if (U_SUCCESS(status)) {
      // Should we use auxiliary set, instead?
      ulocdata_getExemplarSet(uld, lang_set, 0, ULOCDATA_ES_STANDARD, &status);
      ulocdata_close(uld);
      if (U_SUCCESS(status)) {
        UnicodeSet* allowed_characters =
            reinterpret_cast<UnicodeSet*>(lang_set);
        // If |lang| is compatible with ASCII Latin letters, add them.
        if (IsCompatibleWithASCIILetters(lang))
          allowed_characters->addAll(ascii_letters);
        if (allowed_characters->containsAll(component_characters)) {
          safe = true;
          break;
        }
      }
    }
  }
  uset_close(lang_set);
  return safe;
}

// Converts one component of a host (between dots) to IDN if safe. The result
// will be APPENDED to the given output string and  will be the same as the
// input if it is not IDN or the IDN is unsafe to display.
void IDNToUnicodeOneComponent(const char16* comp,
                              int comp_len,
                              const std::wstring& languages,
                              std::string16* out) {
  DCHECK(comp_len >= 0);
  if (comp_len == 0)
    return;

  // Expand the output string to make room for a possibly longer string
  // (we'll expand if it's still not big enough below).
  int extra_space = 64;
  size_t host_begin_in_output = out->size();

  // Just copy the input if it can't be an IDN component.
  if (comp_len < 4 ||
      comp[0] != 'x' || comp[1] != 'n' || comp[2] != '-' || comp[3] != '-') {
    out->resize(host_begin_in_output + comp_len);
    for (int i = 0; i < comp_len; i++)
      (*out)[host_begin_in_output + i] = comp[i];
    return;
  }

  while (true) {
    UErrorCode status = U_ZERO_ERROR;
    out->resize(out->size() + extra_space);
    int output_chars =
        uidna_IDNToUnicode(comp, comp_len, &(*out)[host_begin_in_output],
                           extra_space, UIDNA_DEFAULT, NULL, &status);
    if (status == U_ZERO_ERROR) {
      // Converted successfully.
      out->resize(host_begin_in_output + output_chars);
      if (!IsIDNComponentSafe(&out->data()[host_begin_in_output],
                              output_chars,
                              languages))
        break;  // The error handling below will undo the IDN.
      return;
    }
    if (status != U_BUFFER_OVERFLOW_ERROR)
      break;

    // Need to loop again with a bigger buffer. It looks like ICU will
    // return the required size of the buffer, but that's not documented,
    // so we'll just grow by 2x. This should be rare and is not on a
    // critical path.
    extra_space *= 2;
  }

  // We get here on error, in which case we replace anything that was added
  // with the literal input.
  out->resize(host_begin_in_output + comp_len);
  for (int i = 0; i < comp_len; i++)
    (*out)[host_begin_in_output + i] = comp[i];
}

}  // namespace

namespace net {

GURL FilePathToFileURL(const std::wstring& file_path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  std::wstring url_str(kFileURLPrefix);
  url_str.append(file_path);

  // Now do replacement of some characters. Since we assume the input is a
  // literal filename, anything the URL parser might consider special should
  // be escaped here.

  // must be the first substitution since others will introduce percents as the
  // escape character
  ReplaceSubstringsAfterOffset(&url_str, 0, L"%", L"%25");

  // semicolon is supposed to be some kind of separator according to RFC 2396
  ReplaceSubstringsAfterOffset(&url_str, 0, L";", L"%3B");

  ReplaceSubstringsAfterOffset(&url_str, 0, L"#", L"%23");

#if defined(WCHAR_T_IS_UTF32)
  return GURL(WideToUTF8(url_str));
#else
  return GURL(url_str);
#endif
}

bool FileURLToFilePath(const GURL& url, std::wstring* file_path) {
  file_path->clear();

  if (!url.is_valid())
    return false;

  std::string path;
  std::string host = url.host();
  if (host.empty()) {
    // URL contains no host, the path is the filename. In this case, the path
    // will probably be preceeded with a slash, as in "/C:/foo.txt", so we
    // trim out that here.
    path = url.path();
    size_t first_non_slash = path.find_first_not_of("/\\");
    if (first_non_slash != std::string::npos && first_non_slash > 0)
      path.erase(0, first_non_slash);
  } else {
    // URL contains a host: this means it's UNC. We keep the preceeding slash
    // on the path.
    path = "\\\\";
    path.append(host);
    path.append(url.path());
  }

  if (path.empty())
    return false;
  std::replace(path.begin(), path.end(), '/', '\\');

  // GURL stores strings as percent-encoded UTF-8, this will undo if possible.
  path = UnescapeURLComponent(path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  if (!IsStringUTF8(path.c_str())) {
    // Not UTF-8, assume encoding is native codepage and we're done. We know we
    // are giving the conversion function a nonempty string, and it may fail if
    // the given string is not in the current encoding and give us an empty
    // string back. We detect this and report failure.
    *file_path = base::SysNativeMBToWide(path);
    return !file_path->empty();
  }
  file_path->assign(UTF8ToWide(path));

  // Now we have an unescaped filename, but are still not sure about its
  // encoding. For example, each character could be part of a UTF-8 string.
  if (file_path->empty() || !IsString8Bit(*file_path)) {
    // assume our 16-bit encoding is correct if it won't fit into an 8-bit
    // string
    return true;
  }

  // Convert our narrow string into the native wide path.
  std::string narrow;
  if (!WideToLatin1(*file_path, &narrow)) {
    NOTREACHED() << "Should have filtered out non-8-bit strings above.";
    return false;
  }
  if (IsStringUTF8(narrow.c_str())) {
    // Our string actually looks like it could be UTF-8, convert to 8-bit
    // UTF-8 and then to the corresponding wide string.
    *file_path = UTF8ToWide(narrow);
  } else {
    // Our wide string contains only 8-bit characters and it's not UTF-8, so
    // we assume it's in the native codepage.
    *file_path = base::SysNativeMBToWide(narrow);
  }

  // Fail if 8-bit -> wide conversion failed and gave us an empty string back
  // (we already filtered out empty strings above).
  return !file_path->empty();
}

std::wstring GetSpecificHeader(const std::wstring& headers,
                               const std::wstring& name) {
  return GetSpecificHeaderT(headers, name);
}

std::string GetSpecificHeader(const std::string& headers,
                               const std::string& name) {
  return GetSpecificHeaderT(headers, name);
}

std::wstring GetFileNameFromCD(const std::string& header) {
  std::string param_value = GetHeaderParamValue(header, "filename");
  if (param_value.empty()) {
    // Some servers use 'name' parameter.
    param_value = GetHeaderParamValue(header, "name");
  }
  if (param_value.empty())
    return std::wstring();
  std::string decoded;
  if (DecodeParamValue(param_value, &decoded))
    return UTF8ToWide(decoded);
  return std::wstring();
}

std::wstring GetHeaderParamValue(const std::wstring& field,
                                 const std::wstring& param_name) {
  return GetHeaderParamValueT(field, param_name);
}

std::string GetHeaderParamValue(const std::string& field,
                                const std::string& param_name) {
  return GetHeaderParamValueT(field, param_name);
}

// TODO(brettw) bug 734373: check the scripts for each host component and
// don't un-IDN-ize if there is more than one. Alternatively, only IDN for
// scripts that the user has installed. For now, just put the entire
// path through IDN. Maybe this feature can be implemented in ICU itself?
//
// We may want to skip this step in the case of file URLs to allow unicode
// UNC hostnames regardless of encodings.
void IDNToUnicode(const char* host,
                  int host_len,
                  const std::wstring& languages,
                  std::wstring* out) {
  // Convert the ASCII input to a wide string for ICU.
  std::string16 input16;
  input16.reserve(host_len);
  for (int i = 0; i < host_len; i++)
    input16.push_back(host[i]);

  std::string16 out16;

  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  size_t cur_begin = 0;  // Beginning of the current component (inclusive).
  while (cur_begin < input16.size()) {
    // Find the next dot or the end of the string.
    size_t next_dot = input16.find_first_of('.', cur_begin);
    if (next_dot == std::wstring::npos)
      next_dot = input16.size();  // For getting the last component.

    if (next_dot > cur_begin) {
      // Add the substring that we just found.
      IDNToUnicodeOneComponent(&input16[cur_begin],
                               static_cast<int>(next_dot - cur_begin),
                               languages,
                               &out16);
    }

    // Need to add the dot we just found (if we found one). This needs to be
    // done before we break out below in case the URL ends in a dot.
    if (next_dot < input16.size())
      out16.push_back('.');
    else
      break;  // No more components left.

    cur_begin = next_dot + 1;
  }

#if defined(WCHAR_T_IS_UTF32)
  UTF16ToWide(out16.data(), out16.length(), out);
#elif defined(WCHAR_T_IS_UTF16)
  out->swap(out16);
#endif
}

std::string CanonicalizeHost(const std::string& host, bool* is_ip_address) {
  // Try to canonicalize the host.
  const url_parse::Component raw_host_component(0,
      static_cast<int>(host.length()));
  std::string canon_host;
  url_canon::StdStringCanonOutput canon_host_output(&canon_host);
  url_parse::Component canon_host_component;
  if (!url_canon::CanonicalizeHost(host.c_str(), raw_host_component,
                                   &canon_host_output, &canon_host_component)) {
    if (is_ip_address)
      *is_ip_address = false;
    return std::string();
  }
  canon_host_output.Complete();

  if (is_ip_address) {
    // See if the host is an IP address.
    url_canon::RawCanonOutputT<char, 128> ignored_output;
    url_parse::Component ignored_component;
    *is_ip_address = url_canon::CanonicalizeIPAddress(canon_host.c_str(),
                                                      canon_host_component,
                                                      &ignored_output,
                                                      &ignored_component);
  }

  // Return the host as a string, stripping any unnecessary bits off the ends.
  if ((canon_host_component.begin == 0) &&
      (static_cast<size_t>(canon_host_component.len) == canon_host.length()))
    return canon_host;
  return canon_host.substr(canon_host_component.begin,
                           canon_host_component.len);
}

std::string CanonicalizeHost(const std::wstring& host, bool* is_ip_address) {
  std::string converted_host;
  WideToUTF8(host.c_str(), host.length(), &converted_host);
  return CanonicalizeHost(converted_host, is_ip_address);
}
  
std::string GetDirectoryListingHeader(const std::string& title) {
  std::string result = NetModule::GetResource(IDR_DIR_HEADER_HTML);
  if (result.empty()) {
    NOTREACHED() << "expected resource not found";
  }

  result.append("<script>start(");
  string_escape::JavascriptDoubleQuote(title, true, &result);
  result.append(");</script>\n");

  return result;
}

#ifdef OS_WIN
std::string GetDirectoryListingEntry(const std::string& name,
                                     DWORD attrib,
                                     int64 size,
                                     const FILETIME* modified) {
  std::string result;
  result.append("<script>addRow(");
  string_escape::JavascriptDoubleQuote(name, true, &result);
  result.append(",");
  string_escape::JavascriptDoubleQuote(
      EscapePath(name), true, &result);
  if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  string_escape::JavascriptDoubleQuote(
      FormatBytes(size, GetByteDisplayUnits(size), true), true, &result);

  result.append(",");

  Time time(Time::FromFileTime(*modified));
  string_escape::JavascriptDoubleQuote(base::TimeFormatShortDateAndTime(time),
      true, &result);

  result.append(");</script>\n");

  return result;
}
#endif

std::wstring StripWWW(const std::wstring& text) {
  const std::wstring www(L"www.");
  return (text.compare(0, www.length(), www) == 0) ?
      text.substr(www.length()) : text;
}

std::wstring GetSuggestedFilename(const GURL& url,
                                  const std::string& content_disposition,
                                  const std::wstring& default_name) {
  std::wstring filename = GetFileNameFromCD(content_disposition);
  if (!filename.empty()) {
    // Remove any path information the server may have sent, take the name
    // only.
    filename = file_util::GetFilenameFromPath(filename);
    // Next, remove "." from the beginning and end of the file name to avoid
    // tricks with hidden files, "..", and "."
    TrimString(filename, L".", &filename);
  }
  if (filename.empty()) {
    if (url.is_valid()) {
      filename = UnescapeAndDecodeUTF8URLComponent(
          url.ExtractFileName(),
          UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
    }
  }

  // Trim '.' once more.
  TrimString(filename, L".", &filename);
  // If there's no filename or it gets trimed to be empty, use
  // the URL hostname or default_name
  if (filename.empty()) {
    if (!default_name.empty())
      filename = default_name;
    else if (url.is_valid()) {
      // Some schemes (e.g. file) do not have a hostname. Even though it's
      // not likely to reach here, let's hardcode the last fallback name.
      // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
      filename = url.host().empty() ? L"download" : UTF8ToWide(url.host());
    } else
      NOTREACHED();
  }

  file_util::ReplaceIllegalCharacters(&filename, '-');
  return filename;
}

std::wstring GetSuggestedFilename(const GURL& url,
                                  const std::wstring& content_disposition,
                                  const std::wstring& default_name) {
  return GetSuggestedFilename(
      url, WideToUTF8(content_disposition), default_name);
}

bool IsPortAllowedByDefault(int port) {
  int array_size = arraysize(kRestrictedPorts);
  for (int i = 0; i < array_size; i++) {
    if (kRestrictedPorts[i] == port) {
      return false;
    }
  }
  return true;
}

bool IsPortAllowedByFtp(int port) {
  int array_size = arraysize(kAllowedFtpPorts);
  for (int i = 0; i < array_size; i++) {
    if (kAllowedFtpPorts[i] == port) {
        return true;
    }
  }
  // Port not explicitly allowed by FTP, so return the default restrictions.
  return IsPortAllowedByDefault(port);
}

}  // namespace net
