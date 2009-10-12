// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/stats_counters.h"

#include "flip_framer.h"  // cross-google3 directory naming.
#include "flip_frame_builder.h"
#include "flip_bitmasks.h"

#include "third_party/zlib/zlib.h"

namespace flip {

// The initial size of the control frame buffer; this is used internally
// as we we parse though control frames.
static const size_t kControlFrameBufferInitialSize = 32 * 1024;
// The maximum size of the control frame buffer that we support.
// TODO(mbelshe): We should make this stream-based so there are no limits.
static const size_t kControlFrameBufferMaxSize = 64 * 1024;

// This implementation of Flip is version 1.
static const int kFlipProtocolVersion = 1;

// By default is compression on or off.
bool FlipFramer::compression_default_ = true;

#ifdef DEBUG_FLIP_STATE_CHANGES
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

FlipFramer::FlipFramer()
    : state_(FLIP_RESET),
      error_code_(FLIP_NO_ERROR),
      remaining_payload_(0),
      remaining_control_payload_(0),
      current_frame_buffer_(NULL),
      current_frame_len_(0),
      current_frame_capacity_(0),
      enable_compression_(compression_default_),
      visitor_(NULL) {
}

FlipFramer::~FlipFramer() {
  if (compressor_.get()) {
    deflateEnd(compressor_.get());
  }
  if (decompressor_.get()) {
    inflateEnd(decompressor_.get());
  }
  delete [] current_frame_buffer_;
}

void FlipFramer::Reset() {
  state_ = FLIP_RESET;
  error_code_ = FLIP_NO_ERROR;
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

const char* FlipFramer::StateToString(int state) {
  switch (state) {
    case FLIP_ERROR:
      return "ERROR";
    case FLIP_DONE:
      return "DONE";
    case FLIP_AUTO_RESET:
      return "AUTO_RESET";
    case FLIP_RESET:
      return "RESET";
    case FLIP_READING_COMMON_HEADER:
      return "READING_COMMON_HEADER";
    case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return "INTERPRET_CONTROL_FRAME_COMMON_HEADER";
    case FLIP_CONTROL_FRAME_PAYLOAD:
      return "CONTROL_FRAME_PAYLOAD";
    case FLIP_IGNORE_REMAINING_PAYLOAD:
      return "IGNORE_REMAINING_PAYLOAD";
    case FLIP_FORWARD_STREAM_FRAME:
      return "FORWARD_STREAM_FRAME";
  }
  return "UNKNOWN_STATE";
}

size_t FlipFramer::BytesSafeToRead() const {
  switch (state_) {
    case FLIP_ERROR:
    case FLIP_DONE:
    case FLIP_AUTO_RESET:
    case FLIP_RESET:
      return 0;
    case FLIP_READING_COMMON_HEADER:
      DCHECK(current_frame_len_ < sizeof(FlipFrame));
      return sizeof(FlipFrame) - current_frame_len_;
    case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return 0;
    case FLIP_CONTROL_FRAME_PAYLOAD:
    case FLIP_IGNORE_REMAINING_PAYLOAD:
    case FLIP_FORWARD_STREAM_FRAME:
      return remaining_payload_;
  }
  // We should never get to here.
  return 0;
}

void FlipFramer::set_error(FlipError error) {
  DCHECK(false);
  DCHECK(visitor_);
  error_code_ = error;
  visitor_->OnError(this);
}

const char* FlipFramer::ErrorCodeToString(int error_code) {
  switch (error_code) {
    case FLIP_NO_ERROR:
      return "NO_ERROR";
    case FLIP_UNKNOWN_CONTROL_TYPE:
      return "UNKNOWN_CONTROL_TYPE";
    case FLIP_INVALID_CONTROL_FRAME:
      return "INVALID_CONTROL_FRAME";
    case FLIP_CONTROL_PAYLOAD_TOO_LARGE:
      return "CONTROL_PAYLOAD_TOO_LARGE";
    case FLIP_ZLIB_INIT_FAILURE:
      return "ZLIB_INIT_FAILURE";
    case FLIP_UNSUPPORTED_VERSION:
      return "UNSUPPORTED_VERSION";
    case FLIP_DECOMPRESS_FAILURE:
      return "DECOMPRESS_FAILURE";
  }
  return "UNKNOWN_STATE";
}

size_t FlipFramer::ProcessInput(const char* data, size_t len) {
  DCHECK(visitor_);
  DCHECK(data);

  size_t original_len = len;
  while (len != 0) {
    FlipControlFrame* current_control_frame =
        reinterpret_cast<FlipControlFrame*>(current_frame_buffer_);
    FlipDataFrame* current_data_frame =
        reinterpret_cast<FlipDataFrame*>(current_frame_buffer_);

    switch (state_) {
      case FLIP_ERROR:
      case FLIP_DONE:
        goto bottom;

      case FLIP_AUTO_RESET:
      case FLIP_RESET:
        Reset();
        CHANGE_STATE(FLIP_READING_COMMON_HEADER);
        continue;

      case FLIP_READING_COMMON_HEADER: {
        int bytes_read = ProcessCommonHeader(data, len);
        len -= bytes_read;
        data += bytes_read;
        continue;
      }

      // Arguably, this case is not necessary, as no bytes are consumed here.
      // I felt it was a nice partitioning, however (which probably indicates
      // that it should be refactored into its own function!)
      case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
        DCHECK(error_code_ == 0);
        DCHECK(current_frame_len_ >= sizeof(FlipFrame));
        // Do some sanity checking on the control frame sizes.
        switch (current_control_frame->type()) {
          case SYN_STREAM:
            // NOTE: sizeof(FlipSynStreamControlFrame) is not accurate.
            if (current_control_frame->length() <
                sizeof(FlipSynStreamControlFrame) - sizeof(FlipControlFrame))
              set_error(FLIP_INVALID_CONTROL_FRAME);
            break;
          case SYN_REPLY:
            if (current_control_frame->length() <
                sizeof(FlipSynReplyControlFrame) - sizeof(FlipControlFrame))
              set_error(FLIP_INVALID_CONTROL_FRAME);
            break;
          case FIN_STREAM:
            if (current_control_frame->length() !=
                sizeof(FlipFinStreamControlFrame) - sizeof(FlipFrame))
              set_error(FLIP_INVALID_CONTROL_FRAME);
            break;
          case NOOP:
            // NOP.  Swallow it.
            CHANGE_STATE(FLIP_AUTO_RESET);
            continue;
          default:
            set_error(FLIP_UNKNOWN_CONTROL_TYPE);
            break;
        }

        // We only support version 1 of this protocol.
        if (current_control_frame->version() != kFlipProtocolVersion)
          set_error(FLIP_UNSUPPORTED_VERSION);

        if (error_code_) {
          CHANGE_STATE(FLIP_ERROR);
          goto bottom;
        }

        remaining_control_payload_ = current_control_frame->length();
        if (remaining_control_payload_ > kControlFrameBufferMaxSize) {
          set_error(FLIP_CONTROL_PAYLOAD_TOO_LARGE);
          CHANGE_STATE(FLIP_ERROR);
          goto bottom;
        }
        ExpandControlFrameBuffer(remaining_control_payload_);
        CHANGE_STATE(FLIP_CONTROL_FRAME_PAYLOAD);
        continue;

      case FLIP_CONTROL_FRAME_PAYLOAD: {
        int bytes_read = ProcessControlFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
      }
        // intentional fallthrough
      case FLIP_IGNORE_REMAINING_PAYLOAD:
        // control frame has too-large payload
        // intentional fallthrough
      case FLIP_FORWARD_STREAM_FRAME:
        if (remaining_payload_) {
          size_t amount_to_forward = std::min(remaining_payload_, len);
          if (amount_to_forward && state_ != FLIP_IGNORE_REMAINING_PAYLOAD) {
            const FlipDataFrame* data_frame =
                reinterpret_cast<const FlipDataFrame*>(current_data_frame);
            if (data_frame->flags() & DATA_FLAG_COMPRESSED) {
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
                set_error(FLIP_DECOMPRESS_FAILURE);
                goto bottom;
              }
              size_t decompressed_size = decompressed_max_size -
                                      decompressor_->avail_out;
              visitor_->OnStreamFrameData(current_data_frame->stream_id(),
                                          decompressed.get(),
                                          decompressed_size);
              amount_to_forward -= decompressor_->avail_in;
            } else {
              // The data frame was not compressed
              visitor_->OnStreamFrameData(current_data_frame->stream_id(),
                                          data, amount_to_forward);
            }
          }
          data += amount_to_forward;
          len -= amount_to_forward;
          remaining_payload_ -= amount_to_forward;
        } else {
          CHANGE_STATE(FLIP_AUTO_RESET);
        }
        continue;
      default:
        break;
    }
  }
 bottom:
  return original_len - len;
}

size_t FlipFramer::ProcessCommonHeader(const char* data, size_t len) {
  // This should only be called when we're in the FLIP_READING_COMMON_HEADER
  // state.
  DCHECK(state_ == FLIP_READING_COMMON_HEADER);

  int original_len = len;
  FlipDataFrame* current_frame =
      reinterpret_cast<FlipDataFrame*>(current_frame_buffer_);

  do {
    if (current_frame_len_ < sizeof(FlipFrame)) {
      size_t bytes_desired = sizeof(FlipFrame) - current_frame_len_;
      size_t bytes_to_append = std::min(bytes_desired, len);
      char* header_buffer = current_frame_buffer_;
      memcpy(&header_buffer[current_frame_len_], data, bytes_to_append);
      current_frame_len_ += bytes_to_append;
      data += bytes_to_append;
      len -= bytes_to_append;
      // Check for an empty data packet.
      if (current_frame_len_ == sizeof(FlipFrame) &&
          !current_frame->is_control_frame() &&
          current_frame->length() == 0) {
        visitor_->OnStreamFrameData(current_frame->stream_id(), NULL, 0);
        CHANGE_STATE(FLIP_RESET);
      }
      break;
    }
    remaining_payload_ = current_frame->length();
    // if we're here, then we have the common header all received.
    if (!current_frame->is_control_frame())
      CHANGE_STATE(FLIP_FORWARD_STREAM_FRAME);
    else
      CHANGE_STATE(FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER);
  } while (false);

  return original_len - len;
}

size_t FlipFramer::ProcessControlFramePayload(const char* data, size_t len) {
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
    FlipControlFrame* control_frame =
        reinterpret_cast<FlipControlFrame*>(current_frame_buffer_);
    visitor_->OnControl(control_frame);
    CHANGE_STATE(FLIP_IGNORE_REMAINING_PAYLOAD);
  } while (false);
  return original_len - len;
}

void FlipFramer::ExpandControlFrameBuffer(size_t size) {
  DCHECK(size < kControlFrameBufferMaxSize);
  if (size < current_frame_capacity_)
    return;

  int alloc_size = size + sizeof(FlipFrame);
  char* new_buffer = new char[alloc_size];
  memcpy(new_buffer, current_frame_buffer_, current_frame_len_);
  current_frame_capacity_ = alloc_size;
  current_frame_buffer_ = new_buffer;
}

bool FlipFramer::ParseHeaderBlock(const FlipFrame* frame,
                                  FlipHeaderBlock* block) {
  uint32 type = reinterpret_cast<const FlipControlFrame*>(frame)->type();
  if (type != SYN_STREAM && type != SYN_REPLY)
    return false;

  // Find the header data within the control frame.
  scoped_array<FlipSynStreamControlFrame> control_frame(
    reinterpret_cast<FlipSynStreamControlFrame*>(DecompressFrame(frame)));
  if (!control_frame.get())
    return false;
  const char *header_data = control_frame.get()->header_block();
  int header_length = control_frame.get()->header_block_len();

  FlipFrameBuilder builder(header_data, header_length);
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
      if (block->empty()) {
        (*block)[name] = value;
      } else {
        FlipHeaderBlock::iterator last = --block->end();
        if (block->key_comp()(last->first, name)) {
          block->insert(block->end(),
                       std::pair<std::string, std::string>(name, value));
        } else {
          return false;
        }
      }
    }
    return true;
  }
  return false;
}

FlipSynStreamControlFrame* FlipFramer::CreateSynStream(
    int stream_id,
    int priority,
    bool compress,
    FlipHeaderBlock* headers) {
  FlipFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(stream_id);
  frame.WriteUInt16(ntohs(priority) << 6);  // Priority.

  frame.WriteUInt16(headers->size());  // Number of headers.
  FlipHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - sizeof(FlipFrame));
  if (compress) {
    FlipSynStreamControlFrame* new_frame =
        reinterpret_cast<FlipSynStreamControlFrame*>(
        CompressFrame(frame.data()));
    return new_frame;
  }

  return reinterpret_cast<FlipSynStreamControlFrame*>(frame.take());
}

/* static */
FlipFinStreamControlFrame* FlipFramer::CreateFinStream(int stream_id,
                                                       int status) {
  FlipFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(FIN_STREAM);
  frame.WriteUInt32(8);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(status);
  return reinterpret_cast<FlipFinStreamControlFrame*>(frame.take());
}

FlipSynReplyControlFrame* FlipFramer::CreateSynReply(int stream_id,
    bool compressed, FlipHeaderBlock* headers) {

  FlipFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(SYN_REPLY);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(stream_id);
  frame.WriteUInt16(0);  // Priority.

  frame.WriteUInt16(headers->size());  // Number of headers.
  FlipHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    // TODO(mbelshe): Headers need to be sorted.
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - sizeof(FlipFrame));
  if (compressed)
    return reinterpret_cast<FlipSynReplyControlFrame*>(
        CompressFrame(frame.data()));
  return reinterpret_cast<FlipSynReplyControlFrame*>(frame.take());
}

FlipDataFrame* FlipFramer::CreateDataFrame(int stream_id,
                                           const char* data,
                                           int len, bool compressed) {
  FlipFrameBuilder frame;

  frame.WriteUInt32(stream_id);

  frame.WriteUInt32(len);
  frame.WriteBytes(data, len);
  if (compressed)
    return reinterpret_cast<FlipDataFrame*>(CompressFrame(frame.data()));
  return reinterpret_cast<FlipDataFrame*>(frame.take());
}

static const int kCompressorLevel = Z_DEFAULT_COMPRESSION;
// This is just a hacked dictionary to use for shrinking HTTP-like headers.
// TODO(mbelshe): Use a scientific methodology for computing the dictionary.
static const char dictionary[] =
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
static uLong dictionary_id = 0;

bool FlipFramer::InitializeCompressor() {
  if (compressor_.get())
    return true;  // Already initialized.

  compressor_.reset(new z_stream);
  memset(compressor_.get(), 0, sizeof(z_stream));

  int success = deflateInit(compressor_.get(), kCompressorLevel);
  if (success == Z_OK)
    success = deflateSetDictionary(compressor_.get(),
                                   reinterpret_cast<const Bytef*>(dictionary),
                                   sizeof(dictionary));
  if (success != Z_OK)
    compressor_.reset(NULL);
  return success == Z_OK;
}

bool FlipFramer::InitializeDecompressor() {
  if (decompressor_.get())
    return true;  // Already initialized.

  decompressor_.reset(new z_stream);
  memset(decompressor_.get(), 0, sizeof(z_stream));

  // Compute the id of our dictionary so that we know we're using the
  // right one when asked for it.
  if (dictionary_id == 0) {
    dictionary_id = adler32(0L, Z_NULL, 0);
    dictionary_id = adler32(dictionary_id,
                            reinterpret_cast<const Bytef*>(dictionary),
                            sizeof(dictionary));
  }

  int success = inflateInit(decompressor_.get());
  if (success != Z_OK)
    decompressor_.reset(NULL);
  return success == Z_OK;
}

bool FlipFramer::GetFrameBoundaries(const FlipFrame* frame,
                                    int* payload_length,
                                    int* header_length,
                                    const unsigned char** payload) const {
  if (frame->is_control_frame()) {
    const FlipControlFrame* control_frame =
        reinterpret_cast<const FlipControlFrame*>(frame);
    switch (control_frame->type()) {
      case SYN_STREAM:
      case SYN_REPLY:
        {
          const FlipSynStreamControlFrame *syn_frame =
              reinterpret_cast<const FlipSynStreamControlFrame*>(frame);
          *payload_length = syn_frame->header_block_len();
          *header_length = sizeof(FlipFrame) + syn_frame->length() -
              syn_frame->header_block_len();
          *payload = reinterpret_cast<const unsigned char*>(frame) +
              *header_length;
        }
        break;
      default:
        // TODO(mbelshe): set an error?
        return false;  // We can't compress this frame!
    }
  } else {
    *header_length = sizeof(FlipFrame);
    *payload_length = frame->length();
    *payload = reinterpret_cast<const unsigned char*>(frame) +
        sizeof(FlipFrame);
  }
  DCHECK(static_cast<size_t>(*header_length) <=
      sizeof(FlipFrame) + *payload_length);
  return true;
}


FlipFrame* FlipFramer::CompressFrame(const FlipFrame* frame) {
  int payload_length;
  int header_length;
  const unsigned char* payload;

  static StatsCounter pre_compress_bytes("flip.PreCompressSize");
  static StatsCounter post_compress_bytes("flip.PostCompressSize");

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
  FlipFrame* new_frame =
      reinterpret_cast<FlipFrame*>(new char[new_frame_size]);
  memcpy(new_frame, frame, header_length);

  compressor_->next_in = const_cast<Bytef*>(payload);
  compressor_->avail_in = payload_length;
  compressor_->next_out = reinterpret_cast<Bytef*>(new_frame) + header_length;
  compressor_->avail_out = compressed_max_size;

  // Data packets have a 'compressed flag
  if (!new_frame->is_control_frame()) {
    FlipDataFrame* data_frame = reinterpret_cast<FlipDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() | DATA_FLAG_COMPRESSED);
  }

  int rv = deflate(compressor_.get(), Z_SYNC_FLUSH);
  if (rv != Z_OK) {  // How can we know that it compressed everything?
    // This shouldn't happen, right?
    free(new_frame);
    return NULL;
  }

  int compressed_size = compressed_max_size - compressor_->avail_out;
  new_frame->set_length(header_length + compressed_size - sizeof(FlipFrame));

  pre_compress_bytes.Add(payload_length);
  post_compress_bytes.Add(new_frame->length());

  return new_frame;
}

FlipFrame* FlipFramer::DecompressFrame(const FlipFrame* frame) {
  int payload_length;
  int header_length;
  const unsigned char* payload;

  static StatsCounter pre_decompress_bytes("flip.PreDeCompressSize");
  static StatsCounter post_decompress_bytes("flip.PostDeCompressSize");

  if (!enable_compression_)
    return DuplicateFrame(frame);

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  if (!frame->is_control_frame()) {
    const FlipDataFrame* data_frame =
        reinterpret_cast<const FlipDataFrame*>(frame);
    if (!data_frame->flags() & DATA_FLAG_COMPRESSED)
      return DuplicateFrame(frame);
  }

  if (!InitializeDecompressor())
    return NULL;

  // TODO(mbelshe): Should we have a zlib header like what http servers do?

  // Create an output frame.  Assume it does not need to be longer than
  // the input data.
  int decompressed_max_size = kControlFrameBufferInitialSize;
  int new_frame_size = header_length + decompressed_max_size;
  FlipFrame* new_frame =
      reinterpret_cast<FlipFrame*>(new char[new_frame_size]);
  memcpy(new_frame, frame, header_length);

  decompressor_->next_in = const_cast<Bytef*>(payload);
  decompressor_->avail_in = payload_length;
  decompressor_->next_out = reinterpret_cast<Bytef*>(new_frame) +
      header_length;
  decompressor_->avail_out = decompressed_max_size;

  int rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
  if (rv == Z_NEED_DICT) {
    // Need to try again with the right dictionary.
    if (decompressor_->adler == dictionary_id) {
      rv = inflateSetDictionary(decompressor_.get(), (const Bytef*)dictionary,
                                sizeof(dictionary));
      if (rv == Z_OK)
        rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
    }
  }
  if (rv != Z_OK) {  // How can we know that it decompressed everything?
    free(new_frame);
    return NULL;
  }

  // Unset the compressed flag for data frames.
  if (!new_frame->is_control_frame()) {
    FlipDataFrame* data_frame = reinterpret_cast<FlipDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() & ~DATA_FLAG_COMPRESSED);
  }

  int decompressed_size = decompressed_max_size - decompressor_->avail_out;
  new_frame->set_length(header_length + decompressed_size - sizeof(FlipFrame));

  pre_decompress_bytes.Add(frame->length());
  post_decompress_bytes.Add(new_frame->length());

  return new_frame;
}

FlipFrame* FlipFramer::DuplicateFrame(const FlipFrame* frame) {
  int size = sizeof(FlipFrame) + frame->length();
  char* new_frame = new char[size];
  memcpy(new_frame, frame, size);
  return reinterpret_cast<FlipFrame*>(new_frame);
}

void FlipFramer::set_enable_compression(bool value) {
  enable_compression_ = value;
}

void FlipFramer::set_enable_compression_default(bool value) {
  compression_default_ = value;
}

}  // namespace flip

