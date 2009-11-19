// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome_frame/html_utils.h"

#include <atlbase.h>
#include <urlmon.h>

#include "base/string_util.h"
#include "base/string_tokenizer.h"
#include "chrome_frame/utils.h"

const wchar_t kQuotes[] = L"\"'";
const char kXFrameOptionsHeader[] = "X-Frame-Options";
const char kXFrameOptionsValueAllowAll[] = "allowall";

HTMLScanner::StringRange::StringRange() {
}

HTMLScanner::StringRange::StringRange(StrPos start, StrPos end)
    : start_(start), end_(end) {
}

bool HTMLScanner::StringRange::LowerCaseEqualsASCII(const char* other) const {
  return ::LowerCaseEqualsASCII(start_, end_, other);
}

bool HTMLScanner::StringRange::Equals(const wchar_t* other) const {
  int ret = wcsncmp(&start_[0], other, end_ - start_);
  if (ret == 0)
    ret = (other[end_ - start_] == L'\0') ? 0 : -1;
  return ret == 0;
}

std::wstring HTMLScanner::StringRange::Copy() const {
  return std::wstring(start_, end_);
}

bool HTMLScanner::StringRange::GetTagName(std::wstring* tag_name) const {
  if (*start_ != L'<') {
    LOG(ERROR) << "Badly formatted tag found";
    return false;
  }

  StrPos name_start = start_;
  name_start++;
  while (name_start < end_ && IsWhitespace(*name_start))
    name_start++;

  if (name_start >= end_) {
    // We seem to have a degenerate tag (i.e. <   >). Return false here.
    return false;
  }

  StrPos name_end = name_start + 1;
  while (name_end < end_ && !IsWhitespace(*name_end))
    name_end++;

  if (name_end > end_) {
    // This looks like an improperly formatted tab ('<foo'). Return false here.
    return false;
  }

  tag_name->assign(name_start, name_end);
  return true;
}


bool HTMLScanner::StringRange::GetTagAttribute(const wchar_t* attribute_name,
    StringRange* attribute_value) const {
  if (NULL == attribute_name || NULL == attribute_value) {
    NOTREACHED();
    return false;
  }

  // Use this so we can use the convenience method LowerCaseEqualsASCII()
  // from string_util.h.
  std::string search_name_ascii(WideToASCII(attribute_name));

  WStringTokenizer tokenizer(start_, end_, L" =/");
  tokenizer.set_options(WStringTokenizer::RETURN_DELIMS);

  // Set up the quote chars so that we get quoted attribute values as single
  // tokens.
  tokenizer.set_quote_chars(L"\"'");

  const bool PARSE_STATE_NAME = true;
  const bool PARSE_STATE_VALUE = false;
  bool parse_state = PARSE_STATE_NAME;

  // Used to skip the first token, which is the tag name.
  bool first_token_skipped = false;

  // This is set during a loop iteration in which an '=' sign was spotted.
  // It is used to filter out degenerate tags such as:
  // <meta foo==bar>
  bool last_token_was_delim = false;

  // Set this if the attribute name has been found that we might then
  // pick up the value in the next loop iteration.
  bool attribute_name_found = false;

  while (tokenizer.GetNext()) {
    // If we have a whitespace delimiter, just keep going. Cases of this should
    // be reduced by the CollapseWhitespace call. If we have an '=' character,
    // we update our state and reiterate.
    if (tokenizer.token_is_delim()) {
      if (*tokenizer.token_begin() == L'=') {
        if (last_token_was_delim) {
          // Looks like we have a badly formed tag, just stop parsing now.
          return false;
        }
        parse_state = !parse_state;
        last_token_was_delim = true;
      }
      continue;
    }

    last_token_was_delim = false;

    // The first non-delimiter token is the tag name, which we don't want.
    if (!first_token_skipped) {
      first_token_skipped = true;
      continue;
    }

    if (PARSE_STATE_NAME == parse_state) {
      // We have a tag name, check to see if it matches our target name:
      if (::LowerCaseEqualsASCII(tokenizer.token_begin(), tokenizer.token_end(),
                                 search_name_ascii.c_str())) {
        attribute_name_found = true;
        continue;
      }
    } else if (PARSE_STATE_VALUE == parse_state && attribute_name_found) {
      attribute_value->start_ = tokenizer.token_begin();
      attribute_value->end_ = tokenizer.token_end();

      // Unquote the attribute value if need be.
      attribute_value->UnQuote();

      return true;
    } else if (PARSE_STATE_VALUE == parse_state) {
      // If we haven't found the attribute name we want yet, ignore this token
      // and go back to looking for our name.
      parse_state = PARSE_STATE_NAME;
    }
  }

  return false;
}

bool HTMLScanner::StringRange::UnQuote() {
  if (start_ + 2 > end_) {
    // String's too short to be quoted, bail.
    return false;
  }

  if ((*start_ == L'\'' && *(end_ - 1) == L'\'') ||
      (*start_ == L'"' && *(end_ - 1) == L'"')) {
    start_ = start_ + 1;
    end_ = end_ - 1;
    return true;
  }

  return false;
}

HTMLScanner::HTMLScanner(const wchar_t* html_string)
    : html_string_(CollapseWhitespace(html_string, true)),
      quotes_(kQuotes) {
}

void HTMLScanner::GetTagsByName(const wchar_t* name, StringRangeList* tag_list,
                                const wchar_t* stop_tag) {
  DCHECK(NULL != name);
  DCHECK(NULL != tag_list);
  DCHECK(NULL != stop_tag);

  StringRange remaining_html(html_string_.begin(), html_string_.end());

  std::wstring search_name(name);
  TrimWhitespace(search_name, TRIM_ALL, &search_name);

  // Use this so we can use the convenience method LowerCaseEqualsASCII()
  // from string_util.h.
  std::string search_name_ascii(WideToASCII(search_name));
  std::string stop_tag_ascii(WideToASCII(stop_tag));

  StringRange current_tag;
  std::wstring current_name;
  while (NextTag(&remaining_html, &current_tag)) {
    if (current_tag.GetTagName(&current_name)) {
      if (LowerCaseEqualsASCII(current_name, search_name_ascii.c_str())) {
        tag_list->push_back(current_tag);
      } else if (LowerCaseEqualsASCII(current_name, stop_tag_ascii.c_str())) {
        // We hit the stop tag so it's time to go home.
        break;
      }
    }
  }
}

struct ScanState {
  bool in_quote;
  bool in_escape;
  wchar_t quote_char;
  ScanState() : in_quote(false), in_escape(false) {}
};

bool HTMLScanner::IsQuote(wchar_t c) {
  return quotes_.find(c) != std::wstring::npos;
}

bool HTMLScanner::IsHTMLCommentClose(StringRange* html_string, StrPos pos) {
  if (pos < html_string->end_ && pos > html_string->start_ + 2 &&
      *pos == L'>') {
    return *(pos-1) == L'-' && *(pos-2) == L'-';
  }
  return false;
}

bool HTMLScanner::NextTag(StringRange* html_string, StringRange* tag) {
  DCHECK(NULL != html_string);
  DCHECK(NULL != tag);

  tag->start_ = html_string->start_;
  while (tag->start_ < html_string->end_ && *tag->start_ != L'<') {
    tag->start_++;
  }

  // we went past the end of the string.
  if (tag->start_ >= html_string->end_) {
    return false;
  }

  tag->end_ = tag->start_ + 1;

  // Get the tag name to see if we are in an HTML comment. If we are, then
  // don't consider quotes. This should work for example:
  // <!-- foo ' --> <meta foo='bar'>
  std::wstring tag_name;
  StringRange start_range(tag->start_, html_string->end_);
  start_range.GetTagName(&tag_name);
  if (StartsWith(tag_name, L"!--", true)) {
    // We're inside a comment tag, keep going until we get out of it.
    while (tag->end_ < html_string->end_ &&
           !IsHTMLCommentClose(html_string, tag->end_)) {
      tag->end_++;
    }
  } else {
    // Properly handle quoted strings within non-comment tags by maintaining
    // some state while scanning. Specifically, we have to maintain state on
    // whether we are inside a string, what the string terminating character
    // will be and whether we are inside an escape sequence.
    ScanState state;
    while (tag->end_ < html_string->end_) {
      if (state.in_quote) {
        if (state.in_escape) {
          state.in_escape = false;
        } else if (*tag->end_ == '\\') {
          state.in_escape = true;
        } else if (*tag->end_ == state.quote_char) {
          state.in_quote = false;
        }
      } else {
        state.in_quote = IsQuote(state.quote_char = *tag->end_);
      }

      if (!state.in_quote && *tag->end_ == L'>') {
        break;
      }
      tag->end_++;
    }
  }

  // We hit the end_ but found no matching tag closure. Consider this an
  // incomplete tag and do not report it.
  if (tag->end_ >= html_string->end_)
    return false;

  // Modify html_string to point to just beyond the end_ of the current tag.
  html_string->start_ = tag->end_ + 1;

  return true;
}

namespace http_utils {

const char kChromeFrameUserAgent[] = "chromeframe";

const char* GetChromeFrameUserAgent() {
  static char cf_user_agent[100] = {0};
  if (!cf_user_agent[0]) {
    _pAtlModule->m_csStaticDataInitAndTypeInfo.Lock();
    if (!cf_user_agent[0]) {
      uint32 version = 0;
      GetModuleVersion(reinterpret_cast<HMODULE>(&__ImageBase), &version, NULL);
      wsprintfA(cf_user_agent, "%s/%i.%i", kChromeFrameUserAgent,
                HIWORD(version), LOWORD(version));
    }
    _pAtlModule->m_csStaticDataInitAndTypeInfo.Unlock();
  }
  return cf_user_agent;
}

std::string AddChromeFrameToUserAgentValue(const std::string& value) {
  if (value.empty()) {
    DLOG(WARNING) << "empty user agent value";
    return "";
  }

  DCHECK_EQ(false, StartsWithASCII(value, "User-Agent:", true));

  if (value.find(kChromeFrameUserAgent) != std::string::npos) {
    // Our user agent has already been added.
    return value;
  }

  std::string ret(value);
  ret += " ";
  ret += GetChromeFrameUserAgent();

  return ret;
}

std::string GetDefaultUserAgentHeaderWithCFTag() {
  std::string ua(GetDefaultUserAgent());
  return "User-Agent: " + AddChromeFrameToUserAgentValue(ua);
}

std::string GetDefaultUserAgent() {
  std::string ret;
  DWORD size = MAX_PATH;  // NOLINT
  HRESULT hr = E_OUTOFMEMORY;
  for (int retries = 1; hr == E_OUTOFMEMORY && retries <= 10; ++retries) {
    hr = ::ObtainUserAgentString(0, WriteInto(&ret, size + 1), &size);
    if (hr == E_OUTOFMEMORY) {
      size = MAX_PATH * retries;
    } else if (SUCCEEDED(hr)) {
      // Truncate the extra allocation.
      DCHECK(size > 0);  // NOLINT
      ret.resize(size - sizeof(char));  // NOLINT
    }
  }

  if (FAILED(hr)) {
    NOTREACHED() << StringPrintf("ObtainUserAgentString==0x%08X", hr);
    return "";
  } else {
    DCHECK(ret.length() == lstrlenA(ret.c_str()));
  }

  return ret;
}

bool HasFrameBustingHeader(const std::string& http_headers) {
  net::HttpUtil::HeadersIterator it(
      http_headers.begin(), http_headers.end(), "\r\n");
  while (it.GetNext()) {
    if (it.name() == kXFrameOptionsHeader) {
      std::string allow_all(kXFrameOptionsValueAllowAll);
      if (it.values_end() - it.values_begin() != allow_all.length() ||
          !std::equal(it.values_begin(), it.values_end(),
              allow_all.begin(),
              CaseInsensitiveCompareASCII<const char>())) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace http_utils
