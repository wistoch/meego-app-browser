// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/encoder.h"

namespace remoting {

// By default we capture 20 times a second. This number is obtained by
// experiment to provide good latency.
static const double kDefaultCaptureRate = 20.0;

// Interval that we perform rate regulation.
static const base::TimeDelta kRateControlInterval =
    base::TimeDelta::FromSeconds(1);

// We divide the pending update stream number by this value to determine the
// rate divider.
static const int kSlowDownFactor = 10;

// A list of dividers used to divide the max rate to determine the current
// capture rate.
static const int kRateDividers[] = {1, 2, 4, 8, 16};

SessionManager::SessionManager(
    MessageLoop* capture_loop,
    MessageLoop* encode_loop,
    MessageLoop* network_loop,
    Capturer* capturer,
    Encoder* encoder)
    : capture_loop_(capture_loop),
      encode_loop_(encode_loop),
      network_loop_(network_loop),
      capturer_(capturer),
      encoder_(encoder),
      rate_(kDefaultCaptureRate),
      max_rate_(kDefaultCaptureRate),
      started_(false),
      recordings_(0),
      rate_control_started_(false),
      capture_width_(0),
      capture_height_(0),
      capture_pixel_format_(PixelFormatInvalid),
      encode_stream_started_(false),
      encode_done_(false) {
  DCHECK(capture_loop_);
  DCHECK(encode_loop_);
  DCHECK(network_loop_);
}

SessionManager::~SessionManager() {
  clients_.clear();
}

void SessionManager::Start() {
  capture_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &SessionManager::DoStart));
}

void SessionManager::DoStart() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (started_) {
    NOTREACHED() << "Record session already started";
    return;
  }

  started_ = true;
  DoCapture();

  // Starts the rate regulation.
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoStartRateControl));
}

void SessionManager::DoStartRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  if (rate_control_started_) {
    NOTREACHED() << "Rate regulation already started";
    return;
  }
  rate_control_started_ = true;
  ScheduleNextRateControl();
}

void SessionManager::Pause() {
  capture_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &SessionManager::DoPause));
}

void SessionManager::DoPause() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!started_) {
    NOTREACHED() << "Record session not started";
    return;
  }

  started_ = false;

  // Pause the rate regulation.
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoPauseRateControl));
}

void SessionManager::DoPauseRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  if (!rate_control_started_) {
    NOTREACHED() << "Rate regulation not started";
    return;
  }
  rate_control_started_ = false;
}

void SessionManager::SetMaxRate(double rate) {
  capture_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &SessionManager::DoSetMaxRate, rate));
}

void SessionManager::AddClient(scoped_refptr<ClientConnection> client) {
  // Gets the init information for the client.
  capture_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoGetInitInfo, client));
}

void SessionManager::RemoveClient(scoped_refptr<ClientConnection> client) {
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoRemoveClient, client));
}

void SessionManager::DoCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (recordings_ >= 2 || !started_)
    return;

  base::Time now = base::Time::Now();
  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond / rate_));
  base::TimeDelta elapsed = now - last_capture_time_;

  // If this method is called sonner than the required interval we return
  // immediately
  if (elapsed < interval)
    return;

  // At this point we are going to perform one capture so save the current time.
  last_capture_time_ = now;
  ++recordings_;

  // Before we actually do a capture, schedule the next one.
  ScheduleNextCapture();

  // And finally perform one capture.
  DCHECK(capturer_.get());
  capturer_->CaptureDirtyRects(
      NewRunnableMethod(this, &SessionManager::CaptureDoneTask));
}

void SessionManager::DoFinishEncode() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Decrement the number of recording in process since we have completed
  // one cycle.
  --recordings_;

  // Try to do a capture again. Note that the following method may do nothing
  // if it is too early to perform a capture.
  if (rate_ > 0)
    DoCapture();
}

void SessionManager::DoEncode() {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  // Reset states about the encode stream.
  encode_done_ = false;
  encode_stream_started_ = false;

  DCHECK(!encoded_data_.get());
  DCHECK(encoder_.get());

  // TODO(hclam): Enable |force_refresh| if a new client was
  // added.
  encoder_->SetSize(capture_width_, capture_height_);
  encoder_->SetPixelFormat(capture_pixel_format_);
  encoder_->Encode(
      capture_dirty_rects_,
      capture_data_,
      capture_data_strides_,
      false,
      &encoded_data_header_,
      &encoded_data_,
      &encode_done_,
      NewRunnableMethod(this, &SessionManager::EncodeDataAvailableTask));
}

void SessionManager::DoSendUpdate(
    UpdateStreamPacketHeader* header,
    scoped_refptr<media::DataBuffer> encoded_data,
    bool begin_update, bool end_update) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  for (size_t i = 0; i < clients_.size(); ++i) {
    if (begin_update)
      clients_[i]->SendBeginUpdateStreamMessage();

    // This will pass the ownership of the DataBuffer to the ClientConnection.
    clients_[i]->SendUpdateStreamPacketMessage(header, encoded_data);

    if (end_update)
      clients_[i]->SendEndUpdateStreamMessage();
  }
}

void SessionManager::DoSendInit(scoped_refptr<ClientConnection> client,
                                int width, int height) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // Sends the client init information.
  client->SendInitClientMessage(width, height);
}

void SessionManager::DoGetInitInfo(scoped_refptr<ClientConnection> client) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Sends the init message to the cleint.
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoSendInit, client,
                        capturer_->GetWidth(), capturer_->GetHeight()));

  // And then add the client to the list so it can receive update stream.
  // It is important we do so in such order or the client will receive
  // update stream before init message.
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoAddClient, client));
}

void SessionManager::DoSetRate(double rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  if (rate == rate_)
    return;

  // Change the current capture rate.
  rate_ = rate;

  // If we have already started then schedule the next capture with the new
  // rate.
  if (started_)
    ScheduleNextCapture();
}

void SessionManager::DoSetMaxRate(double max_rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // TODO(hclam): Should also check for small epsilon.
  if (max_rate != 0) {
    max_rate_ = max_rate;
    DoSetRate(max_rate);
  } else {
    NOTREACHED() << "Rate is too small.";
  }
}

void SessionManager::DoAddClient(scoped_refptr<ClientConnection> client) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Force a full frame for next encode.
  clients_.push_back(client);
}

void SessionManager::DoRemoveClient(scoped_refptr<ClientConnection> client) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Is it correct to do to a scoped_refptr?
  ClientConnectionList::iterator it
      = std::find(clients_.begin(), clients_.end(), client);
  if (it != clients_.end())
    clients_.erase(it);
}

void SessionManager::DoRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // If we have been paused then shutdown the rate regulation loop.
  if (!rate_control_started_)
    return;

  int max_pending_update_streams = 0;
  for (size_t i = 0; i < clients_.size(); ++i) {
    max_pending_update_streams =
        std::max(max_pending_update_streams,
                 clients_[i]->GetPendingUpdateStreamMessages());
  }

  // If |slow_down| equals zero, we have no slow down.
  int slow_down = max_pending_update_streams / kSlowDownFactor;
  // Set new_rate to -1 for checking later.
  double new_rate = -1;
  // If the slow down is too large.
  if (slow_down >= arraysize(kRateDividers)) {
    // Then we stop the capture completely.
    new_rate = 0;
  } else {
    // Slow down the capture rate using the divider.
    new_rate = max_rate_ / kRateDividers[slow_down];
  }
  DCHECK_NE(new_rate, -1.0);

  // Then set the rate.
  capture_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoSetRate, new_rate));
  ScheduleNextRateControl();
}

void SessionManager::ScheduleNextCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (rate_ == 0)
    return;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond / rate_));
  capture_loop_->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoCapture),
      interval.InMilliseconds());
}

void SessionManager::ScheduleNextRateControl() {
  network_loop_->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &SessionManager::DoRateControl),
      kRateControlInterval.InMilliseconds());
}

void SessionManager::CaptureDoneTask() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Save results of the capture.
  capturer_->GetData(capture_data_);
  capturer_->GetDataStride(capture_data_strides_);
  capture_dirty_rects_.clear();
  capturer_->GetDirtyRects(&capture_dirty_rects_);
  capture_pixel_format_ = capturer_->GetPixelFormat();
  capture_width_ = capturer_->GetWidth();
  capture_height_ = capturer_->GetHeight();

  encode_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &SessionManager::DoEncode));
}

void SessionManager::EncodeDataAvailableTask() {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  // Before a new encode task starts, notify clients a new update
  // stream is coming.
  // Notify this will keep a reference to the DataBuffer in the
  // task. The ownership will eventually pass to the ClientConnections.
  network_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &SessionManager::DoSendUpdate,
                        &encoded_data_header_,
                        encoded_data_,
                        !encode_stream_started_,
                        encode_done_));

  // Since we have received data from the Encoder, mark the encode
  // stream has started.
  encode_stream_started_ = true;

  // Give up the ownership of DataBuffer since it is passed to
  // the ClientConnections.
  encoded_data_ = NULL;

  if (encode_done_) {
    capture_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &SessionManager::DoFinishEncode));
  }
}

}  // namespace remoting
