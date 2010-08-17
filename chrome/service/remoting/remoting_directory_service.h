// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_REMOTING_REMOTING_DIRECTORY_SERVICE_H_
#define CHROME_SERVICE_REMOTING_REMOTING_DIRECTORY_SERVICE_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

// A class to provide access to the remoting directory service.
// TODO(hclam): Should implement this in Javascript.
class RemotingDirectoryService : public URLFetcher::Delegate {
 public:
  // Client to receive events from the directory service.
  class Client {
   public:
    virtual ~Client() {}

    // Called when a remoting host was added.
    virtual void OnRemotingHostAdded() {}

    // Called when the last operation has failed.
    virtual void OnRemotingDirectoryError() {}
  };

  RemotingDirectoryService(Client* client);
  ~RemotingDirectoryService();

  // Add this computer as host. Use the token for authentication.
  // TODO(hclam): Need more information for this method call.
  void AddHost(const std::string& token);

  // Cancel the last requested operation.
  void CancelRequest();

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  Client* client_;
  scoped_ptr<URLFetcher> fetcher_;

  // True if a URL request has made and response is pending.
  bool request_pending_;

  DISALLOW_COPY_AND_ASSIGN(RemotingDirectoryService);
};

#endif // CHROME_SERVICE_REMOTING_REMOTING_DIRECTORY_SERVICE_H_
