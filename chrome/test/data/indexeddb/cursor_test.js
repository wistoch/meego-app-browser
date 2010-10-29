// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function emptyCursorSuccess()
{
  debug('Empty cursor opened successfully.')
  done();
}

function openEmptyCursor()
{
  debug('Opening an empty cursor.');
  keyRange = webkitIDBKeyRange.leftBound('InexistentKey');
  result = objectStore.openCursor(keyRange);
  result.onsuccess = emptyCursorSuccess;
  result.onerror = unexpectedErrorCallback;
}

function cursorSuccess()
{
  var cursor = event.result;
  if (cursor === null) {
    debug('Cursor reached end of range.');
    openEmptyCursor();
    return;
  }

  debug('Cursor opened successfully.');
  shouldBe("event.result.direction", "0");
  shouldBe("event.result.key", "'myKey'");
  shouldBe("event.result.value", "'myValue'");

  cursor.continue();
}

function openCursor(objectStore)
{
  debug('Opening cursor');
  var keyRange = webkitIDBKeyRange.leftBound('myKey');
  var result = objectStore.openCursor(keyRange);
  result.onsuccess = cursorSuccess;
  result.onerror = unexpectedErrorCallback;
}

function dataAddedSuccess()
{
  debug('Data added');
  openCursor(objectStore);
}

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('test');
  var result = objectStore.add('myValue', 'myKey');
  result.onsuccess = dataAddedSuccess;
  result.onerror = unexpectedErrorCallback;
}

function setVersion()
{
  debug('setVersion');
  window.db = event.result;
  var result = db.setVersion('new version');
  result.onsuccess = populateObjectStore;
  result.onerror = unexpectedErrorCallback;
}

function test()
{
  debug('Connecting to indexedDB');
  var result = webkitIndexedDB.open('name', 'description');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
