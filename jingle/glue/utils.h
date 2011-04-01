// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_UTILS_H_
#define JINGLE_GLUE_UTILS_H_

namespace net {
class IPEndPoint;
}  // namespace net

namespace talk_base {
  class SocketAddress;
}  // namespace talk_base

namespace jingle_glue {

// Chromium and libjingle represent socket addresses differently. The
// following two functions are used to convert addresses from one
// representation to another.
bool IPEndPointToSocketAddress(const net::IPEndPoint& address_chrome,
                               talk_base::SocketAddress* address_lj);
bool SocketAddressToIPEndPoint(const talk_base::SocketAddress& address_lj,
                               net::IPEndPoint* address_chrome);

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_UTILS_H_
