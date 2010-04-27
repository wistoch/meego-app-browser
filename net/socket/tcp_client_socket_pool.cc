// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_pool.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/tcp_client_socket.h"

using base::TimeDelta;

namespace net {

// TCPConnectJobs will time out after this many seconds.  Note this is the total
// time, including both host resolution and TCP connect() times.
//
// TODO(eroman): The use of this constant needs to be re-evaluated. The time
// needed for TCPClientSocketXXX::Connect() can be arbitrarily long, since
// the address list may contain many alternatives, and most of those may
// timeout. Even worse, the per-connect timeout threshold varies greatly
// between systems (anywhere from 20 seconds to 190 seconds).
// See comment #12 at http://crbug.com/23364 for specifics.
static const int kTCPConnectJobTimeoutInSeconds = 240; // 4 minutes.

TCPConnectJob::TCPConnectJob(
    const std::string& group_name,
    const TCPSocketParams& params,
    base::TimeDelta timeout_duration,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    Delegate* delegate,
    const BoundNetLog& net_log)
    : ConnectJob(group_name, timeout_duration, delegate, net_log),
      params_(params),
      client_socket_factory_(client_socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this,
                    &TCPConnectJob::OnIOComplete)),
      resolver_(host_resolver) {}

TCPConnectJob::~TCPConnectJob() {
  // We don't worry about cancelling the host resolution and TCP connect, since
  // ~SingleRequestHostResolver and ~ClientSocket will take care of it.
}

LoadState TCPConnectJob::GetLoadState() const {
  switch (next_state_) {
    case kStateResolveHost:
    case kStateResolveHostComplete:
      return LOAD_STATE_RESOLVING_HOST;
    case kStateTCPConnect:
    case kStateTCPConnectComplete:
      return LOAD_STATE_CONNECTING;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

int TCPConnectJob::ConnectInternal() {
  next_state_ = kStateResolveHost;
  start_time_ = base::TimeTicks::Now();
  return DoLoop(OK);
}

void TCPConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|
}

int TCPConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, kStateNone);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = kStateNone;
    switch (state) {
      case kStateResolveHost:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case kStateResolveHostComplete:
        rv = DoResolveHostComplete(rv);
        break;
      case kStateTCPConnect:
        DCHECK_EQ(OK, rv);
        rv = DoTCPConnect();
        break;
      case kStateTCPConnectComplete:
        rv = DoTCPConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != kStateNone);

  return rv;
}

int TCPConnectJob::DoResolveHost() {
  next_state_ = kStateResolveHostComplete;
  return resolver_.Resolve(params_.destination(), &addresses_, &callback_,
                           net_log());
}

int TCPConnectJob::DoResolveHostComplete(int result) {
  if (result == OK)
    next_state_ = kStateTCPConnect;
  return result;
}

int TCPConnectJob::DoTCPConnect() {
  next_state_ = kStateTCPConnectComplete;
  set_socket(client_socket_factory_->CreateTCPClientSocket(
        addresses_, net_log().net_log()));
  connect_start_time_ = base::TimeTicks::Now();
  return socket()->Connect(&callback_);
}

int TCPConnectJob::DoTCPConnectComplete(int result) {
  if (result == OK) {
    DCHECK(connect_start_time_ != base::TimeTicks());
    DCHECK(start_time_ != base::TimeTicks());
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta total_duration = now - start_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.DNS_Resolution_And_TCP_Connection_Latency2",
        total_duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10),
        100);

    base::TimeDelta connect_duration = now - connect_start_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency",
        connect_duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10),
        100);
  } else {
    // Delete the socket on error.
    set_socket(NULL);
  }

  return result;
}

ConnectJob* TCPClientSocketPool::TCPConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate,
    const BoundNetLog& net_log) const {
  return new TCPConnectJob(group_name, request.params(), ConnectionTimeout(),
                           client_socket_factory_, host_resolver_, delegate,
                           net_log);
}

base::TimeDelta
    TCPClientSocketPool::TCPConnectJobFactory::ConnectionTimeout() const {
  return base::TimeDelta::FromSeconds(kTCPConnectJobTimeoutInSeconds);
}

TCPClientSocketPool::TCPClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    const std::string& name,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    NetworkChangeNotifier* network_change_notifier)
    : base_(max_sockets, max_sockets_per_group, name,
            base::TimeDelta::FromSeconds(kUnusedIdleSocketTimeout),
            base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout),
            new TCPConnectJobFactory(client_socket_factory, host_resolver),
            network_change_notifier) {
  base_.enable_backup_jobs();
}

TCPClientSocketPool::~TCPClientSocketPool() {}

int TCPClientSocketPool::RequestSocket(
    const std::string& group_name,
    const void* params,
    RequestPriority priority,
    ClientSocketHandle* handle,
    CompletionCallback* callback,
    const BoundNetLog& net_log) {
  const TCPSocketParams* casted_params =
      static_cast<const TCPSocketParams*>(params);

  if (net_log.HasListener()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEventWithString(
        NetLog::TYPE_TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
        "host_and_port",
        StringPrintf("%s [port %d]",
                     casted_params->destination().hostname().c_str(),
                     casted_params->destination().port()));
  }

  return base_.RequestSocket(group_name, *casted_params, priority, handle,
                             callback, net_log);
}

void TCPClientSocketPool::CancelRequest(
    const std::string& group_name,
    const ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void TCPClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    ClientSocket* socket) {
  base_.ReleaseSocket(group_name, socket);
}

void TCPClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

int TCPClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState TCPClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

}  // namespace net
