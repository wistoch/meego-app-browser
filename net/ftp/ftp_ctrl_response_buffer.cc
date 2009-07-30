// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_ctrl_response_buffer.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"

namespace {

// TODO(phajdan.jr): Remove when http://crbug.com/18036 is diagnosed.
void LogResponse(const net::FtpCtrlResponse& response) {
  DLOG(INFO) << "received response with code " << response.status_code;
  for (std::vector<std::string>::const_iterator i = response.lines.begin();
       i != response.lines.end(); ++i) {
    DLOG(INFO) << "line [" << *i << "]";
  }
}

}  // namespace

namespace net {

// static
const int FtpCtrlResponse::kInvalidStatusCode = -1;

int FtpCtrlResponseBuffer::ConsumeData(const char* data, int data_length) {
  buffer_.append(std::string(data, data_length));
  ExtractFullLinesFromBuffer();

  while (!lines_.empty()) {
    ParsedLine line = lines_.front();
    lines_.pop();

    if (line_buf_.empty()) {
      if (!line.is_complete)
        return ERR_INVALID_RESPONSE;

      if (line.is_multiline) {
        line_buf_ = line.status_text;
        response_buf_.status_code = line.status_code;
      } else {
        response_buf_.status_code = line.status_code;
        response_buf_.lines.push_back(line.status_text);
        LogResponse(response_buf_);
        responses_.push(response_buf_);

        // Prepare to handle following lines.
        response_buf_ = FtpCtrlResponse();
        line_buf_.clear();
      }
    } else {
      if (!line.is_complete || line.status_code != response_buf_.status_code) {
        line_buf_.append(line.raw_text);
        continue;
      }

      response_buf_.lines.push_back(line_buf_);

      line_buf_ = line.status_text;
      DCHECK_EQ(line.status_code, response_buf_.status_code);

      if (!line.is_multiline) {
        response_buf_.lines.push_back(line_buf_);
        LogResponse(response_buf_);
        responses_.push(response_buf_);

        // Prepare to handle following lines.
        response_buf_ = FtpCtrlResponse();
        line_buf_.clear();
      }
    }
  }

  return OK;
}

// static
FtpCtrlResponseBuffer::ParsedLine FtpCtrlResponseBuffer::ParseLine(
    const std::string& line) {
  ParsedLine result;

  if (line.length() >= 3) {
    if (StringToInt(line.substr(0, 3), &result.status_code))
      result.has_status_code = (100 <= result.status_code &&
                                result.status_code <= 599);
    if (result.has_status_code && line.length() >= 4 && line[3] == ' ') {
      result.is_complete = true;
    } else if (result.has_status_code && line.length() >= 4 && line[3] == '-') {
      result.is_complete = true;
      result.is_multiline = true;
    }
  }

  if (result.is_complete) {
    result.status_text = line.substr(4);
  } else {
    result.status_text = line;
  }

  result.raw_text = line;

  return result;
}

void FtpCtrlResponseBuffer::ExtractFullLinesFromBuffer() {
  std::string line_buf;
  int cut_pos = 0;
  for (size_t i = 0; i < buffer_.length(); i++) {
    line_buf.push_back(buffer_[i]);
    if (i >= 1 && buffer_[i - 1] == '\r' && buffer_[i] == '\n') {
      lines_.push(ParseLine(line_buf.substr(0, line_buf.length() - 2)));
      cut_pos = i + 1;
      line_buf.clear();
    }
  }
  buffer_.erase(0, cut_pos);
}

}  // namespace net
