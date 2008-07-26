// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/history/in_memory_database.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace history {

InMemoryDatabase::InMemoryDatabase() : URLDatabase(), db_(NULL) {
}

InMemoryDatabase::~InMemoryDatabase() {
}

bool InMemoryDatabase::InitDB() {
  DCHECK(!db_) << "Already initialized!";
  if (sqlite3_open(":memory:", &db_) != SQLITE_OK) {
    NOTREACHED() << "Cannot open memory database";
    return false;
  }
  statement_cache_ = new SqliteStatementCache(db_);
  DBCloseScoper scoper(&db_, &statement_cache_);  // closes the DB on error

  // No reason to leave data behind in memory when rows are removed.
  sqlite3_exec(db_, "PRAGMA auto_vacuum=1", NULL, NULL, NULL);
  // Set the database page size to 4K for better performance.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);
  // Ensure this is really an in-memory-only cache.
  sqlite3_exec(db_, "PRAGMA temp_store=MEMORY", NULL, NULL, NULL);

  // Create the URL table, but leave it empty for now.
  if (!CreateURLTable(false)) {
    NOTREACHED() << "Unable to create table";
    return false;
  }

  // Succeeded, keep the DB open.
  scoper.Detach();
  db_closer_.Attach(&db_, &statement_cache_);
  return true;
}

bool InMemoryDatabase::InitFromScratch() {
  if (!InitDB())
    return false;

  // InitDB doesn't create the index so in the disk-loading case, it can be
  // added afterwards.
  CreateMainURLIndex();
  return true;
}

bool InMemoryDatabase::InitFromDisk(const std::wstring& history_name) {
  if (!InitDB())
    return false;

  // Attach to the history database on disk.  (We can't ATTACH in the middle of
  // a transaction.)
  SQLStatement attach;
  if (attach.prepare(db_, "ATTACH ? AS history") != SQLITE_OK) {
    NOTREACHED() << "Unable to attach to history database.";
    return false;
  }
  attach.bind_string(0, WideToUTF8(history_name));
  if (attach.step() != SQLITE_DONE) {
    NOTREACHED() << "Unable to bind";
    return false;
  }

  // Copy URL data to memory.
  if (sqlite3_exec(db_,
      "INSERT INTO urls SELECT * FROM history.urls WHERE typed_count > 0",
      NULL, NULL, NULL) != SQLITE_OK) {
    // Unable to get data from the history database. This is OK, the file may
    // just not exist yet.
  }

  // Detach from the history database on disk.
  if (sqlite3_exec(db_, "DETACH history", NULL, NULL, NULL) != SQLITE_OK) {
    NOTREACHED() << "Unable to detach from history database.";
    return false;
  }

  // Index the table, this is faster than creating the index first and then
  // inserting into it.
  CreateMainURLIndex();

  return true;
}

sqlite3* InMemoryDatabase::GetDB() {
  return db_;
}

SqliteStatementCache& InMemoryDatabase::GetStatementCache() {
  return *statement_cache_;
}

}  // namespace history
