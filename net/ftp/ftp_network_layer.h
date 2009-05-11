// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_LAYER_H_
#define NET_FTP_FTP_NETWORK_LAYER_H_

#include "base/ref_counted.h"
#include "net/ftp/ftp_transaction_factory.h"

namespace net {

class FtpNetworkSession;
class FtpAuthCache;

class FtpNetworkLayer : public FtpTransactionFactory {
 public:
  FtpNetworkLayer();
  ~FtpNetworkLayer();

  static FtpTransactionFactory* CreateFactory();

  // FtpTransactionFactory methods:
  virtual FtpTransaction* CreateTransaction();
  virtual void Suspend(bool suspend);

 private:
  scoped_refptr<FtpNetworkSession> session_;
  bool suspended_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_LAYER_H_
