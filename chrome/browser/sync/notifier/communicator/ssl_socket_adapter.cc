// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/ssl_socket_adapter.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"

namespace notifier {

namespace {

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixError(int err) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return net::ERR_IO_PENDING;
    case ENETDOWN:
      return net::ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return net::ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
      return net::ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return net::ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return net::ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return net::ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return net::ERR_ADDRESS_INVALID;
    case 0:
      return net::OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return net::ERR_FAILED;
  }
}

}  // namespace

SSLSocketAdapter* SSLSocketAdapter::Create(AsyncSocket* socket) {
  return new SSLSocketAdapter(socket);
}

SSLSocketAdapter::SSLSocketAdapter(AsyncSocket* socket)
    : SSLAdapter(socket),
      ignore_bad_cert_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
        connected_callback_(this, &SSLSocketAdapter::OnConnected)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
        io_callback_(this, &SSLSocketAdapter::OnIO)),
      ssl_connected_(false),
      state_(STATE_NONE) {
  transport_socket_ = new TransportSocket(socket, this);
}

int SSLSocketAdapter::StartSSL(const char* hostname, bool restartable) {
  DCHECK(!restartable);
  hostname_ = hostname;

  if (socket_->GetState() != Socket::CS_CONNECTED) {
    state_ = STATE_SSL_WAIT;
    return 0;
  } else {
    return BeginSSL();
  }
}

int SSLSocketAdapter::BeginSSL() {
  if (!MessageLoop::current()) {
    // Certificate verification is done via the Chrome message loop.
    // Without this check, if we don't have a chrome message loop the
    // SSL connection just hangs silently.
    LOG(DFATAL) << "Chrome message loop (needed by SSL certificate "
                << "verification) does not exist";
    return net::ERR_UNEXPECTED;
  }

  // SSLConfigService is not thread-safe, and the default values for SSLConfig
  // are correct for us, so we don't use the config service to initialize this
  // object.
  net::SSLConfig ssl_config;
  transport_socket_->set_addr(talk_base::SocketAddress(hostname_.c_str()));
  ssl_socket_.reset(
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          transport_socket_, hostname_.c_str(), ssl_config));

  int result = ssl_socket_->Connect(&connected_callback_);

  if (result == net::ERR_IO_PENDING || result == net::OK) {
    return 0;
  } else {
    LOG(ERROR) << "Could not start SSL: " << net::ErrorToString(result);
    return result;
  }
}

int SSLSocketAdapter::Send(const void* buf, size_t len) {
  if (!ssl_connected_) {
    return AsyncSocketAdapter::Send(buf, len);
  } else {
    scoped_refptr<net::IOBuffer> transport_buf = new net::IOBuffer(len);
    memcpy(transport_buf->data(), buf, len);

    int result = ssl_socket_->Write(transport_buf, len, NULL);
    if (result == net::ERR_IO_PENDING) {
      SetError(EWOULDBLOCK);
    }
    transport_buf = NULL;
    return result;
  }
}

int SSLSocketAdapter::Recv(void* buf, size_t len) {
  if (!ssl_connected_) {
    return AsyncSocketAdapter::Recv(buf, len);
  }

  switch (state_) {
    case STATE_NONE: {
      transport_buf_ = new net::IOBuffer(len);
      int result = ssl_socket_->Read(transport_buf_, len, &io_callback_);
      if (result >= 0) {
        memcpy(buf, transport_buf_->data(), len);
      }

      if (result == net::ERR_IO_PENDING) {
        state_ = STATE_READ;
        SetError(EWOULDBLOCK);
      } else {
        if (result < 0) {
          SetError(result);
          LOG(INFO) << "Socket error " << result;
        }
        transport_buf_ = NULL;
      }
      return result;
    }
    case STATE_READ_COMPLETE:
      memcpy(buf, transport_buf_->data(), len);
      transport_buf_ = NULL;
      state_ = STATE_NONE;
      return data_transferred_;

    case STATE_READ:
    case STATE_WRITE:
    case STATE_WRITE_COMPLETE:
    case STATE_SSL_WAIT:
      SetError(EWOULDBLOCK);
      return -1;

    default:
      NOTREACHED();
      break;
  }
  return -1;
}

void SSLSocketAdapter::OnConnected(int result) {
  if (result == net::OK) {
    ssl_connected_ = true;
    OnConnectEvent(this);
  } else {
    LOG(WARNING) << "OnConnected failed with error " << result;
  }
}

void SSLSocketAdapter::OnIO(int result) {
  switch (state_) {
    case STATE_READ:
      state_ = STATE_READ_COMPLETE;
      data_transferred_ = result;
      AsyncSocketAdapter::OnReadEvent(this);
      break;
    case STATE_WRITE:
      state_ = STATE_WRITE_COMPLETE;
      data_transferred_ = result;
      AsyncSocketAdapter::OnWriteEvent(this);
      break;
    case STATE_NONE:
    case STATE_READ_COMPLETE:
    case STATE_WRITE_COMPLETE:
    case STATE_SSL_WAIT:
    default:
      NOTREACHED();
      break;
  }
}

void SSLSocketAdapter::OnReadEvent(talk_base::AsyncSocket* socket) {
  if (!transport_socket_->OnReadEvent(socket))
    AsyncSocketAdapter::OnReadEvent(socket);
}

void SSLSocketAdapter::OnWriteEvent(talk_base::AsyncSocket* socket) {
  if (!transport_socket_->OnWriteEvent(socket))
    AsyncSocketAdapter::OnWriteEvent(socket);
}

void SSLSocketAdapter::OnConnectEvent(talk_base::AsyncSocket* socket) {
  if (state_ != STATE_SSL_WAIT) {
    AsyncSocketAdapter::OnConnectEvent(socket);
  } else {
    state_ = STATE_NONE;
    int result = BeginSSL();
    if (0 != result) {
      // TODO(zork): Handle this case gracefully.
      LOG(WARNING) << "BeginSSL() failed with " << result;
    }
  }
}

TransportSocket::TransportSocket(talk_base::AsyncSocket* socket,
                                 SSLSocketAdapter *ssl_adapter)
    : connect_callback_(NULL),
      read_callback_(NULL),
      write_callback_(NULL),
      read_buffer_len_(0),
      write_buffer_len_(0),
      socket_(socket) {
    socket_->SignalConnectEvent.connect(this, &TransportSocket::OnConnectEvent);
}

int TransportSocket::Connect(net::CompletionCallback* callback) {
  connect_callback_ = callback;
  return socket_->Connect(addr_);
}

void TransportSocket::Disconnect() {
  socket_->Close();
}

bool TransportSocket::IsConnected() const {
  return (socket_->GetState() == talk_base::Socket::CS_CONNECTED);
}

bool TransportSocket::IsConnectedAndIdle() const {
  // Not implemented.
  NOTREACHED();
  return false;
}

int TransportSocket::GetPeerAddress(net::AddressList* address) const {
  talk_base::SocketAddress socket_address = socket_->GetRemoteAddress();

  // libjingle supports only IPv4 addresses.
  sockaddr_in ipv4addr;
  socket_address.ToSockAddr(&ipv4addr);

  struct addrinfo ai;
  memset(&ai, 0, sizeof(ai));
  ai.ai_family = ipv4addr.sin_family;
  ai.ai_socktype = SOCK_STREAM;
  ai.ai_protocol = IPPROTO_TCP;
  ai.ai_addr = reinterpret_cast<struct sockaddr*>(&ipv4addr);
  ai.ai_addrlen = sizeof(ipv4addr);

  address->Copy(&ai, false);
  return net::OK;
}

int TransportSocket::Read(net::IOBuffer* buf, int buf_len,
                          net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!read_callback_);
  DCHECK(!read_buffer_.get());
  int result = socket_->Recv(buf->data(), buf_len);
  if (result < 0) {
    result = MapPosixError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_len_ = buf_len;
    }
  }
  return result;
}

int TransportSocket::Write(net::IOBuffer* buf, int buf_len,
                           net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!write_callback_);
  DCHECK(!write_buffer_.get());
  int result = socket_->Send(buf->data(), buf_len);
  if (result < 0) {
    result = MapPosixError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      write_callback_ = callback;
      write_buffer_ = buf;
      write_buffer_len_ = buf_len;
    }
  }
  return result;
}

bool TransportSocket::SetReceiveBufferSize(int32 size) {
  // Not implemented.
  return false;
}

bool TransportSocket::SetSendBufferSize(int32 size) {
  // Not implemented.
  return false;
}

void TransportSocket::OnConnectEvent(talk_base::AsyncSocket * socket) {
  if (connect_callback_) {
    net::CompletionCallback *callback = connect_callback_;
    connect_callback_ = NULL;
    callback->RunWithParams(Tuple1<int>(MapPosixError(socket_->GetError())));
  } else {
    LOG(WARNING) << "OnConnectEvent called with no callback.";
  }
}

bool TransportSocket::OnReadEvent(talk_base::AsyncSocket* socket) {
  if (read_callback_) {
    DCHECK(read_buffer_.get());
    net::CompletionCallback* callback = read_callback_;
    scoped_refptr<net::IOBuffer> buffer = read_buffer_;
    int buffer_len = read_buffer_len_;

    read_callback_ = NULL;
    read_buffer_ = NULL;
    read_buffer_len_ = 0;

    int result = socket_->Recv(buffer->data(), buffer_len);
    if (result < 0) {
      result = MapPosixError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        read_callback_ = callback;
        read_buffer_ = buffer;
        read_buffer_len_ = buffer_len;
        return true;
      }
    }
    callback->RunWithParams(Tuple1<int>(result));
    return true;
  } else {
    LOG(WARNING) << "OnReadEvent called with no callback.";
    return false;
  }
}

bool TransportSocket::OnWriteEvent(talk_base::AsyncSocket* socket) {
  if (write_callback_) {
    DCHECK(write_buffer_.get());
    net::CompletionCallback* callback = write_callback_;
    scoped_refptr<net::IOBuffer> buffer = write_buffer_;
    int buffer_len = write_buffer_len_;

    write_callback_ = NULL;
    write_buffer_ = NULL;
    write_buffer_len_ = 0;

    int result = socket_->Send(buffer->data(), buffer_len);
    if (result < 0) {
      result = MapPosixError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        write_callback_ = callback;
        write_buffer_ = buffer;
        write_buffer_len_ = buffer_len;
        return true;
      }
    }
    callback->RunWithParams(Tuple1<int>(result));
    return true;
  } else {
    LOG(WARNING) << "OnWriteEvent called with no callback.";
    return false;
  }
}

}  // namespace notifier
