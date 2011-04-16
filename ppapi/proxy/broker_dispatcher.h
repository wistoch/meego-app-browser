// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_BROKER_DISPATCHER_H_
#define PPAPI_PROXY_BROKER_DISPATCHER_H_

#include "ppapi/c/trusted/ppp_broker.h"
#include "ppapi/proxy/proxy_channel.h"

namespace pp {
namespace proxy {

class BrokerDispatcher : public ProxyChannel {
 public:
  virtual ~BrokerDispatcher();

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  virtual bool InitBrokerWithChannel(ProxyChannel::Delegate* delegate,
                                     const IPC::ChannelHandle& channel_handle,
                                     bool is_client);

  // Returns true if the dispatcher is on the broker side, or false if it's the
  // browser side.
  // TODO(ddorwin): Implement.
  // virtual bool IsBroker() const = 0;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 protected:
  // You must call InitBrokerWithChannel after the constructor.
  BrokerDispatcher(base::ProcessHandle remote_process_handle,
                   PP_ConnectInstance_Func connect_instance);

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_BROKER_DISPATCHER_H_
