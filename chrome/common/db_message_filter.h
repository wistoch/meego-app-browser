// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DB_MESSAGE_FILTER_H_
#define CHROME_COMMON_DB_MESSAGE_FILTER_H_

#include "ipc/ipc_channel_proxy.h"

// Receives database messages from the browser process and processes them on the
// IO thread.
class DBMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  DBMessageFilter();

 private:
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnDatabaseUpdateSize(const string16& origin_identifier,
                            const string16& database_name,
                            int64 database_size,
                            int64 space_available);
  void OnDatabaseCloseImmediately(const string16& origin_identifier,
                                  const string16& database_name);
};

#endif  // CHROME_COMMON_DB_MESSAGE_FILTER_H_
