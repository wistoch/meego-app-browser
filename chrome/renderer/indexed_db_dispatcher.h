// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
#define CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
#pragma once

#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"

class IndexedDBKey;
class SerializedScriptValue;

namespace WebKit {
class WebFrame;
}

// Handle the indexed db related communication for this entire renderer.
class IndexedDBDispatcher {
 public:
  IndexedDBDispatcher();
  ~IndexedDBDispatcher();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& msg);

  void RequestIndexedDatabaseOpen(
      const string16& name, const string16& description,
      WebKit::WebIDBCallbacks* callbacks, const string16& origin,
      WebKit::WebFrame* web_frame);

  void RequestIDBDatabaseCreateObjectStore(
      const string16& name, const NullableString16& key_path,
      bool auto_increment, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_database_id);

  void RequestIDBDatabaseRemoveObjectStore(
      const string16& name, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_database_id);

  void RequestIDBObjectStoreGet(
      const IndexedDBKey& key, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id);

  void RequestIDBObjectStorePut(
      const SerializedScriptValue& value, const IndexedDBKey& key,
      bool add_only, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id);

  void RequestIDBObjectStoreRemove(
      const IndexedDBKey& key, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id);

  void RequestIDBObjectStoreCreateIndex(
      const string16& name, const NullableString16& key_path, bool unique,
      WebKit::WebIDBCallbacks* callbacks, int32 idb_object_store_id);

  void RequestIDBObjectStoreRemoveIndex(
      const string16& name, WebKit::WebIDBCallbacks* callbacks,
      int32 idb_object_store_id);

 private:
  // IDBCallback message handlers.
  void OnSuccessNull(int32 response_id);
  void OnSuccessIDBDatabase(int32 response_id, int32 object_id);
  void OnSuccessIndexedDBKey(int32 response_id, const IndexedDBKey& key);
  void OnSuccessIDBObjectStore(int32 response_id, int32 object_id);
  void OnSuccessIDBIndex(int32 response_id, int32 object_id);
  void OnSuccessSerializedScriptValue(int32 response_id,
                                      const SerializedScriptValue& value);
  void OnError(int32 response_id, int code, const string16& message);

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<WebKit::WebIDBCallbacks, IDMapOwnPointer> pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

#endif  // CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
