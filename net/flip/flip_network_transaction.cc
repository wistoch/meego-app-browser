// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_network_transaction.h"

#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"

using base::Time;

namespace net {

//-----------------------------------------------------------------------------

FlipStreamParser::FlipStreamParser()
    : flip_(NULL),
      flip_stream_id_(0),
      request_(NULL),
      response_(NULL),
      request_body_stream_(NULL),
      response_complete_(false),
      io_state_(STATE_NONE),
      response_status_(OK),
      user_callback_(NULL),
      user_buffer_(NULL),
      user_buffer_len_(0),
      cancelled_(false) {
}

FlipStreamParser::~FlipStreamParser() {
  if (flip_ && flip_stream_id_) {
    flip_->CancelStream(flip_stream_id_);
  } else if (!response_complete_) {
    NOTREACHED();
  }
}

int FlipStreamParser::SendRequest(FlipSession* flip,
                                  const HttpRequestInfo* request,
                                  CompletionCallback* callback) {
  CHECK(flip);
  CHECK(request);
  CHECK(callback);
  DCHECK(!cancelled_);

  request_ = request;
  flip_ = flip;

  DCHECK(io_state_ == STATE_NONE);
  io_state_ = STATE_SENDING_HEADERS;
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

int FlipStreamParser::ReadResponseHeaders(CompletionCallback* callback) {
  // Note: The FlipStream may have already received the response headers, so
  //       this call may complete synchronously.
  DCHECK_GT(io_state_, STATE_HEADERS_SENT);
  DCHECK(!cancelled_);
  CHECK(callback);

  // The SYN_REPLY has already been received.
  if (response_.get())
    return OK;

  io_state_ = STATE_READ_HEADERS;
  CHECK(!user_callback_);
  user_callback_ = callback;
  return ERR_IO_PENDING;
}

int FlipStreamParser::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(io_state_ == STATE_BODY_PENDING ||
         io_state_ == STATE_READ_BODY ||
         io_state_ == STATE_DONE);
  DCHECK(!cancelled_);
  CHECK(buf);
  CHECK(buf_len);
  CHECK(callback);

  io_state_ = STATE_READ_BODY;

  // If we have data buffered, complete the IO immediately.
  if (response_body_.size()) {
    int bytes_read = 0;
    while (response_body_.size() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      const int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        const int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(new_buffer);
      }
      bytes_read += bytes_to_copy;
    }
    return bytes_read;
  } else if (response_complete_) {
    return response_status_;
  }

  CHECK(!user_callback_);
  CHECK(!user_buffer_);
  CHECK(user_buffer_len_ == 0);

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

uint64 FlipStreamParser::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}

const HttpResponseInfo* FlipStreamParser::GetResponseInfo() const {
  return response_.get();
}

void FlipStreamParser::Cancel() {
  cancelled_ = true;
  user_callback_ = NULL;
}

const HttpRequestInfo* FlipStreamParser::request() const {
  return request_;
}

const UploadDataStream* FlipStreamParser::data() const {
  return request_body_stream_.get();
}

void FlipStreamParser::OnWriteComplete(int status) {
  if (io_state_ == STATE_SENDING_HEADERS)
    io_state_ = STATE_HEADERS_SENT;

  DoLoop(status);
}

void FlipStreamParser::OnResponseReceived(HttpResponseInfo* response) {
  response_.reset(new HttpResponseInfo);
  *response_ = *response;  // TODO(mbelshe): avoid copy.

  DCHECK_GE(io_state_, STATE_HEADERS_SENT);
  io_state_ = STATE_BODY_PENDING;

  if (user_callback_)
    DoCallback(OK);
}

void FlipStreamParser::OnDataReceived(const char* buffer, int bytes) {
  // TODO(mbelshe): if data is received before a syn reply, this will crash.

  DCHECK_GE(bytes, 0);
  if (bytes > 0) {
    DCHECK(buffer);

    // TODO(mbelshe): If read is pending, we should copy the data straight into
    // the read buffer here.  For now, we'll queue it always.

    IOBufferWithSize* io_buffer = new IOBufferWithSize(bytes);
    memcpy(io_buffer->data(), buffer, bytes);

    response_body_.push_back(io_buffer);
  }

  // Note that data may be received for a FlipStream prior to the user calling
  // ReadResponseBody(), therefore user_callback_ may be NULL.
  if (user_callback_) {
    int rv = ReadResponseBody(user_buffer_, user_buffer_len_, user_callback_);
    CHECK(rv != ERR_IO_PENDING);
    user_buffer_ = NULL;
    user_buffer_len_ = 0;
    DoCallback(rv);
  }
}

void FlipStreamParser::OnClose(int status) {
  response_complete_ = true;
  response_status_ = status;
  flip_stream_id_ = 0;

  if (user_callback_)
    DoCallback(status);
}

void FlipStreamParser::DoCallback(int rv) {
  CHECK(rv != ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

int FlipStreamParser::DoSendHeaders(int result) {
  // TODO(mbelshe): rethink this UploadDataStream wrapper.
  if (request_->upload_data)
    request_body_stream_.reset(new UploadDataStream(request_->upload_data));

  CHECK(flip_stream_id_ == 0);
  flip_stream_id_ = flip_->CreateStream(this);

  // The FlipSession will always call us back when the send is complete.
  return ERR_IO_PENDING;
}

// DoSendBody is called to send the optional body for the request.  This call
// will also be called as each write of a chunk of the body completes.
int FlipStreamParser::DoSendBody(int result) {
  // There is no body, move to the next state.
  if (!request_body_stream_.get()) {
    io_state_ = STATE_REQUEST_SENT;
    return result;
  }

  DCHECK(result != 0);  // This should not happen.
  if (result <= 0)
    return result;

  // If we're already in the STATE_SENDING_BODY state, then we've already
  // sent a portion of the body.  In that case, we need to first consume
  // the bytes written in the body stream.  Note that the bytes written is
  // the number of bytes in the frame that were written, only consume the
  // data portion, of course.
  if (io_state_ == STATE_SENDING_BODY)
    request_body_stream_->DidConsume(result);
  else
    io_state_ = STATE_SENDING_BODY;

  if (request_body_stream_->position() < request_body_stream_->size()) {
    int buf_len = static_cast<int>(request_body_stream_->buf_len());
    int rv = flip_->WriteStreamData(flip_stream_id_,
                                    request_body_stream_->buf(),
                                    buf_len);
    return rv;
  }

  io_state_ = STATE_REQUEST_SENT;
  return result;
}

int FlipStreamParser::DoReadHeaders() {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  return ERR_IO_PENDING;
}

int FlipStreamParser::DoReadHeadersComplete(int result) {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  io_state_ = STATE_BODY_PENDING;
  return ERR_IO_PENDING;
}

int FlipStreamParser::DoReadBody() {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  return ERR_IO_PENDING;
}

int FlipStreamParser::DoReadBodyComplete(int result) {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  return ERR_IO_PENDING;
}

int FlipStreamParser::DoLoop(int result) {
  bool can_do_more = true;
  if (cancelled_)
    return ERR_ABORTED;

  do {
    switch (io_state_) {
      case STATE_SENDING_HEADERS:
        result = DoSendHeaders(result);
        break;
      case STATE_HEADERS_SENT:
      case STATE_SENDING_BODY:
        if (result < 0)
          can_do_more = false;
        else
          result = DoSendBody(result);
        break;
      case STATE_REQUEST_SENT:
        DCHECK(result != ERR_IO_PENDING);
        can_do_more = false;
        break;
      case STATE_READ_HEADERS:
        result = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        result = DoReadHeadersComplete(result);
        break;
      case STATE_BODY_PENDING:
        DCHECK(result != ERR_IO_PENDING);
        can_do_more = false;
        break;
      case STATE_READ_BODY:
        result = DoReadBody();
        // DoReadBodyComplete handles error conditions.
        break;
      case STATE_READ_BODY_COMPLETE:
        result = DoReadBodyComplete(result);
        break;
      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        can_do_more = false;
        break;
      default:
        NOTREACHED();
        can_do_more = false;
        break;
    }
  } while (result != ERR_IO_PENDING && can_do_more);

  return result;
}

//-----------------------------------------------------------------------------

FlipNetworkTransaction::FlipNetworkTransaction(HttpNetworkSession* session)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        io_callback_(this, &FlipNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      user_buffer_len_(0),
      session_(session),
      request_(NULL),
      next_state_(STATE_NONE) {
}

FlipNetworkTransaction::~FlipNetworkTransaction() {
  LOG(INFO) << "FlipNetworkTransaction dead. " << this;
  if (flip_stream_parser_.get())
    flip_stream_parser_->Cancel();
}

int FlipNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  LoadLog* load_log) {
  CHECK(request_info);
  CHECK(callback);

  request_ = request_info;
  start_time_ = base::TimeTicks::Now();

  next_state_ = STATE_INIT_CONNECTION;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int FlipNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int FlipNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert, CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int FlipNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return 0;
}

int FlipNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(callback);
  DCHECK(flip_.get());

  user_buffer_ = buf;
  user_buffer_len_ = buf_len;

  next_state_ = STATE_READ_BODY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* FlipNetworkTransaction::GetResponseInfo() const {
  const HttpResponseInfo* response = flip_stream_parser_->GetResponseInfo();
  return (response->headers || response->ssl_info.cert) ? response : NULL;
}

LoadState FlipNetworkTransaction::GetLoadState() const {
  switch (next_state_) {
    case STATE_INIT_CONNECTION_COMPLETE:
      if (flip_.get())
        return flip_->GetLoadState();
      return LOAD_STATE_CONNECTING;
    case STATE_SEND_REQUEST_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

uint64 FlipNetworkTransaction::GetUploadProgress() const {
  if (!flip_stream_parser_.get())
    return 0;

  return flip_stream_parser_->GetUploadProgress();
}

void FlipNetworkTransaction::DoCallback(int rv) {
  CHECK(rv != ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void FlipNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int FlipNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);
  DCHECK(request_);

  if (!request_)
    return 0;

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        break;
      case STATE_NONE:
        rv = ERR_FAILED;
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int FlipNetworkTransaction::DoInitConnection() {
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  std::string host = request_->url.HostNoBrackets();
  int port = request_->url.EffectiveIntPort();

  std::string connection_group = "flip.";
  connection_group.append(host);

  HostResolver::RequestInfo resolve_info(host, port);

  flip_ = session_->flip_session_pool()->Get(resolve_info, session_);
  DCHECK(flip_);

  int rv = flip_->Connect(connection_group, resolve_info, request_->priority);
  DCHECK(rv == OK);  // The API says it will always return OK.
  return OK;
}

int FlipNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_SEND_REQUEST;
  return OK;
}

int FlipNetworkTransaction::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  CHECK(!flip_stream_parser_.get());
  flip_stream_parser_ = new FlipStreamParser;
  return flip_stream_parser_->SendRequest(flip_, request_, &io_callback_);
}

int FlipNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int FlipNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return flip_stream_parser_->ReadResponseHeaders(&io_callback_);
}

int FlipNetworkTransaction::DoReadHeadersComplete(int result) {
  // TODO(willchan): Flesh out the support for HTTP authentication here.
  return OK;
}

int FlipNetworkTransaction::DoReadBody() {
  next_state_ = STATE_READ_BODY_COMPLETE;

  return flip_stream_parser_->ReadResponseBody(
      user_buffer_, user_buffer_len_, &io_callback_);
}

int FlipNetworkTransaction::DoReadBodyComplete(int result) {
  user_buffer_ = NULL;
  user_buffer_len_ = 0;

  if (result <= 0)
    flip_stream_parser_.release();

  return result;
}

}  // namespace net
