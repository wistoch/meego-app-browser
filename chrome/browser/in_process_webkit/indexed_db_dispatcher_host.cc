// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/serialized_script_value.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBIndex.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"

using WebKit::WebDOMStringList;
using WebKit::WebIDBCursor;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBIndex;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransaction;
using WebKit::WebSecurityOrigin;
using WebKit::WebSerializedScriptValue;
using WebKit::WebVector;

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IPC::Message::Sender* sender, WebKitContext* webkit_context)
    : sender_(sender),
      webkit_context_(webkit_context),
      ALLOW_THIS_IN_INITIALIZER_LIST(database_dispatcher_host_(
          new DatabaseDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(index_dispatcher_host_(
          new IndexDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(object_store_dispatcher_host_(
          new ObjectStoreDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(cursor_dispatcher_host_(
          new CursorDispatcherHost(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(transaction_dispatcher_host_(
          new TransactionDispatcherHost(this))),
      process_handle_(0) {
  DCHECK(sender_);
  DCHECK(webkit_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
}

void IndexedDBDispatcherHost::Init(int process_id,
                                   base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(sender_);  // Ensure Shutdown() has not been called.
  DCHECK(!process_handle_);  // Make sure Init() has not yet been called.
  DCHECK(process_handle);
  process_id_ = process_id;
  process_handle_ = process_handle;
}

void IndexedDBDispatcherHost::Shutdown() {
  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    sender_ = NULL;

    bool success = ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &IndexedDBDispatcherHost::Shutdown));
    if (success)
      return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT) ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));
  DCHECK(!sender_);

  database_dispatcher_host_.reset();
  index_dispatcher_host_.reset();
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(process_handle_);

  switch (message.type()) {
    case ViewHostMsg_IDBCursorDestroyed::ID:
    case ViewHostMsg_IDBCursorDirection::ID:
    case ViewHostMsg_IDBCursorKey::ID:
    case ViewHostMsg_IDBCursorValue::ID:
    case ViewHostMsg_IDBFactoryOpen::ID:
    case ViewHostMsg_IDBFactoryAbortPendingTransactions::ID:
    case ViewHostMsg_IDBDatabaseName::ID:
    case ViewHostMsg_IDBDatabaseDescription::ID:
    case ViewHostMsg_IDBDatabaseVersion::ID:
    case ViewHostMsg_IDBDatabaseObjectStores::ID:
    case ViewHostMsg_IDBDatabaseCreateObjectStore::ID:
    case ViewHostMsg_IDBDatabaseObjectStore::ID:
    case ViewHostMsg_IDBDatabaseRemoveObjectStore::ID:
    case ViewHostMsg_IDBDatabaseSetVersion::ID:
    case ViewHostMsg_IDBDatabaseTransaction::ID:
    case ViewHostMsg_IDBDatabaseDestroyed::ID:
    case ViewHostMsg_IDBIndexName::ID:
    case ViewHostMsg_IDBIndexKeyPath::ID:
    case ViewHostMsg_IDBIndexUnique::ID:
    case ViewHostMsg_IDBIndexDestroyed::ID:
    case ViewHostMsg_IDBObjectStoreName::ID:
    case ViewHostMsg_IDBObjectStoreKeyPath::ID:
    case ViewHostMsg_IDBObjectStoreIndexNames::ID:
    case ViewHostMsg_IDBObjectStoreGet::ID:
    case ViewHostMsg_IDBObjectStoreOpenCursor::ID:
    case ViewHostMsg_IDBObjectStorePut::ID:
    case ViewHostMsg_IDBObjectStoreRemove::ID:
    case ViewHostMsg_IDBObjectStoreCreateIndex::ID:
    case ViewHostMsg_IDBObjectStoreIndex::ID:
    case ViewHostMsg_IDBObjectStoreRemoveIndex::ID:
    case ViewHostMsg_IDBObjectStoreDestroyed::ID:
    case ViewHostMsg_IDBTransactionDestroyed::ID:
    case ViewHostMsg_IDBTransactionObjectStore::ID:
      break;
    default:
      return false;
  }

  bool success = ChromeThread::PostTask(
      ChromeThread::WEBKIT, FROM_HERE, NewRunnableMethod(
          this, &IndexedDBDispatcherHost::OnMessageReceivedWebKit, message));
  DCHECK(success);
  return true;
}

void IndexedDBDispatcherHost::Send(IPC::Message* message) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    // TODO(jorlow): Even if we successfully post, I believe it's possible for
    //               the task to never run (if the IO thread is already shutting
    //               down).  We may want to handle this case, though
    //               realistically it probably doesn't matter.
    if (!ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE, NewRunnableMethod(
                this, &IndexedDBDispatcherHost::Send, message))) {
      // The IO thread is dead.
      delete message;
    }
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!sender_)
    delete message;
  else
    sender_->Send(message);
}

void IndexedDBDispatcherHost::OnMessageReceivedWebKit(
    const IPC::Message& message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  DCHECK(process_handle_);

  bool msg_is_ok = true;
  bool handled =
      database_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      index_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      object_store_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      cursor_dispatcher_host_->OnMessageReceived(message, &msg_is_ok) ||
      transaction_dispatcher_host_->OnMessageReceived(message, &msg_is_ok);

  if (!handled) {
    handled = true;
    DCHECK(msg_is_ok);
    IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost, message, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_IDBFactoryOpen,
                          OnIDBFactoryOpen)
      IPC_MESSAGE_HANDLER(ViewHostMsg_IDBFactoryAbortPendingTransactions,
                          OnIDBFactoryAbortPendingTransactions)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }

  DCHECK(handled);
  if (!msg_is_ok) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message.type(),
                                                         process_handle_);
  }
}

int32 IndexedDBDispatcherHost::Add(WebIDBCursor* idb_cursor) {
  return cursor_dispatcher_host_->map_.Add(idb_cursor);
}

int32 IndexedDBDispatcherHost::Add(WebIDBDatabase* idb_database) {
  return database_dispatcher_host_->map_.Add(idb_database);
}

int32 IndexedDBDispatcherHost::Add(WebIDBIndex* idb_index) {
  return index_dispatcher_host_->map_.Add(idb_index);
}

int32 IndexedDBDispatcherHost::Add(WebIDBObjectStore* idb_object_store) {
  return object_store_dispatcher_host_->map_.Add(idb_object_store);
}

void IndexedDBDispatcherHost::Add(WebIDBTransaction* idb_transaction) {
  transaction_dispatcher_host_->map_.AddWithID(
      idb_transaction, idb_transaction->id());
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
    const ViewHostMsg_IDBFactoryOpen_Params& params) {
  // TODO(jorlow): Check the content settings map and use params.routing_id_
  //               if it's necessary to ask the user for permission.

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  Context()->GetIDBFactory()->open(
      params.name_, params.description_,
      new IndexedDBCallbacks<WebIDBDatabase>(this, params.response_id_),
      WebSecurityOrigin::createFromDatabaseIdentifier(params.origin_), NULL);
}

void IndexedDBDispatcherHost::OnIDBFactoryAbortPendingTransactions(
    const std::vector<int32>& ids) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));

  WebVector<int32> pendingIDs = ids;
  Context()->GetIDBFactory()->abortPendingTransactions(pendingIDs);
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 return_object_id,
    IPC::Message* reply_msg, uint32 message_type) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  ObjectType* return_object = map->Lookup(return_object_id);
  if (!return_object) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message_type,
                                                         process_handle_);
    delete reply_msg;
  }
  return return_object;
}

template <typename ReplyType, typename MessageType,
          typename MapObjectType, typename Method>
void IndexedDBDispatcherHost::SyncGetter(
    IDMap<MapObjectType, IDMapOwnPointer>* map, int32 object_id,
    IPC::Message* reply_msg, Method method) {
  MapObjectType* object = GetOrTerminateProcess(map, object_id, reply_msg,
                                                MessageType::ID);
  if (!object)
      return;

  ReplyType reply = (object->*method)();
  MessageType::WriteReplyParams(reply_msg, reply);
  Send(reply_msg);
}

template <typename ObjectType>
void IndexedDBDispatcherHost::DestroyObject(
    IDMap<ObjectType, IDMapOwnPointer>* map, int32 object_id,
    uint32 message_type) {
  GetOrTerminateProcess(map, object_id, NULL, message_type);
  map->Remove(object_id);
}


//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::DatabaseDispatcherHost
//

IndexedDBDispatcherHost::DatabaseDispatcherHost::DatabaseDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
}

IndexedDBDispatcherHost::DatabaseDispatcherHost::~DatabaseDispatcherHost() {
}

bool IndexedDBDispatcherHost::DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::DatabaseDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseDescription,
                                    OnDescription)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseVersion, OnVersion)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseObjectStores,
                                    OnObjectStores)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseCreateObjectStore,
                        OnCreateObjectStore)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseObjectStore,
                                    OnObjectStore)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseRemoveObjectStore,
                        OnRemoveObjectStore)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseSetVersion,
                        OnSetVersion)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBDatabaseTransaction,
                                    OnTransaction)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBDatabaseDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseName>(
      &map_, object_id, reply_msg, &WebIDBDatabase::name);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDescription(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseDescription>(
      &map_, object_id, reply_msg, &WebIDBDatabase::description);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnVersion(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBDatabaseVersion>(
      &map_, object_id, reply_msg, &WebIDBDatabase::version);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnObjectStores(
    int32 idb_database_id, IPC::Message* reply_msg) {
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, reply_msg,
      ViewHostMsg_IDBDatabaseObjectStores::ID);
  if (!idb_database)
    return;

  WebDOMStringList web_object_stores = idb_database->objectStores();
  std::vector<string16> object_stores;
  object_stores.reserve(web_object_stores.length());
  for (unsigned i = 0; i < web_object_stores.length(); ++i)
    object_stores.push_back(web_object_stores.item(i));
  ViewHostMsg_IDBDatabaseObjectStores::WriteReplyParams(reply_msg,
                                                        object_stores);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateObjectStore(
    const ViewHostMsg_IDBDatabaseCreateObjectStore_Params& params) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, params.idb_database_id_, NULL,
      ViewHostMsg_IDBDatabaseCreateObjectStore::ID);
  if (!idb_database)
    return;
  idb_database->createObjectStore(
      params.name_, params.key_path_, params.auto_increment_,
      new IndexedDBCallbacks<WebIDBObjectStore>(parent_, params.response_id_));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnObjectStore(
    int32 idb_database_id, const string16& name, int32 mode,
    IPC::Message* reply_msg) {
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, reply_msg,
      ViewHostMsg_IDBDatabaseObjectStore::ID);
  if (!idb_database)
    return;

  WebIDBObjectStore* object_store = idb_database->objectStore(name, mode);
  int32 object_id = object_store ? parent_->Add(object_store) : 0;
  ViewHostMsg_IDBDatabaseObjectStore::WriteReplyParams(
      reply_msg, !!object_store, object_id);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnRemoveObjectStore(
    int32 idb_database_id, int32 response_id, const string16& name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, NULL,
      ViewHostMsg_IDBDatabaseRemoveObjectStore::ID);
  if (!idb_database)
    return;
  idb_database->removeObjectStore(
      name, new IndexedDBCallbacks<void>(parent_, response_id));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetVersion(
    int32 idb_database_id, int32 response_id, const string16& version) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBDatabase* idb_database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, NULL,
      ViewHostMsg_IDBDatabaseSetVersion::ID);
  if (!idb_database)
    return;
  idb_database->setVersion(
      version, new IndexedDBCallbacks<void>(parent_, response_id));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnTransaction(
    int32 idb_database_id, const std::vector<string16>& names,
    int32 mode, int32 timeout, IPC::Message* reply_msg) {
  WebIDBDatabase* database = parent_->GetOrTerminateProcess(
      &map_, idb_database_id, reply_msg,
      ViewHostMsg_IDBDatabaseTransaction::ID);
  if (!database)
      return;

  WebDOMStringList object_stores;
  for (std::vector<string16>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    object_stores.append(*it);
  }

  WebIDBTransaction* transaction = database->transaction(
      object_stores, mode, timeout);
  transaction->setCallbacks(
      new IndexedDBTransactionCallbacks(parent_, transaction->id()));
  parent_->Add(transaction);
  ViewHostMsg_IDBDatabaseTransaction::WriteReplyParams(
      reply_msg, transaction->id());
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id,
                         ViewHostMsg_IDBDatabaseDestroyed::ID);
}


//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::IndexDispatcherHost
//

IndexedDBDispatcherHost::IndexDispatcherHost::IndexDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
}

IndexedDBDispatcherHost::IndexDispatcherHost::~IndexDispatcherHost() {
}

bool IndexedDBDispatcherHost::IndexDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::IndexDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexKeyPath, OnKeyPath)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBIndexUnique, OnUnique)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBIndexDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::IndexDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBIndexName>(
      &map_, object_id, reply_msg, &WebIDBIndex::name);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnKeyPath(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<NullableString16, ViewHostMsg_IDBIndexKeyPath>(
      &map_, object_id, reply_msg, &WebIDBIndex::keyPath);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnUnique(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<bool, ViewHostMsg_IDBIndexUnique>(
      &map_, object_id, reply_msg, &WebIDBIndex::unique);
}

void IndexedDBDispatcherHost::IndexDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(&map_, object_id, ViewHostMsg_IDBIndexDestroyed::ID);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::ObjectStoreDispatcherHost
//

IndexedDBDispatcherHost::ObjectStoreDispatcherHost::ObjectStoreDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
}

IndexedDBDispatcherHost::
ObjectStoreDispatcherHost::~ObjectStoreDispatcherHost() {
}

bool IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::ObjectStoreDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreName, OnName)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreKeyPath,
                                    OnKeyPath)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreIndexNames,
                                    OnIndexNames)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreGet, OnGet);
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStorePut, OnPut);
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreRemove, OnRemove);
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreCreateIndex, OnCreateIndex);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBObjectStoreIndex, OnIndex);
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreRemoveIndex, OnRemoveIndex);
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnName(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<string16, ViewHostMsg_IDBObjectStoreName>(
      &map_, object_id, reply_msg, &WebIDBObjectStore::name);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnKeyPath(
    int32 object_id, IPC::Message* reply_msg) {
  parent_->SyncGetter<NullableString16, ViewHostMsg_IDBObjectStoreKeyPath>(
      &map_, object_id, reply_msg, &WebIDBObjectStore::keyPath);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndexNames(
    int32 idb_object_store_id, IPC::Message* reply_msg) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, reply_msg,
      ViewHostMsg_IDBObjectStoreIndexNames::ID);
  if (!idb_object_store)
    return;

  WebDOMStringList web_index_names = idb_object_store->indexNames();
  std::vector<string16> index_names;
  index_names.reserve(web_index_names.length());
  for (unsigned i = 0; i < web_index_names.length(); ++i)
    index_names[i] = web_index_names.item(i);
  ViewHostMsg_IDBObjectStoreIndexNames::WriteReplyParams(reply_msg,
                                                         index_names);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnGet(
    int idb_object_store_id, int32 response_id, const IndexedDBKey& key) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL, ViewHostMsg_IDBObjectStoreGet::ID);
  if (!idb_object_store)
    return;
  idb_object_store->get(key, new IndexedDBCallbacks<WebSerializedScriptValue>(
      parent_, response_id));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnPut(
    int idb_object_store_id, int32 response_id,
    const SerializedScriptValue& value, const IndexedDBKey& key,
    bool add_only) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL, ViewHostMsg_IDBObjectStorePut::ID);
  if (!idb_object_store)
    return;
  idb_object_store->put(
      value, key, add_only, new IndexedDBCallbacks<WebIDBKey>(
          parent_, response_id));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnRemove(
    int idb_object_store_id, int32 response_id, const IndexedDBKey& key) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL, ViewHostMsg_IDBObjectStoreRemove::ID);
  if (!idb_object_store)
    return;
  idb_object_store->remove(key, new IndexedDBCallbacks<void>(parent_,
                                                             response_id));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnCreateIndex(
   const ViewHostMsg_IDBObjectStoreCreateIndex_Params& params) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, params.idb_object_store_id_, NULL,
      ViewHostMsg_IDBObjectStoreCreateIndex::ID);
  if (!idb_object_store)
    return;
  idb_object_store->createIndex(
      params.name_, params.key_path_, params.unique_,
      new IndexedDBCallbacks<WebIDBIndex>(parent_, params.response_id_));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnIndex(
    int32 idb_object_store_id, const string16& name, IPC::Message* reply_msg) {
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, reply_msg,
      ViewHostMsg_IDBObjectStoreIndex::ID);
  if (!idb_object_store)
    return;

  WebIDBIndex* index = idb_object_store->index(name);
  int32 object_id = index ? parent_->Add(index) : 0;
  ViewHostMsg_IDBObjectStoreIndex::WriteReplyParams(reply_msg, !!index,
                                                    object_id);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnRemoveIndex(
    int32 idb_object_store_id, int32 response_id, const string16& name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &map_, idb_object_store_id, NULL,
      ViewHostMsg_IDBObjectStoreRemoveIndex::ID);
  if (!idb_object_store)
    return;
  idb_object_store->removeIndex(
      name, new IndexedDBCallbacks<void>(parent_, response_id));
}

void IndexedDBDispatcherHost::ObjectStoreDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBObjectStoreDestroyed::ID);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::CursorDispatcherHost
//

IndexedDBDispatcherHost::CursorDispatcherHost::CursorDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
}

IndexedDBDispatcherHost::CursorDispatcherHost::~CursorDispatcherHost() {
}

bool IndexedDBDispatcherHost::CursorDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::CursorDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBObjectStoreOpenCursor, OnOpenCursor)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorDirection,
                                    OnDirection)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorKey, OnKey)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBCursorValue, OnValue)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBCursorDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}


void IndexedDBDispatcherHost::CursorDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnOpenCursor(
    const ViewHostMsg_IDBObjectStoreOpenCursor_Params& params) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  WebIDBObjectStore* idb_object_store = parent_->GetOrTerminateProcess(
      &parent_->object_store_dispatcher_host_->map_,
      params.idb_object_store_id_, NULL,
      ViewHostMsg_IDBObjectStoreOpenCursor::ID);
  if (!idb_object_store)
    return;
  idb_object_store->openCursor(
      WebIDBKeyRange(params.left_key_, params.right_key_, params.flags_),
      params.direction_,
      new IndexedDBCallbacks<WebIDBCursor>(parent_, params.response_id_));
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDirection(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorDirection::ID);
  if (!idb_cursor)
    return;

  int direction = idb_cursor->direction();
  ViewHostMsg_IDBCursorDirection::WriteReplyParams(reply_msg, direction);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnKey(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorKey::ID);
  if (!idb_cursor)
    return;

  IndexedDBKey key(idb_cursor->key());
  ViewHostMsg_IDBCursorKey::WriteReplyParams(reply_msg, key);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnValue(
    int32 object_id, IPC::Message* reply_msg) {
  WebIDBCursor* idb_cursor = parent_->GetOrTerminateProcess(
      &map_, object_id, reply_msg,
      ViewHostMsg_IDBCursorValue::ID);
  if (!idb_cursor)
    return;

  SerializedScriptValue value(idb_cursor->value());
  ViewHostMsg_IDBCursorValue::WriteReplyParams(reply_msg, value);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBCursorDestroyed::ID);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::TransactionDispatcherHost
//

IndexedDBDispatcherHost::TransactionDispatcherHost::TransactionDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
}

IndexedDBDispatcherHost::
    TransactionDispatcherHost::~TransactionDispatcherHost() {
}

bool IndexedDBDispatcherHost::TransactionDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* msg_is_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost::TransactionDispatcherHost,
                           message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_IDBTransactionObjectStore,
                                    OnObjectStore)
    IPC_MESSAGE_HANDLER(ViewHostMsg_IDBTransactionDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::Send(
    IPC::Message* message) {
  // The macro magic in OnMessageReceived requires this to link, but it should
  // never actually be called.
  NOTREACHED();
  parent_->Send(message);
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnObjectStore(
    int32 transaction_id, const string16& name, IPC::Message* reply_msg) {
  WebIDBTransaction* idb_transaction = parent_->GetOrTerminateProcess(
      &map_, transaction_id, reply_msg,
      ViewHostMsg_IDBDatabaseObjectStore::ID);
  if (!idb_transaction)
    return;

  WebIDBObjectStore* object_store = idb_transaction->objectStore(name);
  int32 object_id = object_store ? parent_->Add(object_store) : 0;
  ViewHostMsg_IDBTransactionObjectStore::WriteReplyParams(
         reply_msg, object_id);
  parent_->Send(reply_msg);
}

void IndexedDBDispatcherHost::TransactionDispatcherHost::OnDestroyed(
    int32 object_id) {
  parent_->DestroyObject(
      &map_, object_id, ViewHostMsg_IDBTransactionDestroyed::ID);
}
