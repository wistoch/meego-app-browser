// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/notifier/base/ssl_adapter.h"

#if defined(OS_WIN)
#include "talk/base/ssladapter.h"
#else
#include "chrome/common/net/notifier/communicator/ssl_socket_adapter.h"
#endif

namespace notifier {

talk_base::SSLAdapter* CreateSSLAdapter(talk_base::AsyncSocket* socket) {
  talk_base::SSLAdapter* ssl_adapter =
#if defined(OS_WIN)
      talk_base::SSLAdapter::Create(socket);
#else
      notifier::SSLSocketAdapter::Create(socket);
#endif
  DCHECK(ssl_adapter);
  return ssl_adapter;
}

}  // namespace notifier

