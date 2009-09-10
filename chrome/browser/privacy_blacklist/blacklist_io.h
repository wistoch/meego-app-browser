// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_

#include <list>

#include "base/string16.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"

class FilePath;

// Helper class to keep state while reading multiple text blacklists to
// produce a single binary blacklist used by the Blacklist constructor.
class BlacklistIO {
 public:
  BlacklistIO();
  ~BlacklistIO();

  // Reads a text blacklist, as downloaded from the blacklist provider.
  bool Read(const FilePath& path);

  // Writes a binary blacklist with aggregated entries for all read blacklists.
  bool Write(const FilePath& path);

  // Returns the text of the last occuring error. An empty string is returned
  // if no such error happened.
  const string16& last_error() const {
    return last_error_;
  }

 private:
  // Introspection functions, for testing purposes.
  const std::list<Blacklist::Entry*>& blacklist() const {
    return blacklist_;
  }
  const std::list<Blacklist::Provider*>& providers() const {
    return providers_;
  }

  std::list<Blacklist::Entry*> blacklist_;
  std::list<Blacklist::Provider*> providers_;
  string16 last_error_;  // Stores text of last error, empty if N/A.

  FRIEND_TEST(BlacklistIOTest, Generic);
  FRIEND_TEST(BlacklistIOTest, Combine);
  DISALLOW_COPY_AND_ASSIGN(BlacklistIO);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_
