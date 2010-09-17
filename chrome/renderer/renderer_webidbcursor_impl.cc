// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbcursor_impl.h"

#include "chrome/common/indexed_db_key.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/render_thread.h"

using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKey;
using WebKit::WebSerializedScriptValue;

RendererWebIDBCursorImpl::RendererWebIDBCursorImpl(int32 idb_cursor_id)
    : idb_cursor_id_(idb_cursor_id) {
}

RendererWebIDBCursorImpl::~RendererWebIDBCursorImpl() {
  RenderThread::current()->Send(new ViewHostMsg_IDBCursorDestroyed(
      idb_cursor_id_));
}

unsigned short RendererWebIDBCursorImpl::direction() const {
  int direction;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorDirection(idb_cursor_id_, &direction));
  return direction;
}

WebIDBKey RendererWebIDBCursorImpl::key() const {
  IndexedDBKey key;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorKey(idb_cursor_id_, &key));
  return key;
}

void RendererWebIDBCursorImpl::value(
    WebSerializedScriptValue& webScriptValue,
    WebIDBKey& webKey) const {
  SerializedScriptValue scriptValue;
  IndexedDBKey key;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorValue(idb_cursor_id_, &scriptValue,
                                     &key));
  // Only one or the other type should have been "returned" to us.
  DCHECK(scriptValue.is_null() != (key.type() == WebIDBKey::InvalidType));
  webScriptValue = scriptValue;
  webKey = key;
}

void RendererWebIDBCursorImpl::update(const WebSerializedScriptValue& value,
                                      WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorUpdate(
      SerializedScriptValue(value), callbacks, idb_cursor_id_);
}

void RendererWebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorContinue(
      IndexedDBKey(key), callbacks, idb_cursor_id_);
}

void RendererWebIDBCursorImpl::remove(WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorRemove(callbacks, idb_cursor_id_);
}
