// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_

#include <deque>
#include <list>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

// The reference implementation database using SQLite.
class SafeBrowsingDatabaseBloom : public SafeBrowsingDatabase {
 public:
  SafeBrowsingDatabaseBloom();
  virtual ~SafeBrowsingDatabaseBloom();

  // SafeBrowsingDatabase interface:

  // Initializes the database with the given filename.  The callback is
  // executed after finishing a chunk.
  virtual bool Init(const std::wstring& filename,
                    Callback0::Type* chunk_inserted_callback);

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase();

  // Returns false if the given url is not in the database.  If it returns
  // true, then either "list" is the name of the matching list, or prefix_hits
  // contains the matching hash prefixes.
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           Time last_update);

  // Processes add/sub commands.  Database will free the chunks when it's done.
  virtual void InsertChunks(const std::string& list_name,
                            std::deque<SBChunk>* chunks);

  // Processs adddel/subdel commands.  Database will free chunk_deletes when
  // it's done.
  virtual void DeleteChunks(std::vector<SBChunkDelete>* chunk_deletes);

  // Returns the lists and their add/sub chunks.
  virtual void GetListsInfo(std::vector<SBListChunkRanges>* lists);

  // Does nothing in this implementation.  Operations in this class are 
  // always synchronous.
  virtual void SetSynchronous();

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& full_hits);

  // Called when the user's machine has resumed from a lower power state.
  virtual void HandleResume();

  virtual void UpdateFinished();
  virtual bool NeedToCheckUrl(const GURL& url);

 private:
  // Opens the database.
  bool Open();

  // Closes the database.
  bool Close();

  // Creates the SQL tables.
  bool CreateTables();

  // Checks the database version and if it's incompatible with the current one,
  // resets the database.
  bool CheckCompatibleVersion();

  // Returns true if any of the given prefixes exist for the given host.
  // Also returns the matching list or any prefix matches.
  void CheckUrl(const std::string& host,
                SBPrefix host_key,
                const std::vector<std::string>& paths,
                std::vector<SBPrefix>* prefix_hits);

  enum ChunkType {
    ADD_CHUNK = 0,
    SUB_CHUNK = 1,
  };

  // Checks if a chunk is in the database.
  bool ChunkExists(int list_id, ChunkType type, int chunk_id);

  // Note the existence of a chunk in the database.  This is used as a faster
  // cache of all of the chunks we have.
  void InsertChunk(int list_id, ChunkType type, int chunk_id);

  // Return a comma separated list of chunk ids that are in the database for
  // the given list and chunk type.
  void GetChunkIds(int list_id, ChunkType type, std::string* list);

  // Adds the given list to the database.  Returns its row id.
  int AddList(const std::string& name);

  // Given a list name, returns its internal id.  If we haven't seen it before,
  // an id is created and stored in the database.  On error, returns 0.
  int GetListID(const std::string& name);

  // Given a list id, returns its name.
  std::string GetListName(int id);

  // Generate a bloom filter.
  virtual void BuildBloomFilter();

  // Used when generating the bloom filter.  Reads a small number of hostkeys
  // starting at the given row id.
  void OnReadHostKeys(int start_id);

  // Synchronous methods to process the currently queued up chunks or add-dels
  void ProcessPendingWork();
  void ProcessChunks();
  void ProcessAddDel();
  void ProcessAddChunks(std::deque<SBChunk>* chunks);
  void ProcessSubChunks(std::deque<SBChunk>* chunks);

  void BeginTransaction();
  void EndTransaction();

  // Processes an add-del command, which deletes all the prefixes that came
  // from that add chunk id.
  void AddDel(const std::string& list_name, int add_chunk_id);
  void AddDel(int list_id, int add_chunk_id);

  // Processes a sub-del command, which just removes the sub chunk id from
  // our list.
  void SubDel(const std::string& list_name, int sub_chunk_id);
  void SubDel(int list_id, int sub_chunk_id);

  // Looks up any cached full hashes we may have.
  void GetCachedFullHashes(const std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           Time last_update);

  // Remove cached entries that have prefixes contained in the entry.
  void ClearCachedHashes(const SBEntry* entry);

  // Remove all GetHash entries that match the list and chunk id from an AddDel.
  void ClearCachedHashesForChunk(int list_id, int add_chunk_id);

  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Clears the did_resume_ flag.  This is called by HandleResume after a delay
  // to handle the case where we weren't in the middle of any work.
  void OnResumeDone();
  // If the did_resume_ flag is set, sleep for a period and then clear the
  // flag.  This method should be called periodically inside of busy disk loops.
  void WaitAfterResume();

  //
  void AddEntry(SBPrefix host, SBEntry* entry);
  void AddPrefix(SBPrefix prefix, int encoded_chunk);
  void AddSub(int chunk, SBPrefix host, SBEntry* entry);
  void AddSubPrefix(SBPrefix prefix, int encoded_chunk, int encoded_add_chunk);
  void ProcessPendingSubs();
  int EncodedChunkId(int chunk, int list_id);
  void DecodeChunkId(int encoded, int* chunk, int* list_id);
  void CreateChunkCaches();

  // The database connection.
  sqlite3* db_;

  // Cache of compiled statements for our database.
  scoped_ptr<SqliteStatementCache> statement_cache_;

  int transaction_count_;
  scoped_ptr<SQLTransaction> transaction_;

  // True iff the database has been opened successfully.
  bool init_;

  std::wstring filename_;

  // Used to store throttled work for commands that write to the database.
  std::queue<std::deque<SBChunk>*> pending_chunks_;

  struct AddDelWork {
    int list_id;
    int add_chunk_id;
    std::vector<std::string> hostkeys;
  };

  std::queue<AddDelWork> pending_add_del_;

  // Called after an add/sub chunk is processed.
  Callback0::Type* chunk_inserted_callback_;

  // Used to schedule resetting the database because of corruption.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseBloom> reset_factory_;

  // Used to schedule resuming from a lower power state.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseBloom> resume_factory_;

  // Used for caching GetHash results.
  typedef struct HashCacheEntry {
    SBFullHash full_hash;
    int list_id;
    int add_chunk_id;
    Time received;
  } HashCacheEntry;

  typedef std::list<HashCacheEntry> HashList;
  typedef base::hash_map<SBPrefix, HashList> HashCache;
  HashCache hash_cache_;

  // Cache of prefixes that returned empty results (no full hash match).
  std::set<SBPrefix> prefix_miss_cache_;

  // a cache of all of the existing add and sub chunks
  std::set<int> add_chunk_cache_;
  std::set<int> sub_chunk_cache_;

  // The number of entries in the add_prefix table. Used to pick the correct
  // size for the bloom filter.
  int add_count_;

  // Set to true if the machine just resumed out of a sleep.  When this happens,
  // we pause disk activity for some time to avoid thrashing the system while
  // it's presumably going to be pretty busy.
  bool did_resume_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseBloom);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_
