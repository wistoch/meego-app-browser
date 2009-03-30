// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_client_socket.h"

#include "base/memory_debug.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

//-----------------------------------------------------------------------------

static int MapWinsockError(DWORD err) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case WSAENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case WSAETIMEDOUT:
      return ERR_TIMED_OUT;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSAEDISCON:
      // Returned by WSARecv or WSARecvFrom for message-oriented sockets (where
      // a return value of zero means a zero-byte message) to indicate graceful
      // connection shutdown.  We should not ever see this error code for TCP
      // sockets, which are byte stream oriented.
      NOTREACHED();
      return ERR_CONNECTION_CLOSED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case WSA_IO_INCOMPLETE:
      return ERR_UNEXPECTED;
    case ERROR_SUCCESS:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

//-----------------------------------------------------------------------------

TCPClientSocket::TCPClientSocket(const AddressList& addresses)
    : socket_(INVALID_SOCKET),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      wait_state_(NOT_WAITING),
      callback_(NULL) {
  memset(&overlapped_, 0, sizeof(overlapped_));
  EnsureWinsockInit();
}

TCPClientSocket::~TCPClientSocket() {
  Disconnect();
}

int TCPClientSocket::Connect(CompletionCallback* callback) {
  // If already connected, then just return OK.
  if (socket_ != INVALID_SOCKET)
    return OK;

  TRACE_EVENT_BEGIN("socket.connect", this, "");
  const struct addrinfo* ai = current_ai_;
  DCHECK(ai);

  int rv = CreateSocket(ai);
  if (rv != OK)
    return rv;

  // WSACreateEvent creates a manual-reset event object.
  overlapped_.hEvent = WSACreateEvent();
  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, overlapped_.hEvent, FD_CONNECT);

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    WaitForAndResetEvent();
    TRACE_EVENT_END("socket.connect", this, "");
    return OK;
  }

  DWORD err = WSAGetLastError();
  if (err != WSAEWOULDBLOCK) {
    LOG(ERROR) << "connect failed: " << err;
    return MapWinsockError(err);
  }

  watcher_.StartWatching(overlapped_.hEvent, this);
  wait_state_ = WAITING_CONNECT;
  callback_ = callback;
  return ERR_IO_PENDING;
}

void TCPClientSocket::Disconnect() {
  if (socket_ == INVALID_SOCKET)
    return;

  TRACE_EVENT_INSTANT("socket.disconnect", this, "");

  // Make sure the message loop is not watching this object anymore.
  watcher_.StopWatching();

  // Cancel any pending IO and wait for it to be aborted.
  if (wait_state_ == WAITING_READ || wait_state_ == WAITING_WRITE) {
    CancelIo(reinterpret_cast<HANDLE>(socket_));
    WaitForSingleObject(overlapped_.hEvent, INFINITE);
    wait_state_ = NOT_WAITING;
  }

  // In most socket implementations, closing a socket results in a graceful
  // connection shutdown, but in Winsock we have to call shutdown explicitly.
  // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
  // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
  shutdown(socket_, SD_SEND);

  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  WSACloseEvent(overlapped_.hEvent);
  memset(&overlapped_, 0, sizeof(overlapped_));

  // Reset for next time.
  current_ai_ = addresses_.head();
}

bool TCPClientSocket::IsConnected() const {
  if (socket_ == INVALID_SOCKET || wait_state_ == WAITING_CONNECT)
    return false;

  // Check if connection is alive.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv == 0)
    return false;
  if (rv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

bool TCPClientSocket::IsConnectedAndIdle() const {
  if (socket_ == INVALID_SOCKET || wait_state_ == WAITING_CONNECT)
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv >= 0)
    return false;
  if (WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocket::Read(char* buf,
                          int buf_len,
                          CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = buf;

  TRACE_EVENT_BEGIN("socket.read", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num, flags = 0;
  int rv = WSARecv(socket_, &buffer_, 1, &num, &flags, &overlapped_, NULL);
  if (rv == 0) {
    WaitForAndResetEvent();
    TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", num));

    // Because of how WSARecv fills memory when used asynchronously, Purify
    // isn't able to detect that it's been initialized, so it scans for 0xcd
    // in the buffer and reports UMRs (uninitialized memory reads) for those
    // individual bytes. We override that in PURIFY builds to avoid the false
    // error reports.
    // See bug 5297.
    base::MemoryDebug::MarkAsInitialized(buffer_.buf, num);
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_READ;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocket::Write(const char* buf,
                           int buf_len,
                           CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = const_cast<char*>(buf);

  TRACE_EVENT_BEGIN("socket.write", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num;
  int rv = WSASend(socket_, &buffer_, 1, &num, 0, &overlapped_, NULL);
  if (rv == 0) {
    WaitForAndResetEvent();
    TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", num));
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_WRITE;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocket::CreateSocket(const struct addrinfo* ai) {
  socket_ = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET) {
    DWORD err = WSAGetLastError();
    LOG(ERROR) << "WSASocket failed: " << err;
    return MapWinsockError(err);
  }

  // Increase the socket buffer sizes from the default sizes for WinXP.  In
  // performance testing, there is substantial benefit by increasing from 8KB
  // to 64KB.
  // See also:
  //    http://support.microsoft.com/kb/823764/EN-US
  // On Vista, if we manually set these sizes, Vista turns off its receive
  // window auto-tuning feature.
  //    http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
  // Since Vista's auto-tune is better than any static value we can could set,
  // only change these on pre-vista machines.
  int32 major_version, minor_version, fix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
    &fix_version);
  if (major_version < 6) {
    const int kSocketBufferSize = 64 * 1024;
    int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
        reinterpret_cast<const char*>(&kSocketBufferSize),
        sizeof(kSocketBufferSize));
    DCHECK(!rv) << "Could not set socket send buffer size";
    rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<const char*>(&kSocketBufferSize),
        sizeof(kSocketBufferSize));
    DCHECK(!rv) << "Could not set socket receive buffer size";
  }

  // Disable Nagle.
  // The Nagle implementation on windows is governed by RFC 896.  The idea
  // behind Nagle is to reduce small packets on the network.  When Nagle is
  // enabled, if a partial packet has been sent, the TCP stack will disallow
  // further *partial* packets until an ACK has been received from the other
  // side.  Good applications should always strive to send as much data as
  // possible and avoid partial-packet sends.  However, in most real world
  // applications, there are edge cases where this does not happen, and two
  // partil packets may be sent back to back.  For a browser, it is NEVER
  // a benefit to delay for an RTT before the second packet is sent.
  //
  // As a practical example in Chromium today, consider the case of a small
  // POST.  I have verified this:
  //     Client writes 649 bytes of header  (partial packet #1)
  //     Client writes 50 bytes of POST data (partial packet #2)
  // In the above example, with Nagle, a RTT delay is inserted between these
  // two sends due to nagle.  RTTs can easily be 100ms or more.  The best
  // fix is to make sure that for POSTing data, we write as much data as
  // possible and minimize partial packets.  We will fix that.  But disabling
  // Nagle also ensure we don't run into this delay in other edge cases.
  // See also:
  //    http://technet.microsoft.com/en-us/library/bb726981.aspx
  const BOOL kDisableNagle = TRUE;
  int rv = setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
      reinterpret_cast<const char*>(&kDisableNagle), sizeof(kDisableNagle));
  DCHECK(!rv) << "Could not disable nagle";

  return OK;
}

void TCPClientSocket::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocket::DidCompleteConnect() {
  int result;

  TRACE_EVENT_END("socket.connect", this, "");
  wait_state_ = NOT_WAITING;

  WSANETWORKEVENTS events;
  int rv = WSAEnumNetworkEvents(socket_, overlapped_.hEvent, &events);
  if (rv == SOCKET_ERROR) {
    NOTREACHED();
    result = MapWinsockError(WSAGetLastError());
  } else if (events.lNetworkEvents & FD_CONNECT) {
    wait_state_ = NOT_WAITING;
    DWORD error_code = static_cast<DWORD>(events.iErrorCode[FD_CONNECT_BIT]);
    if (current_ai_->ai_next && (
        error_code == WSAEADDRNOTAVAIL ||
        error_code == WSAEAFNOSUPPORT ||
        error_code == WSAECONNREFUSED ||
        error_code == WSAENETUNREACH ||
        error_code == WSAEHOSTUNREACH ||
        error_code == WSAETIMEDOUT)) {
      // Try using the next address.
      const struct addrinfo* next = current_ai_->ai_next;
      Disconnect();
      current_ai_ = next;
      result = Connect(callback_);
    } else {
      result = MapWinsockError(error_code);
    }
  } else {
    NOTREACHED();
    result = ERR_UNEXPECTED;
  }

  if (result != ERR_IO_PENDING)
    DoCallback(result);
}

void TCPClientSocket::DidCompleteIO() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(
      socket_, &overlapped_, &num_bytes, FALSE, &flags);
  WSAResetEvent(overlapped_.hEvent);
  if (wait_state_ == WAITING_READ) {
    TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", num_bytes));
  } else {
    TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", num_bytes));
  }
  wait_state_ = NOT_WAITING;
  DoCallback(ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

void TCPClientSocket::OnObjectSignaled(HANDLE object) {
  DCHECK(object == overlapped_.hEvent);

  switch (wait_state_) {
    case WAITING_CONNECT:
      DidCompleteConnect();
      break;
    case WAITING_READ:
    case WAITING_WRITE:
      DidCompleteIO();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void TCPClientSocket::WaitForAndResetEvent() {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(overlapped_.hEvent, INFINITE);
  CHECK(wait_rv == WAIT_OBJECT_0);
  BOOL ok = WSAResetEvent(overlapped_.hEvent);
  CHECK(ok);
}

}  // namespace net
