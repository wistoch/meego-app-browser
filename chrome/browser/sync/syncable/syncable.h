// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_

#include <algorithm>
#include <bitset>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/time.h"
#include "chrome/browser/sync/syncable/blob.h"
#include "chrome/browser/sync/syncable/dir_open_result.h"
#include "chrome/browser/sync/syncable/directory_event.h"
#include "chrome/browser/sync/syncable/path_name_cmp.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/dbgq.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/row_iterator.h"
#include "chrome/browser/sync/util/sync_types.h"

struct PurgeInfo;

namespace sync_api {
class ReadTransaction;
class WriteNode;
class ReadNode;
}

namespace syncable {
class Entry;
}

std::ostream& operator<<(std::ostream& s, const syncable::Entry& e);

namespace syncable {

class DirectoryBackingStore;

static const int64 kInvalidMetaHandle = 0;

enum {
  BEGIN_FIELDS = 0,
  INT64_FIELDS_BEGIN = BEGIN_FIELDS
};

enum MetahandleField {
  // Primary key into the table.  Keep this as a handle to the meta entry
  // across transactions.
  META_HANDLE = INT64_FIELDS_BEGIN
};

enum BaseVersion {
  // After initial upload, the version is controlled by the server, and is
  // increased whenever the data or metadata changes on the server.
  BASE_VERSION = META_HANDLE + 1,
};

enum Int64Field {
  SERVER_VERSION = BASE_VERSION + 1,
  MTIME,
  SERVER_MTIME,
  CTIME,
  SERVER_CTIME,

  // A numeric position value that indicates the relative ordering of
  // this object among its siblings.
  SERVER_POSITION_IN_PARENT,

  LOCAL_EXTERNAL_ID,  // ID of an item in the external local storage that this
                      // entry is associated with. (such as bookmarks.js)

  INT64_FIELDS_END
};

enum {
  INT64_FIELDS_COUNT = INT64_FIELDS_END,
  ID_FIELDS_BEGIN = INT64_FIELDS_END,
};

enum IdField {
  // Code in InitializeTables relies on ID being the first IdField value.
  ID = ID_FIELDS_BEGIN,
  PARENT_ID,
  SERVER_PARENT_ID,

  PREV_ID,
  NEXT_ID,
  ID_FIELDS_END
};

enum {
  ID_FIELDS_COUNT = ID_FIELDS_END - ID_FIELDS_BEGIN,
  BIT_FIELDS_BEGIN = ID_FIELDS_END
};

enum IndexedBitField {
  IS_UNSYNCED = BIT_FIELDS_BEGIN,
  IS_UNAPPLIED_UPDATE,
  INDEXED_BIT_FIELDS_END,
};

enum IsDelField {
  IS_DEL = INDEXED_BIT_FIELDS_END,
};

enum BitField {
  IS_DIR = IS_DEL + 1,
  IS_BOOKMARK_OBJECT,

  SERVER_IS_DIR,
  SERVER_IS_DEL,
  SERVER_IS_BOOKMARK_OBJECT,

  BIT_FIELDS_END
};

enum {
  BIT_FIELDS_COUNT = BIT_FIELDS_END - BIT_FIELDS_BEGIN,
  STRING_FIELDS_BEGIN = BIT_FIELDS_END
};

enum StringField {
  // The name, transformed so as to be suitable for use as a path-element.  It
  // is unique, and legal for this client.
  NAME = STRING_FIELDS_BEGIN,
  // The local name, pre-sanitization.  It is not necessarily unique.  If this
  // is empty, it means |NAME| did not require sanitization.
  UNSANITIZED_NAME,
  // If NAME/UNSANITIZED_NAME are "Foo (2)", then NON_UNIQUE_NAME may be "Foo".
  NON_UNIQUE_NAME,
  // The server version of |NAME|.  It is uniquified, but not necessarily
  // OS-legal.
  SERVER_NAME,
  // The server version of |NON_UNIQUE_NAME|.  Again, if SERVER_NAME is
  // like "Foo (2)" due to a commit-time name aside, SERVER_NON_UNIQUE_NAME
  // may hold the value "Foo".
  SERVER_NON_UNIQUE_NAME,
  // For bookmark entries, the URL of the bookmark.
  BOOKMARK_URL,
  SERVER_BOOKMARK_URL,

  // A tag string which identifies this node as a particular top-level
  // permanent object.  The tag can be thought of as a unique key that
  // identifies a singleton instance.
  SINGLETON_TAG,
  STRING_FIELDS_END,
};

enum {
  STRING_FIELDS_COUNT = STRING_FIELDS_END - STRING_FIELDS_BEGIN,
  BLOB_FIELDS_BEGIN = STRING_FIELDS_END
};

// From looking at the sqlite3 docs, it's not directly stated, but it
// seems the overhead for storing a NULL blob is very small.
enum BlobField {
  // For bookmark entries, the favicon data.  These will be NULL for
  // non-bookmark items.
  BOOKMARK_FAVICON = BLOB_FIELDS_BEGIN,
  SERVER_BOOKMARK_FAVICON,
  BLOB_FIELDS_END,
};

enum {
  BLOB_FIELDS_COUNT = BLOB_FIELDS_END - BLOB_FIELDS_BEGIN
};

enum {
  FIELD_COUNT = BLOB_FIELDS_END,
  // Past this point we have temporaries, stored in memory only.
  BEGIN_TEMPS = BLOB_FIELDS_END,
  BIT_TEMPS_BEGIN = BEGIN_TEMPS,
};

enum BitTemp {
  SYNCING = BIT_TEMPS_BEGIN,
  IS_NEW,  // Means use INSERT instead of UPDATE to save to db.
  DEPRECATED_DELETE_ON_CLOSE,  // Set by redirector, IS_OPEN must also be set.
  DEPRECATED_CHANGED_SINCE_LAST_OPEN,  // Have we been written to since we've
                                       // been opened.
  BIT_TEMPS_END,
};

enum {
  BIT_TEMPS_COUNT = BIT_TEMPS_END - BIT_TEMPS_BEGIN
};

class BaseTransaction;
class WriteTransaction;
class ReadTransaction;
class Directory;
class ScopedDirLookup;
class ExtendedAttribute;

// Instead of:
//   Entry e = transaction.GetById(id);
// use:
//   Entry e(transaction, GET_BY_ID, id);
//
// Why?  The former would require a copy constructor, and it would be difficult
// to enforce that an entry never outlived its transaction if there were a copy
// constructor.
enum GetById {
  GET_BY_ID
};

enum GetByTag {
  GET_BY_TAG
};

enum GetByHandle {
  GET_BY_HANDLE
};

enum GetByPath {
  GET_BY_PATH
};

enum GetByParentIdAndName {
  GET_BY_PARENTID_AND_NAME
};

// DBName is the name stored in the database.
enum GetByParentIdAndDBName {
  GET_BY_PARENTID_AND_DBNAME
};

enum Create {
  CREATE
};

enum CreateNewUpdateItem {
  CREATE_NEW_UPDATE_ITEM
};

typedef std::set<PathString> AttributeKeySet;

// DBName is a PathString with additional transformation methods that are
// useful when trying to derive a unique and legal database name from an
// unsanitized sync name.
class DBName : public PathString {
 public:
  explicit DBName(const PathString& database_name)
      : PathString(database_name) { }

  // TODO(ncarter): Remove these codepaths to maintain alternate titles which
  // are OS legal filenames, Chrome doesn't depend on this like some other
  // browsers do.
  void MakeOSLegal() {
    PathString new_value = MakePathComponentOSLegal(*this);
    if (!new_value.empty())
      swap(new_value);
  }

  // Modify the value of this DBName so that it is not in use by any entry
  // inside |parent_id|, except maybe |e|.  |e| may be NULL if you are trying
  // to compute a name for an entry which has yet to be created.
  void MakeNoncollidingForEntry(BaseTransaction* trans,
                                const Id& parent_id,
                                Entry* e);
};

// SyncName encapsulates a canonical server name.  In general, when we need to
// muck around with a name that the server sends us (e.g. to make it OS legal),
// we try to preserve the original value in a SyncName,
// and distill the new local value into a DBName.
// At other times, we need to apply transforms in the
// other direction -- that is, to create a server-appropriate SyncName from a
// user-updated DBName (which is an OS legal name, but not necessarily in the
// format that the server wants it to be).  For that sort of thing, you should
// initialize a SyncName from the DB name value, and use the methods of
// SyncName to canonicalize it.  At other times, you have a pair of canonical
// server values -- one (the "value") which is unique in the parent, and another
// (the "non unique value") which is not unique in the parent -- and you
// simply want to create a SyncName to hold them as a pair.
class SyncName {
 public:
  // Create a SyncName with the initially specified value.
  explicit SyncName(const PathString& sync_name)
      : value_(sync_name), non_unique_value_(sync_name) { }

  // Create a SyncName by specifying a value and a non-unique value.  If
  // you use this constructor, the values you provide should already be
  // acceptable server names.  Don't use the mutation/sanitization methods
  // on the resulting instance -- mutation won't work if you have distinct
  // values for the unique and non-unique fields.
  SyncName(const PathString& unique_sync_name,
           const PathString& non_unique_sync_name)
      : value_(unique_sync_name), non_unique_value_(non_unique_sync_name) { }

  // Transform |value_| so that it's a legal server name.
  void MakeServerLegal() {
    DCHECK_EQ(value_, non_unique_value_)
        << "Deriving value_ will overwrite non_unique_value_.";
    // Append a trailing space if the value is one of the server's three
    // forbidden special cases.
    if (value_.empty() ||
        value_ == PSTR(".") ||
        value_ == PSTR("..")) {
      value_.append(PSTR(" "));
      non_unique_value_ = value_;
    }
    // TODO(ncarter): Handle server's other requirement: truncation to 256
    // bytes in Unicode NFC.
  }

  const PathString& value() const { return value_; }
  PathString& value() { return value_; }
  const PathString& non_unique_value() const { return non_unique_value_; }
  PathString& non_unique_value() { return non_unique_value_; }

  inline bool operator==(const SyncName& right_hand_side) const {
    return value_ == right_hand_side.value_ &&
        non_unique_value_ == right_hand_side.non_unique_value_;
  }
  inline bool operator!=(const SyncName& right_hand_side) const {
    return !(*this == right_hand_side);
  }
 private:
  PathString value_;
  PathString non_unique_value_;
};

// Name is a SyncName which has an additional DBName that provides a way to
// interpolate the "unsanitized name" according to the syncable convention.
//
// A method might accept a Name as an parameter when the sync and database
// names need to be set simultaneously:
//
//    void PutName(const Name& new_name) {
//       Put(NAME, new_name.db_value());
//       Put(UNSANITIZED_NAME, new_name.GetUnsanitizedName());
//    }
//
// A code point that is trying to convert between local database names and
// server sync names can use Name to help with the conversion:
//
//    SyncName server_name = entry->GetServerName();
//    Name name = Name::FromSyncName(server_name);  // Initially, name.value()
//                                                  // and name.db_value() are
//                                                  // equal to
//                                                  // server_name.value().
//    name.db_value().MakeOSLegal();  // Updates name.db_value in-place,
//                                    // leaving name.value() unchanged.
//    foo->PutName(name);
//
class Name : public SyncName {
 public:
  // Create a Name with an initially specified db_value and value.
  Name(const PathString& db_name, const PathString& sync_name)
      : SyncName(sync_name), db_value_(db_name) { }

  // Create a Name by specifying the db name, sync name, and non-unique
  // sync name values.
  Name(const PathString& db_name, const PathString& sync_name,
       const PathString& non_unique_sync_name)
      : SyncName(sync_name, non_unique_sync_name), db_value_(db_name) { }

  // Create a Name with all name values initially equal to the the single
  // specified argument.
  explicit Name(const PathString& sync_and_db_name)
      : SyncName(sync_and_db_name), db_value_(sync_and_db_name) { }

  // Create a Name using the local (non-SERVER) fields of an EntryKernel.
  static Name FromEntryKernel(struct EntryKernel*);

  // Create a Name from a SyncName.  db_value is initially sync_name.value().
  // non_unique_value() and value() are copied from |sync_name|.
  static Name FromSyncName(const SyncName& sync_name) {
    return Name(sync_name.value(), sync_name.value(),
                sync_name.non_unique_value());
  }

  static Name FromDBNameAndSyncName(const PathString& db_name,
                                    const SyncName& sync_name) {
    return Name(db_name, sync_name.value(), sync_name.non_unique_value());
  }

  // Get the database name.  The non-const version is useful for in-place
  // mutation.
  const DBName& db_value() const { return db_value_; }
  DBName& db_value() { return db_value_; }

  // Do the sync names and database names differ?  This indicates that
  // the sync name has been sanitized, and that GetUnsanitizedName() will
  // be non-empty.
  bool HasBeenSanitized() const { return db_value_ != value(); }

  // Compute the value of the unsanitized name from the current sync and db
  // name values.  The unsanitized name is the sync name value, unless the sync
  // name is the same as the db name value, in which case the unsanitized name
  // is empty.
  PathString GetUnsanitizedName() const {
    return HasBeenSanitized() ? value() : PathString();
  }

  inline bool operator==(const Name& right_hand_side) const {
    return this->SyncName::operator==(right_hand_side) &&
        db_value_ == right_hand_side.db_value_;
  }
  inline bool operator!=(const Name& right_hand_side) const {
    return !(*this == right_hand_side);
  }

 private:
  // The database name, which is maintained to be a legal and unique-in-parent
  // name.
  DBName db_value_;
};

// Why the singular enums?  So the code compile-time dispatches instead of
// runtime dispatches as it would with a single enum and an if() statement.

// The EntryKernel class contains the actual data for an entry.  It
// would be a private class, except the number of required friend
// declarations would bloat the code.
struct EntryKernel {
 protected:
  PathString string_fields[STRING_FIELDS_COUNT];
  Blob blob_fields[BLOB_FIELDS_COUNT];
  int64 int64_fields[INT64_FIELDS_COUNT];
  Id id_fields[ID_FIELDS_COUNT];
  std::bitset<BIT_FIELDS_COUNT> bit_fields;
  std::bitset<BIT_TEMPS_COUNT> bit_temps;

 public:
  std::bitset<FIELD_COUNT> dirty;

  // Contain all this error-prone arithmetic in one place.
  inline int64& ref(MetahandleField field) {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline int64& ref(Int64Field field) {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline Id& ref(IdField field) {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline int64& ref(BaseVersion field) {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline std::bitset<BIT_FIELDS_COUNT>::reference ref(IndexedBitField field) {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline std::bitset<BIT_FIELDS_COUNT>::reference ref(IsDelField field) {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline std::bitset<BIT_FIELDS_COUNT>::reference ref(BitField field) {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline PathString& ref(StringField field) {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline Blob& ref(BlobField field) {
    return blob_fields[field - BLOB_FIELDS_BEGIN];
  }
  inline std::bitset<BIT_TEMPS_COUNT>::reference ref(BitTemp field) {
    return bit_temps[field - BIT_TEMPS_BEGIN];
  }

  inline int64 ref(MetahandleField field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline int64 ref(Int64Field field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline const Id& ref(IdField field) const {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline int64 ref(BaseVersion field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline bool ref(IndexedBitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(IsDelField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(BitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline PathString ref(StringField field) const {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline Blob ref(BlobField field) const {
    return blob_fields[field - BLOB_FIELDS_BEGIN];
  }
  inline bool ref(BitTemp field) const {
    return bit_temps[field - BIT_TEMPS_BEGIN];
  }
};

// A read-only meta entry.
class Entry {
  friend class Directory;
  friend std::ostream& ::operator << (std::ostream& s, const Entry& e);

 public:
  // After constructing, you must check good() to test whether the Get
  // succeeded.
  Entry(BaseTransaction* trans, GetByHandle, int64 handle);
  Entry(BaseTransaction* trans, GetById, const Id& id);
  Entry(BaseTransaction* trans, GetByTag, const PathString& tag);
  Entry(BaseTransaction* trans, GetByPath, const PathString& path);
  Entry(BaseTransaction* trans, GetByParentIdAndName, const Id& id,
      const PathString& name);
  Entry(BaseTransaction* trans, GetByParentIdAndDBName, const Id& id,
      const PathString& name);

  bool good() const { return 0 != kernel_; }

  BaseTransaction* trans() const { return basetrans_; }

  // Field accessors.
  inline int64 Get(MetahandleField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline Id Get(IdField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(Int64Field field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(BaseVersion field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IndexedBitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IsDelField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  PathString Get(StringField field) const;
  inline Blob Get(BlobField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitTemp field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline Name GetName() const {
    DCHECK(kernel_);
    return Name::FromEntryKernel(kernel_);
  }
  inline SyncName GetServerName() const {
    DCHECK(kernel_);
    return SyncName(kernel_->ref(SERVER_NAME),
                    kernel_->ref(SERVER_NON_UNIQUE_NAME));
  }
  inline bool SyncNameMatchesServerName() const {
    DCHECK(kernel_);
    SyncName sync_name(GetName());
    return sync_name == GetServerName();
  }
  inline PathString GetSyncNameValue() const {
    DCHECK(kernel_);
    // This should always be equal to GetName().sync_name().value(), but may be
    // faster.
    return kernel_->ref(UNSANITIZED_NAME).empty() ? kernel_->ref(NAME) :
        kernel_->ref(UNSANITIZED_NAME);
  }
  inline bool ExistsOnClientBecauseDatabaseNameIsNonEmpty() const {
    DCHECK(kernel_);
    return !kernel_->ref(NAME).empty();
  }
  inline bool IsRoot() const {
    DCHECK(kernel_);
    return kernel_->ref(ID).IsRoot();
  }

  void GetAllExtendedAttributes(BaseTransaction* trans,
                                std::set<ExtendedAttribute>* result);
  void GetExtendedAttributesList(BaseTransaction* trans,
                                 AttributeKeySet* result);
  // Flags all extended attributes for deletion on the next SaveChanges.
  void DeleteAllExtendedAttributes(WriteTransaction *trans);

  Directory* dir() const;

  const EntryKernel GetKernelCopy() const {
    return *kernel_;
  }


 protected:  // Don't allow creation on heap, except by sync API wrappers.
  friend class sync_api::ReadNode;
  void* operator new(size_t size) { return (::operator new)(size); }

  inline Entry(BaseTransaction* trans) : basetrans_(trans) { }

 protected:

  BaseTransaction* const basetrans_;

  EntryKernel* kernel_;

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

// A mutable meta entry.  Changes get committed to the database when the
// WriteTransaction is destroyed.
class MutableEntry : public Entry {
  friend class WriteTransaction;
  friend class Directory;
  void Init(WriteTransaction* trans, const Id& parent_id,
      const PathString& name);
 public:
  MutableEntry(WriteTransaction* trans, Create, const Id& parent_id,
               const PathString& name);
  MutableEntry(WriteTransaction* trans, CreateNewUpdateItem, const Id& id);
  MutableEntry(WriteTransaction* trans, GetByHandle, int64);
  MutableEntry(WriteTransaction* trans, GetById, const Id&);
  MutableEntry(WriteTransaction* trans, GetByPath, const PathString& path);
  MutableEntry(WriteTransaction* trans, GetByParentIdAndName, const Id&,
    const PathString& name);
  MutableEntry(WriteTransaction* trans, GetByParentIdAndDBName,
    const Id& parentid, const PathString& name);

  inline WriteTransaction* write_transaction() const {
    return write_transaction_;
  }

  // Field Accessors.  Some of them trigger the re-indexing of the entry.
  // Return true on success, return false on failure, which means
  // that putting the value would have caused a duplicate in the index.
  bool Put(Int64Field field, const int64& value);
  bool Put(IdField field, const Id& value);
  bool Put(StringField field, const PathString& value);
  bool Put(BaseVersion field, int64 value);
  inline bool PutName(const Name& name) {
    return (Put(NAME, name.db_value()) &&
            Put(UNSANITIZED_NAME, name.GetUnsanitizedName()) &&
            Put(NON_UNIQUE_NAME, name.non_unique_value()));
  }
  inline bool PutServerName(const SyncName& server_name) {
    return (Put(SERVER_NAME, server_name.value()) &&
            Put(SERVER_NON_UNIQUE_NAME, server_name.non_unique_value()));
  }
  inline bool Put(BlobField field, const Blob& value) {
    return PutField(field, value);
  }
  inline bool Put(BitField field, bool value) {
    return PutField(field, value);
  }
  inline bool Put(IsDelField field, bool value) {
    return PutIsDel(value);
  }
  bool Put(IndexedBitField field, bool value);

  // Avoids temporary collision in index when renaming a bookmark into another
  // folder.
  bool PutParentIdAndName(const Id& parent_id, const Name& name);

  // Sets the position of this item, and updates the entry kernels of the
  // adjacent siblings so that list invariants are maintained.  Returns false
  // and fails if |predecessor_id| does not identify a sibling.  Pass the root
  // ID to put the node in first position.
  bool PutPredecessor(const Id& predecessor_id);

  inline bool Put(BitTemp field, bool value) {
    return PutTemp(field, value);
  }

 protected:

  template <typename FieldType, typename ValueType>
  inline bool PutField(FieldType field, const ValueType& value) {
    DCHECK(kernel_);
    if (kernel_->ref(field) != value) {
      kernel_->ref(field) = value;
      kernel_->dirty[static_cast<int>(field)] = true;
    }
    return true;
  }

  template <typename TempType, typename ValueType>
  inline bool PutTemp(TempType field, const ValueType& value) {
    DCHECK(kernel_);
    kernel_->ref(field) = value;
    return true;
  }

  bool PutIsDel(bool value);

 private:  // Don't allow creation on heap, except by sync API wrappers.
  friend class sync_api::WriteNode;
  void* operator new(size_t size) { return (::operator new)(size); }

  bool PutImpl(StringField field, const PathString& value);

  // Adjusts the successor and predecessor entries so that they no longer
  // refer to this entry.
  void UnlinkFromOrder();

  // Kind of redundant. We should reduce the number of pointers
  // floating around if at all possible. Could we store this in Directory?
  // Scope: Set on construction, never changed after that.
  WriteTransaction* const write_transaction_;

 protected:
  MutableEntry();

  DISALLOW_COPY_AND_ASSIGN(MutableEntry);
};

template <Int64Field field_index>
class SameField;
template <Int64Field field_index>
class HashField;
class LessParentIdAndNames;
class LessMultiIncusionTargetAndMetahandle;
template <typename FieldType, FieldType field_index>
class LessField;
class LessEntryMetaHandles {
 public:
  inline bool operator()(const syncable::EntryKernel& a,
                         const syncable::EntryKernel& b) const {
    return a.ref(META_HANDLE) < b.ref(META_HANDLE);
  }
};
typedef std::set<EntryKernel, LessEntryMetaHandles> OriginalEntries;

// a WriteTransaction has a writer tag describing which body of code is doing
// the write. This is defined up here since DirectoryChangeEvent also contains
// one.
enum WriterTag {
  INVALID, SYNCER, AUTHWATCHER, UNITTEST, VACUUM_AFTER_SAVE, SYNCAPI
};

// A separate Event type and channel for very frequent changes, caused
// by anything, not just the user.
struct DirectoryChangeEvent {
  enum {
    // Means listener should go through list of original entries and
    // calculate what it needs to notify.  It should *not* call any
    // callbacks or attempt to lock anything because a
    // WriteTransaction is being held until the listener returns.
    CALCULATE_CHANGES,
    // Means the WriteTransaction has been released and the listener
    // can now take action on the changes it calculated.
    TRANSACTION_COMPLETE,
    // Channel is closing.
    SHUTDOWN
  } todo;
  // These members are only valid for CALCULATE_CHANGES.
  const OriginalEntries* originals;
  BaseTransaction* trans;
  WriterTag writer;
  typedef DirectoryChangeEvent EventType;
  static inline bool IsChannelShutdownEvent(const EventType& e) {
    return SHUTDOWN == e.todo;
  }
};

struct ExtendedAttributeKey {
  int64 metahandle;
  PathString key;
  inline bool operator < (const ExtendedAttributeKey& x) const {
    if (metahandle != x.metahandle)
      return metahandle < x.metahandle;
    return key.compare(x.key) < 0;
  }
  ExtendedAttributeKey(int64 metahandle, PathString key) :
      metahandle(metahandle), key(key) {  }
};

struct ExtendedAttributeValue {
  Blob value;
  bool is_deleted;
  bool dirty;
};

typedef std::map<ExtendedAttributeKey, ExtendedAttributeValue>
    ExtendedAttributes;

typedef std::set<int64> MetahandleSet;

// A list of metahandles whose metadata should not be purged.
typedef std::multiset<int64> Pegs;

// The name Directory in this case means the entire directory
// structure within a single user account.
//
// Sqlite is a little goofy, in that each thread must access a database
// via its own handle.  So, a Directory object should only be accessed
// from a single thread.  Use DirectoryManager's Open() method to
// always get a directory that has been properly initialized on the
// current thread.
//
// The db is protected against concurrent modification by a reader/
// writer lock, negotiated by the ReadTransaction and WriteTransaction
// friend classes.  The in-memory indices are protected against
// concurrent modification by the kernel lock.
//
// All methods which require the reader/writer lock to be held either
//   are protected and only called from friends in a transaction
//   or are public and take a Transaction* argument.
//
// All methods which require the kernel lock to be already held take a
// ScopeKernelLock* argument.
//
// To prevent deadlock, the reader writer transaction lock must always
// be held before acquiring the kernel lock.
class ScopedKernelLock;
class IdFilter;
class DirectoryManager;
struct PathMatcher;

class Directory {
  friend class BaseTransaction;
  friend class Entry;
  friend class ExtendedAttribute;
  friend class MutableEntry;
  friend class MutableExtendedAttribute;
  friend class ReadTransaction;
  friend class ReadTransactionWithoutDB;
  friend class ScopedKernelLock;
  friend class ScopedKernelUnlock;
  friend class WriteTransaction;
  friend class TestUnsaveableDirectory;
 public:
  // Various data that the Directory::Kernel we are backing (persisting data
  // for) needs saved across runs of the application.
  struct PersistedKernelInfo {
    int64 last_sync_timestamp;
    bool initial_sync_ended;
    std::string store_birthday;
    int64 next_id;
    PersistedKernelInfo() : last_sync_timestamp(0),
        initial_sync_ended(false),
        next_id(0) {
    }
  };

  // What the Directory needs on initialization to create itself and its Kernel.
  // Filled by DirectoryBackingStore::Load.
  struct KernelLoadInfo {
    PersistedKernelInfo kernel_info;
    std::string cache_guid;  // Created on first initialization, never changes.
    int64 max_metahandle;    // Computed (using sql MAX aggregate) on init.
    KernelLoadInfo() : max_metahandle(0) {
    }
  };

  // The dirty/clean state of kernel fields backed by the share_info table.
  // This is public so it can be used in SaveChangesSnapshot for persistence.
  enum KernelShareInfoStatus {
    KERNEL_SHARE_INFO_INVALID,
    KERNEL_SHARE_INFO_VALID,
    KERNEL_SHARE_INFO_DIRTY
  };

  // When the Directory is told to SaveChanges, a SaveChangesSnapshot is
  // constructed and forms a consistent snapshot of what needs to be sent to
  // the backing store.
  struct SaveChangesSnapshot {
    KernelShareInfoStatus kernel_info_status;
    PersistedKernelInfo kernel_info;
    OriginalEntries dirty_metas;
    ExtendedAttributes dirty_xattrs;
    SaveChangesSnapshot() : kernel_info_status(KERNEL_SHARE_INFO_INVALID) {
    }
  };

  Directory();
  virtual ~Directory();

  DirOpenResult Open(const PathString& file_path, const PathString& name);

  void Close();

  int64 NextMetahandle();
  // Always returns a negative id.  Positive client ids are generated
  // by the server only.
  Id NextId();

  PathString file_path() const { return kernel_->db_path; }
  bool good() const { return NULL != store_; }

  // The sync timestamp is an index into the list of changes for an account.
  // It doesn't actually map to any time scale, it's name is an historical
  // anomaly.
  int64 last_sync_timestamp() const;
  void set_last_sync_timestamp(int64 timestamp);

  bool initial_sync_ended() const;
  void set_initial_sync_ended(bool value);

  PathString name() const { return kernel_->name_; }

  // (Account) Store birthday is opaque to the client,
  // so we keep it in the format it is in the proto buffer
  // in case we switch to a binary birthday later.
  std::string store_birthday() const;
  void set_store_birthday(std::string store_birthday);

  // Unique to each account / client pair.
  std::string cache_guid() const;

 protected:  // for friends, mainly used by Entry constructors
  EntryKernel* GetChildWithName(const Id& parent_id, const PathString& name);
  EntryKernel* GetChildWithDBName(const Id& parent_id, const PathString& name);
  EntryKernel* GetEntryByHandle(const int64 handle);
  EntryKernel* GetEntryByHandle(const int64 metahandle, ScopedKernelLock* lock);
  EntryKernel* GetEntryById(const Id& id);
  EntryKernel* GetEntryByTag(const PathString& tag);
  EntryKernel* GetRootEntry();
  EntryKernel* GetEntryByPath(const PathString& path);
  bool ReindexId(EntryKernel* const entry, const Id& new_id);
  bool ReindexParentIdAndName(EntryKernel* const entry, const Id& new_parent_id,
                              const PathString& new_name);
  // These don't do the semantic checking that the redirector needs.
  // The semantic checking is implemented higher up.
  bool Undelete(EntryKernel* const entry);
  bool Delete(EntryKernel* const entry);

  // Overridden by tests.
  virtual DirectoryBackingStore* CreateBackingStore(
      const PathString& dir_name,
      const PathString& backing_filepath);

 private:
  // These private versions expect the kernel lock to already be held
  // before calling.
  EntryKernel* GetEntryById(const Id& id, ScopedKernelLock* const lock);
  EntryKernel* GetChildWithName(const Id& parent_id,
                                const PathString& name,
                                ScopedKernelLock* const lock);
  EntryKernel* GetChildWithNameImpl(const Id& parent_id,
                                    const PathString& name,
                                    ScopedKernelLock* const lock);

  DirOpenResult OpenImpl(const PathString& file_path, const PathString& name);

  struct DirectoryEventTraits {
    typedef DirectoryEvent EventType;
    static inline bool IsChannelShutdownEvent(const DirectoryEvent& event) {
      return DIRECTORY_DESTROYED == event;
    }
  };
 public:
  typedef EventChannel<DirectoryEventTraits, Lock> Channel;
  typedef EventChannel<DirectoryChangeEvent, Lock> ChangesChannel;
  typedef std::vector<int64> ChildHandles;

  // Returns the child meta handles for given parent id.
  void GetChildHandles(BaseTransaction*, const Id& parent_id,
      const PathString& path_spec, ChildHandles* result);
  void GetChildHandles(BaseTransaction*, const Id& parent_id,
      ChildHandles* result);
  void GetChildHandlesImpl(BaseTransaction* trans, const Id& parent_id,
                           PathMatcher* matcher, ChildHandles* result);

  // Find the first or last child in the positional ordering under a parent,
  // and return its id.  Returns a root Id if parent has no children.
  Id GetFirstChildId(BaseTransaction* trans, const Id& parent_id);
  Id GetLastChildId(BaseTransaction* trans, const Id& parent_id);

  // SaveChanges works by taking a consistent snapshot of the current Directory
  // state and indices (by deep copy) under a ReadTransaction, passing this
  // snapshot to the backing store under no transaction, and finally cleaning
  // up by either purging entries no longer needed (this part done under a
  // WriteTransaction) or rolling back dirty and IS_NEW bits.  It also uses
  // internal locking to enforce SaveChanges operations are mutually exclusive.
  //
  // WARNING: THIS METHOD PERFORMS SYNCHRONOUS I/O VIA SQLITE.
  bool SaveChanges();

  // Returns the number of entities with the unsynced bit set.
  int64 unsynced_entity_count() const;

  // Get GetUnsyncedMetaHandles should only be called after SaveChanges and
  // before any new entries have been created. The intention is that the
  // syncer should call it from its PerformSyncQueries member.
  typedef std::vector<int64> UnsyncedMetaHandles;
  void GetUnsyncedMetaHandles(BaseTransaction* trans,
                              UnsyncedMetaHandles* result);

  // Get all the metahandles for unapplied updates
  typedef std::vector<int64> UnappliedUpdateMetaHandles;
  void GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
                                     UnappliedUpdateMetaHandles* result);

  void GetAllExtendedAttributes(BaseTransaction* trans, int64 metahandle,
                                std::set<ExtendedAttribute>* result);
  // Get all extended attribute keys associated with a metahandle
  void GetExtendedAttributesList(BaseTransaction* trans, int64 metahandle,
                                 AttributeKeySet* result);
  // Flags all extended attributes for deletion on the next SaveChanges.
  void DeleteAllExtendedAttributes(WriteTransaction* trans, int64 metahandle);

  // Get the channel for post save notification, used by the syncer.
  inline Channel* channel() const {
    return kernel_->channel;
  }
  inline ChangesChannel* changes_channel() const {
    return kernel_->changes_channel;
  }

  // Checks tree metadata consistency.
  // If full_scan is false, the function will avoid pulling any entries from the
  // db and scan entries currently in ram.
  // If full_scan is true, all entries will be pulled from the database.
  // No return value, CHECKs will be triggered if we're given bad
  // information.
  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           bool full_scan);

  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const OriginalEntries* originals);

  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const MetahandleSet& handles,
                           const IdFilter& idfilter);

 private:
  // Helper to prime ids_index, parent_id_and_names_index, unsynced_metahandles
  // and unapplied_metahandles from metahandles_index.
  void InitializeIndices();

  // Constructs a consistent snapshot of the current Directory state and
  // indices (by deep copy) under a ReadTransaction for use in |snapshot|.
  // See SaveChanges() for more information.
  void TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot);

  // Purges from memory any unused, safe to remove entries that were
  // successfully deleted on disk as a result of the SaveChanges that processed
  // |snapshot|.  See SaveChanges() for more information.
  void VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot);

  // Rolls back dirty and IS_NEW bits in the event that the SaveChanges that
  // processed |snapshot| failed, for ex. due to no disk space.
  void HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot);

  void InsertEntry(EntryKernel* entry, ScopedKernelLock* lock);
  void InsertEntry(EntryKernel* entry);

  // Used by CheckTreeInvariants
  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result);

  static bool SafeToPurgeFromMemory(const EntryKernel* const entry);

  // Helper method used to implement GetFirstChildId/GetLastChildId.
  Id GetChildWithNullIdField(IdField field,
                             BaseTransaction* trans,
                             const Id& parent_id);

  Directory& operator = (const Directory&);

  // TODO(sync):  If lookups and inserts in these sets become
  // the bottle-neck, then we can use hash-sets instead.  But
  // that will require using #ifdefs and compiler-specific code,
  // so use standard sets for now.
 public:
  typedef std::set<EntryKernel*, LessField<MetahandleField, META_HANDLE> >
    MetahandlesIndex;
  typedef std::set<EntryKernel*, LessField<IdField, ID> > IdsIndex;
  // All entries in memory must be in both the MetahandlesIndex and
  // the IdsIndex, but only non-deleted entries will be the
  // ParentIdAndNamesIndex, because there can be multiple deleted
  // entries with the same parent id and name.
  typedef std::set<EntryKernel*, LessParentIdAndNames> ParentIdAndNamesIndex;
  typedef std::vector<int64> MetahandlesToPurge;

 private:

  struct Kernel {
    Kernel(const PathString& db_path, const PathString& name,
           const KernelLoadInfo& info);

    ~Kernel();

    PathString const db_path;
    // TODO(timsteele): audit use of the member and remove if possible
    volatile base::subtle::AtomicWord refcount;
    void AddRef();  // For convenience.
    void Release();

    // Implements ReadTransaction / WriteTransaction using a simple lock.
    Lock transaction_mutex;

    // The name of this directory, used as a key into open_files_;
    PathString const name_;

    // Protects all members below.
    // The mutex effectively protects all the indices, but not the
    // entries themselves.  So once a pointer to an entry is pulled
    // from the index, the mutex can be unlocked and entry read or written.
    //
    // Never hold the mutex and do anything with the database or any
    // other buffered IO.  Violating this rule will result in deadlock.
    Lock mutex;
    MetahandlesIndex* metahandles_index;  // Entries indexed by metahandle
    IdsIndex* ids_index;  // Entries indexed by id
    ParentIdAndNamesIndex* parent_id_and_names_index;
    // So we don't have to create an EntryKernel every time we want to
    // look something up in an index.  Needle in haystack metaphore.
    EntryKernel needle;
    ExtendedAttributes* const extended_attributes;

    // 2 in-memory indices on bits used extremely frequently by the syncer.
    MetahandleSet* const unapplied_update_metahandles;
    MetahandleSet* const unsynced_metahandles;
    // TODO(timsteele): Add a dirty_metahandles index as we now may want to
    // optimize the SaveChanges work of scanning all entries to find dirty ones
    // due to the entire entry domain now being in-memory.

    // TODO(ncarter): Figure out what the hell this is, and comment it.
    Channel* const channel;

    // The changes channel mutex is explicit because it must be locked
    // while holding the transaction mutex and released after
    // releasing the transaction mutex.
    ChangesChannel* const changes_channel;
    Lock changes_channel_mutex;
    KernelShareInfoStatus info_status_;
    // These 5 members are backed in the share_info table, and
    // their state is marked by the flag above.
    // Last sync timestamp fetched from the server.
    int64 last_sync_timestamp_;
    // true iff we ever reached the end of the changelog.
    bool initial_sync_ended_;
    // The store birthday we were given by the server. Contents are opaque to
    // the client.
    std::string store_birthday_;
    // A unique identifier for this account's cache db, used to generate
    // unique server IDs. No need to lock, only written at init time.
    std::string cache_guid_;

    // It doesn't make sense for two threads to run SaveChanges at the same
    // time; this mutex protects that activity.
    Lock save_changes_mutex;

    // The next metahandle and id are protected by kernel mutex.
    int64 next_metahandle;
    int64 next_id;

    // Keep a history of recently flushed metahandles for debugging
    // purposes.  Protected by the save_changes_mutex.
    DebugQueue<int64, 1000> flushed_metahandles_;
  };

  Kernel* kernel_;

  DirectoryBackingStore* store_;
};

class ScopedKernelLock {
 public:
  explicit ScopedKernelLock(const Directory*);
  ~ScopedKernelLock() {}

  AutoLock scoped_lock_;
  Directory* const dir_;
  DISALLOW_COPY_AND_ASSIGN(ScopedKernelLock);
};

// Transactions are now processed FIFO (+overlapping reads).
class BaseTransaction {
  friend class Entry;
 public:
  inline Directory* directory() const { return directory_; }
  inline Id root_id() const { return Id(); }

 protected:
  BaseTransaction(Directory* directory, const char* name,
                  const char* source_file, int line, WriterTag writer);

  void UnlockAndLog(OriginalEntries* entries);

  Directory* const directory_;
  Directory::Kernel* const dirkernel_;  // for brevity
  const char* const name_;
  base::TimeTicks time_acquired_;
  const char* const source_file_;
  const int line_;
  WriterTag writer_;

 private:
  void Lock();

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

// Locks db in constructor, unlocks in destructor.
class ReadTransaction : public BaseTransaction {
 public:
  ReadTransaction(Directory* directory, const char* source_file,
                  int line);
  ReadTransaction(const ScopedDirLookup& scoped_dir,
                  const char* source_file, int line);

  ~ReadTransaction();

 protected:  // Don't allow creation on heap, except by sync API wrapper.
  friend class sync_api::ReadTransaction;
  void* operator new(size_t size) { return (::operator new)(size); }

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

// Locks db in constructor, unlocks in destructor.
class WriteTransaction : public BaseTransaction {
  friend class MutableEntry;
 public:
  explicit WriteTransaction(Directory* directory, WriterTag writer,
                            const char* source_file, int line);
  explicit WriteTransaction(const ScopedDirLookup& directory,
                            WriterTag writer, const char* source_file,
                            int line);
  virtual ~WriteTransaction();

  void SaveOriginal(EntryKernel* entry);

 protected:
  // Before an entry gets modified, we copy the original into a list
  // so that we can issue change notifications when the transaction
  // is done.
  OriginalEntries* const originals_;

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

bool IsLegalNewParent(BaseTransaction* trans, const Id& id, const Id& parentid);
int ComparePathNames(const PathString& a, const PathString& b);

// Exposed in header as this is used as a sqlite3 callback.
int ComparePathNames16(void*, int a_bytes, const void* a, int b_bytes,
                       const void* b);

int64 Now();

// Does wildcard processing.
BOOL PathNameMatch(const PathString& pathname, const PathString& pathspec);

PathString GetFullPath(BaseTransaction* trans, const Entry& e);

inline void ReverseAppend(const PathString& s, PathString* target) {
  target->append(s.rbegin(), s.rend());
}

class ExtendedAttribute {
 public:
  ExtendedAttribute(BaseTransaction* trans, GetByHandle,
                    const ExtendedAttributeKey& key);
  int64 metahandle() const { return i_->first.metahandle; }
  const PathString& key() const { return i_->first.key; }
  const Blob& value() const { return i_->second.value; }
  bool is_deleted() const { return i_->second.is_deleted; }
  bool good() const { return good_; }
  bool operator < (const ExtendedAttribute& x) const {
    return i_->first < x.i_->first;
  }
 protected:
  bool Init(BaseTransaction* trans,
            Directory::Kernel* const kernel,
            ScopedKernelLock* lock,
            const ExtendedAttributeKey& key);
  ExtendedAttribute() { }
  ExtendedAttributes::iterator i_;
  bool good_;
};

class MutableExtendedAttribute : public ExtendedAttribute {
 public:
  MutableExtendedAttribute(WriteTransaction* trans, GetByHandle,
                           const ExtendedAttributeKey& key);
  MutableExtendedAttribute(WriteTransaction* trans, Create,
                           const ExtendedAttributeKey& key);

  Blob* mutable_value() {
    i_->second.dirty = true;
    i_->second.is_deleted = false;
    return &(i_->second.value);
  }

  void delete_attribute() {
    i_->second.dirty = true;
    i_->second.is_deleted = true;
  }
};

// Get an extended attribute from an Entry by name. Returns a pointer
// to a const Blob containing the attribute data, or NULL if there is
// no attribute with the given name. The pointer is valid for the
// duration of the Entry's transaction.
const Blob* GetExtendedAttributeValue(const Entry& e,
                                      const PathString& attribute_name);

// This function sets only the flags needed to get this entry to sync.
void MarkForSyncing(syncable::MutableEntry* e);

// This is not a reset.  It just sets the numeric fields which are not
// initialized by the constructor to zero.
void ZeroFields(EntryKernel* entry, int first_field);

}  // namespace syncable

std::ostream& operator <<(std::ostream&, const syncable::Blob&);

browser_sync::FastDump& operator <<
  (browser_sync::FastDump&, const syncable::Blob&);

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_
