// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('employees', 'id');
  shouldBe("objectStore.name", "'employees'");
  shouldBe("objectStore.keyPath", "'id'");

  shouldBe('db.name', '"name"');
  shouldBe('db.version', '"1.0"');
  shouldBe('db.objectStores.length', '1');
  shouldBe('db.objectStores[0]', '"employees"');

  debug('Deleting an object store.');
  db.removeObjectStore('employees');
  shouldBe('db.objectStores.length', '0');

  done();
}

function setVersion()
{
  debug('setVersion');
  window.db = event.result;
  var result = db.setVersion('1.0');
  result.onsuccess = populateObjectStore;
  result.onerror = unexpectedErrorCallback;
}

function test()
{
  if ('webkitIndexedDB' in window) {
    indexedDB = webkitIndexedDB;
    IDBCursor = webkitIDBCursor;
    IDBKeyRange = webkitIDBKeyRange;
    IDBTransaction = webkitIDBTransaction;
  }

  debug('Connecting to indexedDB');
  var result = indexedDB.open('name', 'description');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
