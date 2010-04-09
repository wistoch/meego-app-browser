// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_framer.h"

#include "base/scoped_ptr.h"
#include "base/stats_counters.h"

#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_bitmasks.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

namespace spdy {

// The initial size of the control frame buffer; this is used internally
// as we parse through control frames.
static const size_t kControlFrameBufferInitialSize = 32 * 1024;
// The maximum size of the control frame buffer that we support.
// TODO(mbelshe): We should make this stream-based so there are no limits.
static const size_t kControlFrameBufferMaxSize = 64 * 1024;

// By default is compression on or off.
bool SpdyFramer::compression_default_ = true;

#ifdef DEBUG_SPDY_STATE_CHANGES
#define CHANGE_STATE(newstate) \
{ \
  do { \
    LOG(INFO) << "Changing state from: " \
      << StateToString(state_) \
      << " to " << StateToString(newstate) << "\n"; \
    state_ = newstate; \
  } while (false); \
}
#else
#define CHANGE_STATE(newstate) (state_ = newstate)
#endif

SpdyFramer::SpdyFramer()
    : state_(SPDY_RESET),
      error_code_(SPDY_NO_ERROR),
      remaining_payload_(0),
      remaining_control_payload_(0),
      current_frame_buffer_(NULL),
      current_frame_len_(0),
      current_frame_capacity_(0),
      enable_compression_(compression_default_),
      visitor_(NULL) {
}

SpdyFramer::~SpdyFramer() {
  if (compressor_.get()) {
    deflateEnd(compressor_.get());
  }
  if (decompressor_.get()) {
    inflateEnd(decompressor_.get());
  }
  delete [] current_frame_buffer_;
}

void SpdyFramer::Reset() {
  state_ = SPDY_RESET;
  error_code_ = SPDY_NO_ERROR;
  remaining_payload_ = 0;
  remaining_control_payload_ = 0;
  current_frame_len_ = 0;
  if (current_frame_capacity_ != kControlFrameBufferInitialSize) {
    delete [] current_frame_buffer_;
    current_frame_buffer_ = 0;
    current_frame_capacity_ = 0;
    ExpandControlFrameBuffer(kControlFrameBufferInitialSize);
  }
}

const char* SpdyFramer::StateToString(int state) {
  switch (state) {
    case SPDY_ERROR:
      return "ERROR";
    case SPDY_DONE:
      return "DONE";
    case SPDY_AUTO_RESET:
      return "AUTO_RESET";
    case SPDY_RESET:
      return "RESET";
    case SPDY_READING_COMMON_HEADER:
      return "READING_COMMON_HEADER";
    case SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return "INTERPRET_CONTROL_FRAME_COMMON_HEADER";
    case SPDY_CONTROL_FRAME_PAYLOAD:
      return "CONTROL_FRAME_PAYLOAD";
    case SPDY_IGNORE_REMAINING_PAYLOAD:
      return "IGNORE_REMAINING_PAYLOAD";
    case SPDY_FORWARD_STREAM_FRAME:
      return "FORWARD_STREAM_FRAME";
  }
  return "UNKNOWN_STATE";
}

size_t SpdyFramer::BytesSafeToRead() const {
  switch (state_) {
    case SPDY_ERROR:
    case SPDY_DONE:
    case SPDY_AUTO_RESET:
    case SPDY_RESET:
      return 0;
    case SPDY_READING_COMMON_HEADER:
      DCHECK(current_frame_len_ < SpdyFrame::size());
      return SpdyFrame::size() - current_frame_len_;
    case SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return 0;
    case SPDY_CONTROL_FRAME_PAYLOAD:
    case SPDY_IGNORE_REMAINING_PAYLOAD:
    case SPDY_FORWARD_STREAM_FRAME:
      return remaining_payload_;
  }
  // We should never get to here.
  return 0;
}

void SpdyFramer::set_error(SpdyError error) {
  DCHECK(visitor_);
  error_code_ = error;
  CHANGE_STATE(SPDY_ERROR);
  visitor_->OnError(this);
}

const char* SpdyFramer::ErrorCodeToString(int error_code) {
  switch (error_code) {
    case SPDY_NO_ERROR:
      return "NO_ERROR";
    case SPDY_INVALID_CONTROL_FRAME:
      return "INVALID_CONTROL_FRAME";
    case SPDY_CONTROL_PAYLOAD_TOO_LARGE:
      return "CONTROL_PAYLOAD_TOO_LARGE";
    case SPDY_ZLIB_INIT_FAILURE:
      return "ZLIB_INIT_FAILURE";
    case SPDY_UNSUPPORTED_VERSION:
      return "UNSUPPORTED_VERSION";
    case SPDY_DECOMPRESS_FAILURE:
      return "DECOMPRESS_FAILURE";
    case SPDY_COMPRESS_FAILURE:
      return "COMPRESS_FAILURE";
  }
  return "UNKNOWN_ERROR";
}

size_t SpdyFramer::ProcessInput(const char* data, size_t len) {
  DCHECK(visitor_);
  DCHECK(data);

  size_t original_len = len;
  while (len != 0) {
    switch (state_) {
      case SPDY_ERROR:
      case SPDY_DONE:
        goto bottom;

      case SPDY_AUTO_RESET:
      case SPDY_RESET:
        Reset();
        CHANGE_STATE(SPDY_READING_COMMON_HEADER);
        continue;

      case SPDY_READING_COMMON_HEADER: {
        int bytes_read = ProcessCommonHeader(data, len);
        len -= bytes_read;
        data += bytes_read;
        continue;
      }

      // Arguably, this case is not necessary, as no bytes are consumed here.
      // I felt it was a nice partitioning, however (which probably indicates
      // that it should be refactored into its own function!)
      case SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
        ProcessControlFrameHeader();
        continue;

      case SPDY_CONTROL_FRAME_PAYLOAD: {
        int bytes_read = ProcessControlFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
      }
        // intentional fallthrough
      case SPDY_IGNORE_REMAINING_PAYLOAD:
        // control frame has too-large payload
        // intentional fallthrough
      case SPDY_FORWARD_STREAM_FRAME: {
        int bytes_read = ProcessDataFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        continue;
      }
      default:
        break;
    }
  }
 bottom:
  return original_len - len;
}

size_t SpdyFramer::ProcessCommonHeader(const char* data, size_t len) {
  // This should only be called when we're in the SPDY_READING_COMMON_HEADER
  // state.
  DCHECK(state_ == SPDY_READING_COMMON_HEADER);

  int original_len = len;
  SpdyFrame current_frame(current_frame_buffer_, false);

  do {
    if (current_frame_len_ < SpdyFrame::size()) {
      size_t bytes_desired = SpdyFrame::size() - current_frame_len_;
      size_t bytes_to_append = std::min(bytes_desired, len);
      char* header_buffer = current_frame_buffer_;
      memcpy(&header_buffer[current_frame_len_], data, bytes_to_append);
      current_frame_len_ += bytes_to_append;
      data += bytes_to_append;
      len -= bytes_to_append;
      // Check for an empty data frame.
      if (current_frame_len_ == SpdyFrame::size() &&
          !current_frame.is_control_frame() &&
          current_frame.length() == 0) {
        if (current_frame.flags() & CONTROL_FLAG_FIN) {
          SpdyDataFrame data_frame(current_frame_buffer_, false);
          visitor_->OnStreamFrameData(data_frame.stream_id(), NULL, 0);
        }
        CHANGE_STATE(SPDY_AUTO_RESET);
      }
      break;
    }
    remaining_payload_ = current_frame.length();

    // This is just a sanity check for help debugging early frame errors.
    if (remaining_payload_ > 1000000u) {
      LOG(WARNING) <<
          "Unexpectedly large frame.  Spdy session is likely corrupt.";
    }

    // if we're here, then we have the common header all received.
    if (!current_frame.is_control_frame())
      CHANGE_STATE(SPDY_FORWARD_STREAM_FRAME);
    else
      CHANGE_STATE(SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER);
  } while (false);

  return original_len - len;
}

void SpdyFramer::ProcessControlFrameHeader() {
  DCHECK_EQ(SPDY_NO_ERROR, error_code_);
  DCHECK_LE(SpdyFrame::size(), current_frame_len_);
  SpdyControlFrame current_control_frame(current_frame_buffer_, false);

  // We check version before we check validity: version can never be 'invalid',
  // it can only be unsupported.
  if (current_control_frame.version() != kSpdyProtocolVersion) {
    set_error(SPDY_UNSUPPORTED_VERSION);
    return;
  }

  // Next up, check to see if we have valid data.  This should be after version
  // checking (otherwise if the the type were out of bounds due to a version
  // upgrade we would misclassify the error) and before checking the type
  // (type can definitely be out of bounds)
  if (!current_control_frame.AppearsToBeAValidControlFrame()) {
    set_error(SPDY_INVALID_CONTROL_FRAME);
    return;
  }

  // Do some sanity checking on the control frame sizes.
  switch (current_control_frame.type()) {
    case SYN_STREAM:
      if (current_control_frame.length() <
          SpdySynStreamControlFrame::size() - SpdyControlFrame::size())
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case SYN_REPLY:
      if (current_control_frame.length() <
          SpdySynReplyControlFrame::size() - SpdyControlFrame::size())
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case RST_STREAM:
      if (current_control_frame.length() !=
          SpdyRstStreamControlFrame::size() - SpdyFrame::size())
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case NOOP:
      // NOOP.  Swallow it.
      CHANGE_STATE(SPDY_AUTO_RESET);
      return;
    case GOAWAY:
      if (current_control_frame.length() !=
          SpdyGoAwayControlFrame::size() - SpdyFrame::size())
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    default:
      LOG(WARNING) << "Valid spdy control frame with unknown type: "
                   << current_control_frame.type();
      DCHECK(false);
      set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
  }

  // We only support version 1 of this protocol.
  if (current_control_frame.version() != kSpdyProtocolVersion) {
    set_error(SPDY_UNSUPPORTED_VERSION);
    return;
  }

  remaining_control_payload_ = current_control_frame.length();
  if (remaining_control_payload_ > kControlFrameBufferMaxSize) {
    set_error(SPDY_CONTROL_PAYLOAD_TOO_LARGE);
    return;
  }

  ExpandControlFrameBuffer(remaining_control_payload_);
  CHANGE_STATE(SPDY_CONTROL_FRAME_PAYLOAD);
}

size_t SpdyFramer::ProcessControlFramePayload(const char* data, size_t len) {
  size_t original_len = len;
  do {
    if (remaining_control_payload_) {
      size_t amount_to_consume = std::min(remaining_control_payload_, len);
      memcpy(&current_frame_buffer_[current_frame_len_], data,
             amount_to_consume);
      current_frame_len_ += amount_to_consume;
      data += amount_to_consume;
      len -= amount_to_consume;
      remaining_control_payload_ -= amount_to_consume;
      remaining_payload_ -= amount_to_consume;
      if (remaining_control_payload_)
        break;
    }
    SpdyControlFrame control_frame(current_frame_buffer_, false);
    visitor_->OnControl(&control_frame);

    // If this is a FIN, tell the caller.
    if (control_frame.type() == SYN_REPLY &&
        control_frame.flags() & CONTROL_FLAG_FIN) {
      visitor_->OnStreamFrameData(reinterpret_cast<SpdySynReplyControlFrame*>(
                                      &control_frame)->stream_id(),
                                  NULL, 0);
    }

    CHANGE_STATE(SPDY_IGNORE_REMAINING_PAYLOAD);
  } while (false);
  return original_len - len;
}

size_t SpdyFramer::ProcessDataFramePayload(const char* data, size_t len) {
  size_t original_len = len;

  SpdyDataFrame current_data_frame(current_frame_buffer_, false);
  if (remaining_payload_) {
    size_t amount_to_forward = std::min(remaining_payload_, len);
    if (amount_to_forward && state_ != SPDY_IGNORE_REMAINING_PAYLOAD) {
      if (current_data_frame.flags() & DATA_FLAG_COMPRESSED) {
        // TODO(mbelshe): Assert that the decompressor is init'ed.
        if (!InitializeDecompressor())
          return NULL;

        size_t decompressed_max_size = amount_to_forward * 100;
        scoped_array<char> decompressed(new char[decompressed_max_size]);
        decompressor_->next_in = reinterpret_cast<Bytef*>(
            const_cast<char*>(data));
        decompressor_->avail_in = amount_to_forward;
        decompressor_->next_out =
            reinterpret_cast<Bytef*>(decompressed.get());
        decompressor_->avail_out = decompressed_max_size;

        int rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
        if (rv != Z_OK) {
          set_error(SPDY_DECOMPRESS_FAILURE);
          return 0;
        }
        size_t decompressed_size = decompressed_max_size -
                                   decompressor_->avail_out;
        // Only inform the visitor if there is data.
        if (decompressed_size)
          visitor_->OnStreamFrameData(current_data_frame.stream_id(),
                                      decompressed.get(),
                                      decompressed_size);
        amount_to_forward -= decompressor_->avail_in;
      } else {
        // The data frame was not compressed.
        // Only inform the visitor if there is data.
        if (amount_to_forward)
          visitor_->OnStreamFrameData(current_data_frame.stream_id(),
                                      data, amount_to_forward);
      }
    }
    data += amount_to_forward;
    len -= amount_to_forward;
    remaining_payload_ -= amount_to_forward;

    // If the FIN flag is set, and there is no more data in this data
    // frame, inform the visitor of EOF via a 0-length data frame.
    if (!remaining_payload_ &&
        current_data_frame.flags() & DATA_FLAG_FIN)
      visitor_->OnStreamFrameData(current_data_frame.stream_id(), NULL,
                                  0);
  } else {
    CHANGE_STATE(SPDY_AUTO_RESET);
  }
  return original_len - len;
}

void SpdyFramer::ExpandControlFrameBuffer(size_t size) {
  DCHECK(size < kControlFrameBufferMaxSize);
  if (size < current_frame_capacity_)
    return;

  int alloc_size = size + SpdyFrame::size();
  char* new_buffer = new char[alloc_size];
  memcpy(new_buffer, current_frame_buffer_, current_frame_len_);
  delete [] current_frame_buffer_;
  current_frame_capacity_ = alloc_size;
  current_frame_buffer_ = new_buffer;
}

bool SpdyFramer::ParseHeaderBlock(const SpdyFrame* frame,
                                  SpdyHeaderBlock* block) {
  SpdyControlFrame control_frame(frame->data(), false);
  uint32 type = control_frame.type();
  if (type != SYN_STREAM && type != SYN_REPLY)
    return false;

  // Find the header data within the control frame.
  scoped_ptr<SpdyFrame> decompressed_frame(DecompressFrame(frame));
  if (!decompressed_frame.get())
    return false;

  const char *header_data = NULL;
  int header_length = 0;

  switch (type) {
    case SYN_STREAM:
      {
        SpdySynStreamControlFrame syn_frame(decompressed_frame->data(), false);
        header_data = syn_frame.header_block();
        header_length = syn_frame.header_block_len();
      }
      break;
    case SYN_REPLY:
      {
        SpdySynReplyControlFrame syn_frame(decompressed_frame->data(), false);
        header_data = syn_frame.header_block();
        header_length = syn_frame.header_block_len();
      }
      break;
  }

  SpdyFrameBuilder builder(header_data, header_length);
  void* iter = NULL;
  uint16 num_headers;
  if (builder.ReadUInt16(&iter, &num_headers)) {
    for (int index = 0; index < num_headers; ++index) {
      std::string name;
      std::string value;
      if (!builder.ReadString(&iter, &name))
        break;
      if (!builder.ReadString(&iter, &value))
        break;
      if (block->find(name) == block->end()) {
        (*block)[name] = value;
      } else {
        return false;
      }
    }
    return true;
  }
  return false;
}

/* static */
bool SpdyFramer::ParseSettings(const SpdySettingsControlFrame* frame,
                               SpdySettings* settings) {
  DCHECK_EQ(frame->type(), SETTINGS);
  DCHECK(settings);

  SpdyFrameBuilder parser(frame->header_block(), frame->header_block_len());
  void* iter = NULL;
  for (size_t index = 0; index < frame->num_entries(); ++index) {
    uint32 id;
    uint32 value;
    if (!parser.ReadUInt32(&iter, &id))
      return false;
    if (!parser.ReadUInt32(&iter, &value))
      return false;
    settings->insert(settings->end(), std::make_pair(id, value));
  }
  return true;
}

SpdySynStreamControlFrame* SpdyFramer::CreateSynStream(
    SpdyStreamId stream_id, SpdyStreamId associated_stream_id, int priority,
    SpdyControlFlags flags, bool compressed, SpdyHeaderBlock* headers) {
  SpdyFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length and flags
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(associated_stream_id);
  frame.WriteUInt16(ntohs(priority) << 6);  // Priority.

  frame.WriteUInt16(headers->size());  // Number of headers.
  SpdyHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }

  // Write the length and flags.
  size_t length = frame.length() - SpdyFrame::size();
  DCHECK(length < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(static_cast<uint32>(length));
  flags_length.flags_[0] = flags;
  frame.WriteBytesToOffset(4, &flags_length, sizeof(flags_length));

  scoped_ptr<SpdyFrame> syn_frame(frame.take());
  if (compressed) {
    return reinterpret_cast<SpdySynStreamControlFrame*>(
        CompressFrame(syn_frame.get()));
  }
  return reinterpret_cast<SpdySynStreamControlFrame*>(syn_frame.release());
}

/* static */
SpdyRstStreamControlFrame* SpdyFramer::CreateRstStream(SpdyStreamId stream_id,
                                                       int status) {
  SpdyFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(RST_STREAM);
  frame.WriteUInt32(8);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(status);
  return reinterpret_cast<SpdyRstStreamControlFrame*>(frame.take());
}

/* static */
SpdyGoAwayControlFrame* SpdyFramer::CreateGoAway(
    SpdyStreamId last_accepted_stream_id) {
  SpdyFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(GOAWAY);
  size_t go_away_size = SpdyGoAwayControlFrame::size() - SpdyFrame::size();
  frame.WriteUInt32(go_away_size);
  frame.WriteUInt32(last_accepted_stream_id);
  return reinterpret_cast<SpdyGoAwayControlFrame*>(frame.take());
}

/* static */
SpdySettingsControlFrame* SpdyFramer::CreateSettings(
    const SpdySettings& values) {
  SpdyFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(SETTINGS);
  size_t settings_size = SpdySettingsControlFrame::size() - SpdyFrame::size() +
      8 * values.size();
  frame.WriteUInt32(settings_size);
  frame.WriteUInt32(values.size());
  SpdySettings::const_iterator it = values.begin();
  while (it != values.end()) {
    frame.WriteUInt32(it->first.id_);
    frame.WriteUInt32(it->second);
    ++it;
  }
  return reinterpret_cast<SpdySettingsControlFrame*>(frame.take());
}

SpdySynReplyControlFrame* SpdyFramer::CreateSynReply(SpdyStreamId stream_id,
    SpdyControlFlags flags, bool compressed, SpdyHeaderBlock* headers) {

  SpdyFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(SYN_REPLY);
  frame.WriteUInt32(0);  // Placeholder for the length and flags.
  frame.WriteUInt32(stream_id);
  frame.WriteUInt16(0);  // Unused

  frame.WriteUInt16(headers->size());  // Number of headers.
  SpdyHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    // TODO(mbelshe): Headers need to be sorted.
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }

  // Write the length and flags.
  size_t length = frame.length() - SpdyFrame::size();
  DCHECK(length < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(static_cast<uint32>(length));
  flags_length.flags_[0] = flags;
  frame.WriteBytesToOffset(4, &flags_length, sizeof(flags_length));

  scoped_ptr<SpdyFrame> reply_frame(frame.take());
  if (compressed) {
    return reinterpret_cast<SpdySynReplyControlFrame*>(
        CompressFrame(reply_frame.get()));
  }
  return reinterpret_cast<SpdySynReplyControlFrame*>(reply_frame.release());
}

SpdyDataFrame* SpdyFramer::CreateDataFrame(SpdyStreamId stream_id,
                                           const char* data,
                                           uint32 len, SpdyDataFlags flags) {
  SpdyFrameBuilder frame;

  frame.WriteUInt32(stream_id);

  DCHECK(len < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(len);
  flags_length.flags_[0] = flags;
  frame.WriteBytes(&flags_length, sizeof(flags_length));

  frame.WriteBytes(data, len);
  scoped_ptr<SpdyFrame> data_frame(frame.take());
  if (flags & DATA_FLAG_COMPRESSED)
    return reinterpret_cast<SpdyDataFrame*>(CompressFrame(data_frame.get()));
  return reinterpret_cast<SpdyDataFrame*>(data_frame.release());
}

/* static */
SpdyControlFrame* SpdyFramer::CreateNopFrame() {
  SpdyFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kSpdyProtocolVersion);
  frame.WriteUInt16(NOOP);
  frame.WriteUInt32(0);
  return reinterpret_cast<SpdyControlFrame*>(frame.take());
}

// The following compression setting are based on Brian Olson's analysis. See
// https://groups.google.com/group/spdy-dev/browse_thread/thread/dfaf498542fac792
// for more details.
static const int kCompressorLevel = 9;
static const int kCompressorWindowSizeInBits = 11;
static const int kCompressorMemLevel = 1;

// This is just a hacked dictionary to use for shrinking HTTP-like headers.
// TODO(mbelshe): Use a scientific methodology for computing the dictionary.
const char SpdyFramer::kDictionary[] =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";
const int SpdyFramer::kDictionarySize = arraysize(kDictionary);

static uLong dictionary_id = 0;

bool SpdyFramer::InitializeCompressor() {
  if (compressor_.get())
    return true;  // Already initialized.

  compressor_.reset(new z_stream);
  memset(compressor_.get(), 0, sizeof(z_stream));

  int success = deflateInit2(compressor_.get(),
                             kCompressorLevel,
                             Z_DEFLATED,
                             kCompressorWindowSizeInBits,
                             kCompressorMemLevel,
                             Z_DEFAULT_STRATEGY);
  if (success == Z_OK)
    success = deflateSetDictionary(compressor_.get(),
                                   reinterpret_cast<const Bytef*>(kDictionary),
                                   kDictionarySize);
  if (success != Z_OK)
    compressor_.reset(NULL);
  return success == Z_OK;
}

bool SpdyFramer::InitializeDecompressor() {
  if (decompressor_.get())
    return true;  // Already initialized.

  decompressor_.reset(new z_stream);
  memset(decompressor_.get(), 0, sizeof(z_stream));

  // Compute the id of our dictionary so that we know we're using the
  // right one when asked for it.
  if (dictionary_id == 0) {
    dictionary_id = adler32(0L, Z_NULL, 0);
    dictionary_id = adler32(dictionary_id,
                            reinterpret_cast<const Bytef*>(kDictionary),
                            kDictionarySize);
  }

  int success = inflateInit(decompressor_.get());
  if (success != Z_OK)
    decompressor_.reset(NULL);
  return success == Z_OK;
}

bool SpdyFramer::GetFrameBoundaries(const SpdyFrame* frame,
                                    int* payload_length,
                                    int* header_length,
                                    const char** payload) const {
  size_t frame_size;
  if (frame->is_control_frame()) {
    const SpdyControlFrame* control_frame =
        reinterpret_cast<const SpdyControlFrame*>(frame);
    switch (control_frame->type()) {
      case SYN_STREAM:
        {
          const SpdySynStreamControlFrame *syn_frame =
              reinterpret_cast<const SpdySynStreamControlFrame*>(frame);
          frame_size = SpdySynStreamControlFrame::size();
          *payload_length = syn_frame->header_block_len();
          *header_length = frame_size;
          *payload = frame->data() + *header_length;
        }
        break;
      case SYN_REPLY:
        {
          const SpdySynReplyControlFrame *syn_frame =
              reinterpret_cast<const SpdySynReplyControlFrame*>(frame);
          frame_size = SpdySynReplyControlFrame::size();
          *payload_length = syn_frame->header_block_len();
          *header_length = frame_size;
          *payload = frame->data() + *header_length;
        }
        break;
      default:
        // TODO(mbelshe): set an error?
        return false;  // We can't compress this frame!
    }
  } else {
    frame_size = SpdyFrame::size();
    *header_length = frame_size;
    *payload_length = frame->length();
    *payload = frame->data() + SpdyFrame::size();
  }
  return true;
}


SpdyFrame* SpdyFramer::CompressFrame(const SpdyFrame* frame) {
  int payload_length;
  int header_length;
  const char* payload;

  static StatsCounter pre_compress_bytes("spdy.PreCompressSize");
  static StatsCounter post_compress_bytes("spdy.PostCompressSize");

  if (!enable_compression_)
    return DuplicateFrame(frame);

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  if (!InitializeCompressor())
    return NULL;

  // TODO(mbelshe): Should we have a zlib header like what http servers do?

  // Create an output frame.
  int compressed_max_size = deflateBound(compressor_.get(), payload_length);
  int new_frame_size = header_length + compressed_max_size;
  SpdyFrame* new_frame = new SpdyFrame(new_frame_size);
  memcpy(new_frame->data(), frame->data(), frame->length() + SpdyFrame::size());

  compressor_->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload));
  compressor_->avail_in = payload_length;
  compressor_->next_out = reinterpret_cast<Bytef*>(new_frame->data()) +
                          header_length;
  compressor_->avail_out = compressed_max_size;

  // Data packets have a 'compressed flag
  if (!new_frame->is_control_frame()) {
    SpdyDataFrame* data_frame = reinterpret_cast<SpdyDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() | DATA_FLAG_COMPRESSED);
  }

  int rv = deflate(compressor_.get(), Z_SYNC_FLUSH);
  if (rv != Z_OK) {  // How can we know that it compressed everything?
    // This shouldn't happen, right?
    delete new_frame;
    return NULL;
  }

  int compressed_size = compressed_max_size - compressor_->avail_out;
  new_frame->set_length(header_length + compressed_size - SpdyFrame::size());

  pre_compress_bytes.Add(payload_length);
  post_compress_bytes.Add(new_frame->length());

  return new_frame;
}

SpdyFrame* SpdyFramer::DecompressFrame(const SpdyFrame* frame) {
  int payload_length;
  int header_length;
  const char* payload;

  static StatsCounter pre_decompress_bytes("spdy.PreDeCompressSize");
  static StatsCounter post_decompress_bytes("spdy.PostDeCompressSize");

  if (!enable_compression_)
    return DuplicateFrame(frame);

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  if (!frame->is_control_frame()) {
    const SpdyDataFrame* data_frame =
        reinterpret_cast<const SpdyDataFrame*>(frame);
    if ((data_frame->flags() & DATA_FLAG_COMPRESSED) == 0)
      return DuplicateFrame(frame);
  }

  if (!InitializeDecompressor())
    return NULL;

  // TODO(mbelshe): Should we have a zlib header like what http servers do?

  // Create an output frame.  Assume it does not need to be longer than
  // the input data.
  int decompressed_max_size = kControlFrameBufferInitialSize;
  int new_frame_size = header_length + decompressed_max_size;
  SpdyFrame* new_frame = new SpdyFrame(new_frame_size);
  memcpy(new_frame->data(), frame->data(), frame->length() + SpdyFrame::size());

  decompressor_->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload));
  decompressor_->avail_in = payload_length;
  decompressor_->next_out = reinterpret_cast<Bytef*>(new_frame->data()) +
      header_length;
  decompressor_->avail_out = decompressed_max_size;

  int rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
  if (rv == Z_NEED_DICT) {
    // Need to try again with the right dictionary.
    if (decompressor_->adler == dictionary_id) {
      rv = inflateSetDictionary(decompressor_.get(), (const Bytef*)kDictionary,
                                kDictionarySize);
      if (rv == Z_OK)
        rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
    }
  }
  if (rv != Z_OK) {  // How can we know that it decompressed everything?
    delete new_frame;
    return NULL;
  }

  // Unset the compressed flag for data frames.
  if (!new_frame->is_control_frame()) {
    SpdyDataFrame* data_frame = reinterpret_cast<SpdyDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() & ~DATA_FLAG_COMPRESSED);
  }

  int decompressed_size = decompressed_max_size - decompressor_->avail_out;
  new_frame->set_length(header_length + decompressed_size - SpdyFrame::size());

  pre_decompress_bytes.Add(frame->length());
  post_decompress_bytes.Add(new_frame->length());

  return new_frame;
}

SpdyFrame* SpdyFramer::DuplicateFrame(const SpdyFrame* frame) {
  int size = SpdyFrame::size() + frame->length();
  SpdyFrame* new_frame = new SpdyFrame(size);
  memcpy(new_frame->data(), frame->data(), size);
  return new_frame;
}

void SpdyFramer::set_enable_compression(bool value) {
  enable_compression_ = value;
}

void SpdyFramer::set_enable_compression_default(bool value) {
  compression_default_ = value;
}

}  // namespace spdy

