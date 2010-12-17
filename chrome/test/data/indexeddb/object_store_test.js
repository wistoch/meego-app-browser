// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function recordNotFound()
{
  debug('Removed data can no longer be found');

  debug('Retrieving an index');
  shouldBe("objectStore.index('fname_index').name", "'fname_index'");

  debug('Removing an index');
  try {
    objectStore.deleteIndex('fname_index');
  } catch(e) {
    fail(e);
  }
  done();
}

function removeSuccess()
{
  debug('Data removed');

  var result = objectStore.get(1);
  result.onsuccess = unexpectedSuccessCallback;
  result.onerror = recordNotFound;
}

function getSuccess()
{
  debug('Data retrieved');

  shouldBe("event.result.fname", "'John'");
  shouldBe("event.result.lname", "'Doe'");
  shouldBe("event.result.id", "1");

  var result = objectStore.delete(1);
  result.onsuccess = removeSuccess;
  result.onerror = unexpectedErrorCallback;
}

function addWithSameKeyFailed()
{
  debug('Adding a record with same key failed');
  shouldBe("event.code", "webkitIDBDatabaseException.CONSTRAINT_ERR");

  var result = objectStore.get(1);
  result.onsuccess = getSuccess;
  result.onerror = unexpectedErrorCallback;
}

function dataAddedSuccess()
{
  debug('Data added');

  debug('Try to add employee with same id');
  var result = objectStore.add({fname: "Tom", lname: "Jones", id: 1});
  result.onsuccess = unexpectedSuccessCallback;
  result.onerror = addWithSameKeyFailed;
}

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('employees', {keyPath: 'id'});
  shouldBe("objectStore.name", "'employees'");
  shouldBe("objectStore.keyPath", "'id'");

  objectStore.createIndex('fname_index', 'fname');
  objectStore.createIndex('lname_index', 'fname');
  debug('Created indexes');
  shouldBe("objectStore.indexNames[0]", "'fname_index'");
  shouldBe("objectStore.indexNames[1]", "'lname_index'");

  var result = objectStore.add({fname: "John", lname: "Doe", id: 1});
  result.onsuccess = dataAddedSuccess;
  result.onerror = unexpectedErrorCallback;
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
  debug('Connecting to indexedDB');
  var result = webkitIndexedDB.open('name');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
