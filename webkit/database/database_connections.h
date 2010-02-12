// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_
#define WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_

#include <map>
#include <vector>

#include "base/string16.h"

namespace webkit_database {

class DatabaseConnections {
 public:
  DatabaseConnections();
  ~DatabaseConnections();

  bool IsEmpty() const;
  bool IsDatabaseOpened(const string16& origin_identifier,
                        const string16& database_name) const;
  bool IsOriginUsed(const string16& origin_identifier) const;
  void AddConnection(const string16& origin_identifier,
                     const string16& database_name);
  void RemoveConnection(const string16& origin_identifier,
                        const string16& database_name);
  void RemoveAllConnections();
  void RemoveConnections(
      const DatabaseConnections& connections,
      std::vector<std::pair<string16, string16> >* closed_dbs);

 private:
  typedef std::map<string16, int> DBConnections;
  typedef std::map<string16, DBConnections> OriginConnections;
  OriginConnections connections_;

  void RemoveConnectionsHelper(const string16& origin_identifier,
                               const string16& database_name,
                               int num_connections);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_
