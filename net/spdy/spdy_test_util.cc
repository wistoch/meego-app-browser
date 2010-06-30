// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util.h"

#include "base/basictypes.h"
#include "base/string_util.h"

namespace net {

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const char* data, int length, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(true, ptr, chunk_size);
  }
  return chunks;
}

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const spdy::SpdyFrame* frame, int num_chunks) {
  return ChopFrame(frame->data(),
                   frame->length() + spdy::SpdyFrame::size(),
                   num_chunks);
}

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendHeadersToSpdyFrame(const char* const extra_headers[],
                              int extra_header_count,
                              spdy::SpdyHeaderBlock* headers) {
  std::string this_header;
  std::string this_value;

  if (!extra_header_count)
    return;

  // Sanity check: Non-NULL header list.
  DCHECK(NULL != extra_headers) << "NULL header value pair list";
  // Sanity check: Non-NULL header map.
  DCHECK(NULL != headers) << "NULL header map";
  // Copy in the headers.
  for (int i = 0; i < extra_header_count; i++) {
    // Sanity check: Non-empty header.
    DCHECK_NE('\0', *extra_headers[i * 2]) << "Empty header value pair";
    this_header = extra_headers[i * 2];
    std::string::size_type header_len = this_header.length();
    if (!header_len)
      continue;
    this_value = extra_headers[1 + (i * 2)];
    std::string new_value;
    if (headers->find(this_header) != headers->end()) {
      // More than one entry in the header.
      // Don't add the header again, just the append to the value,
      // separated by a NULL character.

      // Adjust the value.
      new_value = (*headers)[this_header];
      // Put in a NULL separator.
      new_value.append(1, '\0');
      // Append the new value.
      new_value += this_value;
    } else {
      // Not a duplicate, just write the value.
      new_value = this_value;
    }
    (*headers)[this_header] = new_value;
  }
}

// Writes |val| to a location of size |len|, in big-endian format.
// in the buffer pointed to by |buffer_handle|.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written
int AppendToBuffer(int val,
                   int len,
                   unsigned char** buffer_handle,
                   int* buffer_len_remaining) {
  if (len <= 0)
    return 0;
  DCHECK((size_t) len <= sizeof(len)) << "Data length too long for data type";
  DCHECK(NULL != buffer_handle) << "NULL buffer handle";
  DCHECK(NULL != *buffer_handle) << "NULL pointer";
  DCHECK(NULL != buffer_len_remaining)
      << "NULL buffer remainder length pointer";
  DCHECK_GE(*buffer_len_remaining, len) << "Insufficient buffer size";
  for (int i = 0; i < len; i++) {
    int shift = (8 * (len - (i + 1)));
    unsigned char val_chunk = (val >> shift) & 0x0FF;
    *(*buffer_handle)++ = val_chunk;
    *buffer_len_remaining += 1;
  }
  return len;
}

// Construct a SPDY packet.
// |head| is the start of the packet, up to but not including
// the header value pairs.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |tail| is any (relatively constant) header-value pairs to add.
// |buffer| is the buffer we're filling in.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyPacket(const SpdyHeaderInfo& header_info,
                                     const char* const extra_headers[],
                                     int extra_header_count,
                                     const char* const tail[],
                                     int tail_header_count) {
  spdy::SpdyFramer framer;
  spdy::SpdyHeaderBlock headers;
  // Copy in the extra headers to our map.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  // Copy in the tail headers to our map.
  if (tail && tail_header_count)
    AppendHeadersToSpdyFrame(tail, tail_header_count, &headers);
  spdy::SpdyFrame* frame = NULL;
  switch (header_info.kind) {
    case spdy::SYN_STREAM:
      frame = framer.CreateSynStream(header_info.id, header_info.assoc_id,
                                     header_info.priority,
                                     header_info.control_flags,
                                     header_info.compressed, &headers);
      break;
    case spdy::SYN_REPLY:
      frame = framer.CreateSynReply(header_info.id, header_info.control_flags,
                                    header_info.compressed, &headers);
      break;
    case spdy::RST_STREAM:
      frame = framer.CreateRstStream(header_info.id, header_info.status);
      break;
    default:
      frame = framer.CreateDataFrame(header_info.id, header_info.data,
                                     header_info.data_length,
                                     header_info.data_flags);
      break;
  }
  return frame;
}

// Construct an expected SPDY SETTINGS frame.
// |settings| are the settings to set.
// Returns the constructed frame.  The caller takes ownership of the frame.
spdy::SpdyFrame* ConstructSpdySettings(spdy::SpdySettings settings) {
  spdy::SpdyFramer framer;
  return framer.CreateSettings(settings);
}

// Construct a SPDY GOAWAY frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
spdy::SpdyFrame* ConstructSpdyGoAway() {
  spdy::SpdyFramer framer;
  return framer.CreateGoAway(0);
}

// Construct a single SPDY header entry, for validation.
// |extra_headers| are the extra header-value pairs.
// |buffer| is the buffer we're filling in.
// |index| is the index of the header we want.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyHeader(const char* const extra_headers[],
                        int extra_header_count,
                        char* buffer,
                        int buffer_length,
                        int index) {
  const char* this_header = NULL;
  const char* this_value = NULL;
  if (!buffer || !buffer_length)
    return 0;
  *buffer = '\0';
  // Sanity check: Non-empty header list.
  DCHECK(NULL != extra_headers) << "NULL extra headers pointer";
  // Sanity check: Index out of range.
  DCHECK((index >= 0) && (index < extra_header_count))
      << "Index " << index
      << " out of range [0, " << extra_header_count << ")";
  this_header = extra_headers[index * 2];
  // Sanity check: Non-empty header.
  if (!*this_header)
    return 0;
  std::string::size_type header_len = strlen(this_header);
  if (!header_len)
    return 0;
  this_value = extra_headers[1 + (index * 2)];
  // Sanity check: Non-empty value.
  if (!*this_value)
    this_value = "";
  int n = base::snprintf(buffer,
                         buffer_length,
                         "%s: %s\r\n",
                         this_header,
                         this_value);
  return n;
}

// Constructs a standard SPDY GET SYN packet, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count, bool compressed) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,             // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    SPDY_PRIORITY_LOWEST,         // Priority
    spdy::CONTROL_FLAG_FIN,       // Control Flags
    compressed,                   // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "method",
    "GET",
    "url",
    "http://www.google.com/",
    "version",
    "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      kSynStartHeader,
      extra_headers,
      extra_header_count,
      kStandardGetHeaders,
      arraysize(kStandardGetHeaders) / 2);
}

// Constructs a standard SPDY GET SYN packet, not compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count) {
  return ConstructSpdyGet(extra_headers, extra_header_count, false);
}

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                          int extra_header_count) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    SPDY_PRIORITY_LOWEST,         // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "hello",
    "bye",
    "status",
    "200",
    "url",
    "/index.php",
    "version",
    "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      kSynStartHeader,
      extra_headers,
      extra_header_count,
      kStandardGetHeaders,
      arraysize(kStandardGetHeaders) / 2);
}

// Constructs a standard SPDY POST SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyPost(const char* const extra_headers[],
                                   int extra_header_count) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,             // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    SPDY_PRIORITY_LOWEST,         // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "method",
    "POST",
    "url",
    "http://www.google.com/",
    "version",
    "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      kSynStartHeader,
      extra_headers,
      extra_header_count,
      kStandardGetHeaders,
      arraysize(kStandardGetHeaders) / 2);
}

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY POST.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                           int extra_header_count) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    SPDY_PRIORITY_LOWEST,         // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "hello",
    "bye",
    "status",
    "200",
    "url",
    "/index.php",
    "version",
    "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      kSynStartHeader,
      extra_headers,
      extra_header_count,
      kStandardGetHeaders,
      arraysize(kStandardGetHeaders) / 2);
}

// Constructs a single SPDY data frame with the contents "hello!"
spdy::SpdyFrame* ConstructSpdyBodyFrame() {
  spdy::SpdyFramer framer;
  return framer.CreateDataFrame(1, "hello!", 6, spdy::DATA_FLAG_FIN);
}

// Construct an expected SPDY reply string.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |buffer| is the buffer we're filling in.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyReplyString(const char* const extra_headers[],
                             int extra_header_count,
                             char* buffer,
                             int buffer_length) {
  int packet_size = 0;
  int header_count = 0;
  char* buffer_write = buffer;
  int buffer_left = buffer_length;
  spdy::SpdyHeaderBlock headers;
  if (!buffer || !buffer_length)
    return 0;
  // Copy in the extra headers.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  header_count = headers.size();
  // The iterator gets us the list of header/value pairs in sorted order.
  spdy::SpdyHeaderBlock::iterator next = headers.begin();
  spdy::SpdyHeaderBlock::iterator last = headers.end();
  for ( ; next != last; ++next) {
    // Write the header.
    int value_len, current_len, offset;
    const char* header_string = next->first.c_str();
    packet_size += AppendToBuffer(header_string,
                                  next->first.length(),
                                  &buffer_write,
                                  &buffer_left);
    packet_size += AppendToBuffer(": ",
                                  strlen(": "),
                                  &buffer_write,
                                  &buffer_left);
    // Write the value(s).
    const char* value_string = next->second.c_str();
    // Check if it's split among two or more values.
    value_len = next->second.length();
    current_len = strlen(value_string);
    offset = 0;
    // Handle the first N-1 values.
    while (current_len < value_len) {
      // Finish this line -- write the current value.
      packet_size += AppendToBuffer(value_string + offset,
                                    current_len - offset,
                                    &buffer_write,
                                    &buffer_left);
      packet_size += AppendToBuffer("\n",
                                    strlen("\n"),
                                    &buffer_write,
                                    &buffer_left);
      // Advance to next value.
      offset = current_len + 1;
      current_len += 1 + strlen(value_string + offset);
      // Start another line -- add the header again.
      packet_size += AppendToBuffer(header_string,
                                    next->first.length(),
                                    &buffer_write,
                                    &buffer_left);
      packet_size += AppendToBuffer(": ",
                                    strlen(": "),
                                    &buffer_write,
                                    &buffer_left);
    }
    EXPECT_EQ(value_len, current_len);
    // Copy the last (or only) value.
    packet_size += AppendToBuffer(value_string + offset,
                                  value_len - offset,
                                  &buffer_write,
                                  &buffer_left);
    packet_size += AppendToBuffer("\n",
                                  strlen("\n"),
                                  &buffer_write,
                                  &buffer_left);
  }
  return packet_size;
}

// Create a MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(spdy::SpdyFrame* req) {
  return MockWrite(
      true, req->data(), req->length() + spdy::SpdyFrame::size());
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(spdy::SpdyFrame* req, int seq) {
  return MockWrite(
      true, req->data(), req->length() + spdy::SpdyFrame::size(), seq);
}

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(spdy::SpdyFrame* resp) {
  return MockRead(
      true, resp->data(), resp->length() + spdy::SpdyFrame::size());
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(spdy::SpdyFrame* resp, int seq) {
  return MockRead(
      true, resp->data(), resp->length() + spdy::SpdyFrame::size(), seq);
}

}  // namespace net
