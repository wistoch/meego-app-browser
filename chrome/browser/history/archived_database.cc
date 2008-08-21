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

#include "base/string_util.h"
#include "chrome/browser/history/archived_database.h"

namespace history {

namespace {

static const int kCurrentVersionNumber = 2;

}  // namespace

ArchivedDatabase::ArchivedDatabase()
    : db_(NULL),
      transaction_nesting_(0) {
}

ArchivedDatabase::~ArchivedDatabase() {
}

bool ArchivedDatabase::Init(const std::wstring& file_name) {
  // Open the history database, using the narrow version of open indicates to
  // sqlite that we want the database to be in UTF-8 if it doesn't already
  // exist.
  DCHECK(!db_) << "Already initialized!";
  if (sqlite3_open(WideToUTF8(file_name).c_str(), &db_) != SQLITE_OK)
    return false;
  statement_cache_ = new SqliteStatementCache(db_);
  DBCloseScoper scoper(&db_, &statement_cache_);

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);

  // Don't use very much memory caching this database. We seldom use it for
  // anything important.
  sqlite3_exec(db_, "PRAGMA cache_size=64", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  BeginTransaction();

  // Version check.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber, db_))
    return false;

  // Create the tables.
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable())
    return false;
  CreateMainURLIndex();

  if (EnsureCurrentVersion() != INIT_OK)
    return false;

  // Succeeded: keep the DB open by detaching the auto-closer.
  scoper.Detach();
  db_closer_.Attach(&db_, &statement_cache_);
  CommitTransaction();
  return true;
}

void ArchivedDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void ArchivedDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transactions";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

sqlite3* ArchivedDatabase::GetDB() {
  return db_;
}

SqliteStatementCache& ArchivedDatabase::GetStatementCache() {
  return *statement_cache_;
}

// Migration -------------------------------------------------------------------

InitStatus ArchivedDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber)
    return INIT_TOO_NEW;

  // NOTICE: If you are changing structures for things shared with the archived
  // history file like URLs, visits, or downloads, that will need migration as
  // well. Instead of putting such migration code in this class, it should be
  // in the corresponding file (url_database.cc, etc.) and called from here and
  // from the archived_database.cc.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  int cur_version = meta_table_.GetVersionNumber();

  // Put migration code here

  if (cur_version == 1) {
    if (!DropStarredIDFromURLs())
      return INIT_FAILURE;
    cur_version = 2;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(cur_version);
  }

  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Archived database version " << cur_version << " is too old to handle.";

  return INIT_OK;
}
}  // namespace history
