// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Class that handles the details of sending and receiving client
// invalidation packets.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/non_thread_safe.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "talk/xmpp/jid.h"

namespace invalidation {
class InvalidationClient;
class NetworkEndpoint;
}  // namespace invalidation

namespace talk_base {
class Task;
}  // namespace

namespace sync_notifier {

class CacheInvalidationPacketHandler {
 public:
  // Starts routing packets from |invalidation_client| using
  // |base_task|.  |base_task.get()| must still be non-NULL.
  // |invalidation_client| must not already be routing packets through
  // something.  Does not take ownership of |invalidation_client|.
  CacheInvalidationPacketHandler(
      base::WeakPtr<talk_base::Task> base_task,
      invalidation::InvalidationClient* invalidation_client);

  // Makes the invalidation client passed into the constructor not
  // route packets through the XMPP client passed into the constructor
  // anymore.
  ~CacheInvalidationPacketHandler();

 private:
  FRIEND_TEST(CacheInvalidationPacketHandlerTest, Basic);

  void HandleOutboundPacket(
      invalidation::NetworkEndpoint* const& network_endpoint);

  void HandleInboundPacket(const std::string& packet);

  NonThreadSafe non_thread_safe_;
  base::ScopedCallbackFactory<CacheInvalidationPacketHandler>
      scoped_callback_factory_;
  scoped_ptr<CallbackRunner<Tuple1<invalidation::NetworkEndpoint* const&> > >
      handle_outbound_packet_callback_;

  base::WeakPtr<talk_base::Task> base_task_;
  invalidation::InvalidationClient* invalidation_client_;

  // Parameters for sent messages.

  // Monotonically increasing sequence number.
  int seq_;
  // Unique session token.
  const std::string sid_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationPacketHandler);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
