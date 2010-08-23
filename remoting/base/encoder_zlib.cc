// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_zlib.h"

#include "gfx/rect.h"
#include "media/base/data_buffer.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/compressor_zlib.h"
#include "remoting/base/protocol_util.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

static const int kPacketSize = 1024 * 1024;

EncoderZlib::EncoderZlib() : packet_size_(kPacketSize) {
}

EncoderZlib::EncoderZlib(int packet_size) : packet_size_(packet_size) {
}

void EncoderZlib::Encode(scoped_refptr<CaptureData> capture_data,
                         bool key_frame,
                         DataAvailableCallback* data_available_callback) {
  CHECK(capture_data->pixel_format() == PixelFormatRgb32)
      << "Zlib Encoder only works with RGB32";
  capture_data_ = capture_data;
  callback_.reset(data_available_callback);

  CompressorZlib compressor;
  const InvalidRects& rects = capture_data->dirty_rects();
  int index = 0;
  for (InvalidRects::const_iterator r = rects.begin();
      r != rects.end(); ++r, ++index) {
    EncodeRect(&compressor, *r, index);
  }

  capture_data_ = NULL;
  callback_.reset();
}

void EncoderZlib::EncodeRect(CompressorZlib* compressor,
                             const gfx::Rect& rect, size_t rect_index) {
  CHECK(capture_data_->data_planes().data[0]);
  const int strides = capture_data_->data_planes().strides[0];
  const int bytes_per_pixel = GetBytesPerPixel(capture_data_->pixel_format());
  const int row_size = bytes_per_pixel * rect.width();

  ChromotingHostMessage* message = PrepareMessage(&rect);
  const uint8 * in = capture_data_->data_planes().data[0] +
                     rect.y() * strides +
                     rect.x() * bytes_per_pixel;
  // TODO(hclam): Fill in the sequence number.
  uint8* out = (uint8*)message->mutable_update_stream_packet()->
      mutable_rect_data()->mutable_data()->data();
  int filled = 0;
  int row_x = 0;
  int row_y = 0;
  bool compress_again = true;
  while (compress_again) {
    // Prepare a message for sending out.
    if (!message) {
      message = PrepareMessage(NULL);
      out = (uint8*)(message->mutable_update_stream_packet()->
                     mutable_rect_data()->mutable_data()->data());
      filled = 0;
    }

    Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;
    if (row_y == rect.height() - 1) {
      if (rect_index == capture_data_->dirty_rects().size() - 1) {
        flush = Compressor::CompressorFinish;
      } else {
        flush = Compressor::CompressorSyncFlush;
      }
    }

    int consumed = 0;
    int written = 0;
    compress_again = compressor->Process(in + row_x, row_size - row_x,
                                         out + filled, packet_size_ - filled,
                                         flush, &consumed, &written);
    row_x += consumed;
    filled += written;

    // We have reached the end of stream.
    if (!compress_again) {
      message->mutable_update_stream_packet()->mutable_end_rect();
    }

    // If we have filled the message or we have reached the end of stream.
    if (filled == packet_size_ || !compress_again) {
      message->mutable_update_stream_packet()->mutable_rect_data()->
          mutable_data()->resize(filled);
      SubmitMessage(message, rect_index);
      message = NULL;
    }

    // Reached the end of input row and we're not at the last row.
    if (row_x == row_size && row_y < rect.height() - 1) {
      row_x = 0;
      in += strides;
      ++row_y;
    }
  }
}

ChromotingHostMessage* EncoderZlib::PrepareMessage(const gfx::Rect* rect) {
  ChromotingHostMessage* message = new ChromotingHostMessage();
  UpdateStreamPacketMessage* packet = message->mutable_update_stream_packet();

  // Prepare the begin rect content.
  if (rect != NULL) {
    packet->mutable_begin_rect()->set_x(rect->x());
    packet->mutable_begin_rect()->set_y(rect->y());
    packet->mutable_begin_rect()->set_width(rect->width());
    packet->mutable_begin_rect()->set_height(rect->height());
    packet->mutable_begin_rect()->set_encoding(EncodingZlib);
    packet->mutable_begin_rect()->set_pixel_format(
        capture_data_->pixel_format());
  }

  packet->mutable_rect_data()->mutable_data()->resize(packet_size_);
  return message;
}

void EncoderZlib::SubmitMessage(ChromotingHostMessage* message,
                                size_t rect_index) {
  EncodingState state = EncodingInProgress;
  if (rect_index == 0 && message->update_stream_packet().has_begin_rect())
    state |= EncodingStarting;
  if (rect_index == capture_data_->dirty_rects().size() - 1 &&
      message->update_stream_packet().has_end_rect())
    state |= EncodingEnded;
  callback_->Run(message, state);
}

}  // namespace remoting
