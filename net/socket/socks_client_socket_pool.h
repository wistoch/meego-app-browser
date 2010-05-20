// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"

namespace net {

class ClientSocketFactory;
class ConnectJobFactory;

class SOCKSSocketParams {
 public:
  SOCKSSocketParams(const TCPSocketParams& proxy_server, bool socks_v5,
                    const HostPortPair& host_port_pair,
                    RequestPriority priority, const GURL& referrer)
      : tcp_params_(proxy_server),
        destination_(host_port_pair.host, host_port_pair.port),
        socks_v5_(socks_v5) {
    // The referrer is used by the DNS prefetch system to correlate resolutions
    // with the page that triggered them. It doesn't impact the actual addresses
    // that we resolve to.
    destination_.set_referrer(referrer);
    destination_.set_priority(priority);
  }

  const TCPSocketParams& tcp_params() const { return tcp_params_; }
  const HostResolver::RequestInfo& destination() const { return destination_; }
  bool is_socks_v5() const { return socks_v5_; };

 private:
  // The tcp connection must point toward the proxy server.
  const TCPSocketParams tcp_params_;
  // This is the HTTP destination.
  HostResolver::RequestInfo destination_;
  const bool socks_v5_;
};

// SOCKSConnectJob handles the handshake to a socks server after setting up
// an underlying transport socket.
class SOCKSConnectJob : public ConnectJob {
 public:
  SOCKSConnectJob(const std::string& group_name,
                  const SOCKSSocketParams& params,
                  const base::TimeDelta& timeout_duration,
                  const scoped_refptr<TCPClientSocketPool>& tcp_pool,
                  const scoped_refptr<HostResolver> &host_resolver,
                  Delegate* delegate,
                  const BoundNetLog& net_log);
  virtual ~SOCKSConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const;

 private:
  enum State {
    kStateTCPConnect,
    kStateTCPConnectComplete,
    kStateSOCKSConnect,
    kStateSOCKSConnectComplete,
    kStateNone,
  };

  // Begins the tcp connection and the SOCKS handshake.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal();

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTCPConnect();
  int DoTCPConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);

  SOCKSSocketParams socks_params_;
  const scoped_refptr<TCPClientSocketPool> tcp_pool_;
  const scoped_refptr<HostResolver> resolver_;

  State next_state_;
  CompletionCallbackImpl<SOCKSConnectJob> callback_;
  scoped_ptr<ClientSocketHandle> tcp_socket_handle_;
  scoped_ptr<ClientSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSConnectJob);
};

class SOCKSClientSocketPool : public ClientSocketPool {
 public:
  SOCKSClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      const std::string& name,
      const scoped_refptr<HostResolver>& host_resolver,
      const scoped_refptr<TCPClientSocketPool>& tcp_pool,
      NetworkChangeNotifier* network_change_notifier);

  // ClientSocketPool methods:
  virtual int RequestSocket(const std::string& group_name,
                            const void* connect_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log);

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket);

  virtual void CloseIdleSockets();

  virtual int IdleSocketCount() const {
    return base_.idle_socket_count();
  }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

  virtual base::TimeDelta ConnectionTimeout() const {
    return base_.ConnectionTimeout();
  }

  virtual const std::string& name() const { return base_.name(); };

 protected:
  virtual ~SOCKSClientSocketPool();

 private:
  typedef ClientSocketPoolBase<SOCKSSocketParams> PoolBase;

  class SOCKSConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    SOCKSConnectJobFactory(const scoped_refptr<TCPClientSocketPool>& tcp_pool,
                           HostResolver* host_resolver)
        : tcp_pool_(tcp_pool),
          host_resolver_(host_resolver) {}

    virtual ~SOCKSConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.
    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate,
        const BoundNetLog& net_log) const;

    virtual base::TimeDelta ConnectionTimeout() const;

   private:
    const scoped_refptr<TCPClientSocketPool> tcp_pool_;
    const scoped_refptr<HostResolver> host_resolver_;

    DISALLOW_COPY_AND_ASSIGN(SOCKSConnectJobFactory);
  };

  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(SOCKSClientSocketPool, SOCKSSocketParams)

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CLIENT_SOCKET_POOL_H_
