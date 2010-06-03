/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_ASYNCSOCKET_H__
#define TALK_BASE_ASYNCSOCKET_H__

#include "talk/base/sigslot.h"
#include "talk/base/socket.h"

namespace talk_base {

// Provides the ability to perform socket I/O asynchronously.
class AsyncSocket : public Socket, public sigslot::has_slots<>  {
public:
  virtual ~AsyncSocket() {}

  sigslot::signal1<AsyncSocket*> SignalReadEvent;  // ready to read
  sigslot::signal1<AsyncSocket*> SignalWriteEvent; // ready to write
  sigslot::signal1<AsyncSocket*> SignalConnectEvent; // connected
  sigslot::signal2<AsyncSocket*,int> SignalCloseEvent; // closed
  // TODO: error
};

class AsyncSocketAdapter : public AsyncSocket {
public:
  AsyncSocketAdapter(Socket * socket) : socket_(socket) {
  }
  AsyncSocketAdapter(AsyncSocket * socket) : socket_(socket) {
    socket->SignalConnectEvent.connect(this, &AsyncSocketAdapter::OnConnectEvent);
    socket->SignalReadEvent.connect(this, &AsyncSocketAdapter::OnReadEvent);
    socket->SignalWriteEvent.connect(this, &AsyncSocketAdapter::OnWriteEvent);
    socket->SignalCloseEvent.connect(this, &AsyncSocketAdapter::OnCloseEvent);
  }
  virtual ~AsyncSocketAdapter() { delete socket_; }

  virtual SocketAddress GetLocalAddress() const { return socket_->GetLocalAddress(); }
  virtual SocketAddress GetRemoteAddress() const { return socket_->GetRemoteAddress(); }

  virtual int Bind(const SocketAddress& addr) { return socket_->Bind(addr); }
  virtual int Connect(const SocketAddress& addr) {return socket_->Connect(addr); }
  virtual int Send(const void *pv, size_t cb) { return socket_->Send(pv, cb); }
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr) { return socket_->SendTo(pv, cb, addr); }
  virtual int Recv(void *pv, size_t cb) { return socket_->Recv(pv, cb); }
  virtual int RecvFrom(void *pv, size_t cb, SocketAddress *paddr) { return socket_->RecvFrom(pv, cb, paddr); }
  virtual int Listen(int backlog) { return socket_->Listen(backlog); }
  virtual Socket *Accept(SocketAddress *paddr) { return socket_->Accept(paddr); }
  virtual int Close() { return socket_->Close(); }
  virtual int GetError() const { return socket_->GetError(); }
  virtual void SetError(int error) { return socket_->SetError(error); }

  virtual ConnState GetState() const { return socket_->GetState(); }

  virtual int EstimateMTU(uint16* mtu) { return socket_->EstimateMTU(mtu); }
  virtual int SetOption(Option opt, int value) { return socket_->SetOption(opt, value); }

protected:
  virtual void OnConnectEvent(AsyncSocket * socket) { SignalConnectEvent(this); }
  virtual void OnReadEvent(AsyncSocket * socket) { SignalReadEvent(this); }
  virtual void OnWriteEvent(AsyncSocket * socket) { SignalWriteEvent(this); }
  virtual void OnCloseEvent(AsyncSocket * socket, int err) { SignalCloseEvent(this, err); }

  Socket * socket_;
};

} // namespace talk_base

#endif // TALK_BASE_ASYNCSOCKET_H__
