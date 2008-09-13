// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"

using std::string;

namespace net {

//-----------------------------------------------------------------------------

// Return the index of the closing quote of the string, if any.
static size_t FindStringEnd(const string& line, size_t start, char delim) {
  DCHECK(start < line.length() && line[start] == delim &&
         (delim == '"' || delim == '\''));

  const char set[] = { delim, '\\', '\0' };
  for (;;) {
    // start points to either the start quote or the last
    // escaped char (the char following a '\\')

    size_t end = line.find_first_of(set, start + 1);
    if (end == string::npos)
      return line.length();

    if (line[end] == '\\') {
      // Hit a backslash-escaped char.  Need to skip over it.
      start = end + 1;
      if (start == line.length())
        return start;

      // Go back to looking for the next escape or the string end
      continue;
    }

    return end;
  }

  NOTREACHED();
  return line.length();
}

//-----------------------------------------------------------------------------

// static
size_t HttpUtil::FindDelimiter(const string& line, size_t search_start,
                               char delimiter) {
  do {
    // search_start points to the spot from which we should start looking
    // for the delimiter.
    const char delim_str[] = { delimiter, '"', '\'', '\0' };
    size_t cur_delim_pos = line.find_first_of(delim_str, search_start);
    if (cur_delim_pos == string::npos)
      return line.length();

    char ch = line[cur_delim_pos];
    if (ch == delimiter) {
      // Found delimiter
      return cur_delim_pos;
    }

    // We hit the start of a quoted string.  Look for its end.
    search_start = FindStringEnd(line, cur_delim_pos, ch);
    if (search_start == line.length())
      return search_start;

    ++search_start;

    // search_start now points to the first char after the end of the
    // string, so just go back to the top of the loop and look for
    // |delimiter| again.
  } while (true);

  NOTREACHED();
  return line.length();
}

// static
void HttpUtil::ParseContentType(const string& content_type_str,
                                string* mime_type, string* charset,
                                bool *had_charset) {
  // Trim leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(HTTP_LWS);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end = content_type_str.find_first_of(HTTP_LWS ";(", type_val);
  if (string::npos == type_end)
    type_end = content_type_str.length();

  size_t charset_val = 0;
  size_t charset_end = 0;

  // Iterate over parameters
  bool type_has_charset = false;
  size_t param_start = content_type_str.find_first_of(';', type_end);
  if (param_start != string::npos) {
    // We have parameters.  Iterate over them.
    size_t cur_param_start = param_start + 1;
    do {
      size_t cur_param_end =
          FindDelimiter(content_type_str, cur_param_start, ';');

      size_t param_name_start = content_type_str.find_first_not_of(HTTP_LWS,
          cur_param_start);
      param_name_start = std::min(param_name_start, cur_param_end);

      static const char charset_str[] = "charset=";
      size_t charset_end_offset = std::min(param_name_start +
          sizeof(charset_str) - 1, cur_param_end);
      if (LowerCaseEqualsASCII(content_type_str.begin() + param_name_start,
          content_type_str.begin() + charset_end_offset, charset_str)) {
        charset_val = param_name_start + sizeof(charset_str) - 1;
        charset_end = cur_param_end;
        type_has_charset = true;
      }

      cur_param_start = cur_param_end + 1;
    } while (cur_param_start < content_type_str.length());
  }

  if (type_has_charset) {
    // Trim leading and trailing whitespace from charset_val.  We include
    // '(' in the trailing trim set to catch media-type comments, which are
    // not at all standard, but may occur in rare cases.
    charset_val = content_type_str.find_first_not_of(HTTP_LWS, charset_val);
    charset_val = std::min(charset_val, charset_end);
    char first_char = content_type_str[charset_val];
    if (first_char == '"' || first_char == '\'') {
      charset_end = FindStringEnd(content_type_str, charset_val, first_char);
      ++charset_val;
      DCHECK(charset_end >= charset_val);
    } else {
      charset_end = std::min(content_type_str.find_first_of(HTTP_LWS ";(",
                                                            charset_val),
                             charset_end);
    }
  }

  // if the server sent "*/*", it is meaningless, so do not store it.
  // also, if type_val is the same as mime_type, then just update the
  // charset.  however, if charset is empty and mime_type hasn't
  // changed, then don't wipe-out an existing charset.  We
  // also want to reject a mime-type if it does not include a slash.
  // some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() != 0 &&
      content_type_str != "*/*" &&
      content_type_str.find_first_of('/') != string::npos) {
    // Common case here is that mime_type is empty
    bool eq = !mime_type->empty() &&
        LowerCaseEqualsASCII(content_type_str.begin() + type_val,
                             content_type_str.begin() + type_end,
                             mime_type->data());
    if (!eq) {
      mime_type->assign(content_type_str.begin() + type_val,
                        content_type_str.begin() + type_end);
      StringToLowerASCII(mime_type);
    }
    if ((!eq && *had_charset) || type_has_charset) {
      *had_charset = true;
      charset->assign(content_type_str.begin() + charset_val,
                      content_type_str.begin() + charset_end);
      StringToLowerASCII(charset);
    }
  }
}

// static
bool HttpUtil::HasHeader(const std::string& headers, const char* name) {
  size_t name_len = strlen(name);
  string::const_iterator it =
      std::search(headers.begin(),
                  headers.end(),
                  name,
                  name + name_len,
                  CaseInsensitiveCompareASCII<char>());
  if (it == headers.end())
    return false;

  // ensure match is prefixed by newline
  if (it != headers.begin() && it[-1] != '\n')
    return false;

  // ensure match is suffixed by colon
  if (it + name_len >= headers.end() || it[name_len] != ':')
    return false;

  return true;
}

// static
bool HttpUtil::IsNonCoalescingHeader(string::const_iterator name_begin,
                                     string::const_iterator name_end) {
  // NOTE: "set-cookie2" headers do not support expires attributes, so we don't
  // have to list them here.
  const char* kNonCoalescingHeaders[] = {
    "date",
    "expires",
    "last-modified",
    "location",  // See bug 1050541 for details
    "retry-after",
    "set-cookie"
  };
  for (size_t i = 0; i < arraysize(kNonCoalescingHeaders); ++i) {
    if (LowerCaseEqualsASCII(name_begin, name_end, kNonCoalescingHeaders[i]))
      return true;
  }
  return false;
}

bool HttpUtil::IsLWS(char c) {
  return strchr(HTTP_LWS, c) != NULL;
}

void HttpUtil::TrimLWS(string::const_iterator* begin,
                       string::const_iterator* end) {
  // leading whitespace
  while (*begin < *end && IsLWS((*begin)[0]))
    ++(*begin);

  // trailing whitespace
  while (*begin < *end && IsLWS((*end)[-1]))
    --(*end);
}

// Find the "http" substring in a status line. This allows for
// some slop at the start. If the "http" string could not be found
// then returns -1.
// static
int HttpUtil::LocateStartOfStatusLine(const char* buf, int buf_len) {
  const int slop = 4;
  const int http_len = 4;

  if (buf_len >= http_len) {
    int i_max = std::min(buf_len - http_len, slop);
    for (int i = 0; i <= i_max; ++i) {
      if (LowerCaseEqualsASCII(buf + i, buf + i + http_len, "http"))
        return i;
    }
  }
  return -1; // Not found
}

int HttpUtil::LocateEndOfHeaders(const char* buf, int buf_len, int i) {
  bool was_lf = false;
  char last_c = '\0';
  for (; i < buf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (was_lf)
        return i + 1;
      was_lf = true;
    } else if (c != '\r' || last_c != '\n') {
      was_lf = false;
    }
    last_c = c;
  }
  return -1;
}

// In order for a line to be continuable, it must specify a
// non-blank header-name. Line continuations are specifically for
// header values -- do not allow headers names to span lines.
static bool IsLineSegmentContinuable(const char* begin, const char* end) {
  if (begin == end)
    return false;

  const char* colon = std::find(begin, end, ':');
  if (colon == end)
    return false;

  const char* name_begin = begin;
  const char* name_end = colon;

  // Name can't be empty.
  if (name_begin == name_end)
    return false;

  // Can't start with LWS (this would imply the segment is a continuation)
  if (HttpUtil::IsLWS(*name_begin))
    return false;

  return true;
}

// Helper used by AssembleRawHeaders, to find the end of the status line.
static const char* FindStatusLineEnd(const char* begin, const char* end) {
  size_t i = StringPiece(begin, end - begin).find_first_of("\r\n");
  if (i == StringPiece::npos)
    return end;
  return begin + i;
}

// Helper used by AssembleRawHeaders, to skip past leading LWS.
static const char* FindFirstNonLWS(const char* begin, const char* end) {
  for (const char* cur = begin; cur != end; ++cur) {
    if (!HttpUtil::IsLWS(*cur))
      return cur;
  }
  return end; // Not found.
}

std::string HttpUtil::AssembleRawHeaders(const char* input_begin,
                                         int input_len) {
  std::string raw_headers;
  raw_headers.reserve(input_len);

  const char* input_end = input_begin + input_len;

  // Skip any leading slop, since the consumers of this output
  // (HttpResponseHeaders) don't deal with it.
  int status_begin_offset = LocateStartOfStatusLine(input_begin, input_len);
  if (status_begin_offset != -1)
    input_begin += status_begin_offset;

  // Copy the status line.
  const char* status_line_end = FindStatusLineEnd(input_begin, input_end);
  raw_headers.append(input_begin, status_line_end);

  // After the status line, every subsequent line is a header line segment.
  // Should a segment start with LWS, it is a continuation of the previous
  // line's field-value.

  // TODO(ericroman): is this too permissive? (delimits on [\r\n]+)
  CStringTokenizer lines(status_line_end, input_end, "\r\n");

  // This variable is true when the previous line was continuable.
  bool prev_line_continuable = false;

  while (lines.GetNext()) {
    const char* line_begin = lines.token_begin();
    const char* line_end = lines.token_end();
     
    if (prev_line_continuable && IsLWS(*line_begin)) {
      // Join continuation; reduce the leading LWS to a single SP.
      raw_headers.push_back(' ');
      raw_headers.append(FindFirstNonLWS(line_begin, line_end), line_end);
    } else {
      // Terminate the previous line.
      raw_headers.push_back('\0');

      // Copy the raw data to output.
      raw_headers.append(line_begin, line_end);

      // Check if the current line can be continued.
      prev_line_continuable = IsLineSegmentContinuable(line_begin, line_end);
    }
  }

  raw_headers.append("\0\0", 2);
  return raw_headers;
}

// BNF from section 4.2 of RFC 2616:
//
//   message-header = field-name ":" [ field-value ]
//   field-name     = token
//   field-value    = *( field-content | LWS )
//   field-content  = <the OCTETs making up the field-value
//                     and consisting of either *TEXT or combinations
//                     of token, separators, and quoted-string>
//

HttpUtil::HeadersIterator::HeadersIterator(string::const_iterator headers_begin,
                                           string::const_iterator headers_end,
                                           const std::string& line_delimiter)
    : lines_(headers_begin, headers_end, line_delimiter) {
}

bool HttpUtil::HeadersIterator::GetNext() {
  while (lines_.GetNext()) {
    name_begin_ = lines_.token_begin();
    values_end_ = lines_.token_end();

    string::const_iterator colon = find(name_begin_, values_end_, ':');
    if (colon == values_end_)
      continue;  // skip malformed header

    name_end_ = colon;

    // If the name starts with LWS, it is an invalid line.
    // Leading LWS implies a line continuation, and these should have
    // already been joined by AssembleRawHeaders().
    if (name_begin_ == name_end_ || IsLWS(*name_begin_))
      continue;

    TrimLWS(&name_begin_, &name_end_);
    if (name_begin_ == name_end_)
      continue;  // skip malformed header

    values_begin_ = colon + 1;
    TrimLWS(&values_begin_, &values_end_);

    // if we got a header name, then we are done.
    return true;
  }
  return false;
}

HttpUtil::ValuesIterator::ValuesIterator(
    string::const_iterator values_begin,
    string::const_iterator values_end,
    char delimiter)
    : values_(values_begin, values_end, string(1, delimiter)) {
  values_.set_quote_chars("\'\"");
}

bool HttpUtil::ValuesIterator::GetNext() {
  while (values_.GetNext()) {
    value_begin_ = values_.token_begin();
    value_end_ = values_.token_end();
    TrimLWS(&value_begin_, &value_end_);

    // bypass empty values.
    if (value_begin_ != value_end_)
      return true;
  }
  return false;
}

}  // namespace net

