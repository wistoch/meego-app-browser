// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbcursor_impl.h"

#include "chrome/common/render_messages.h"
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

WebSerializedScriptValue RendererWebIDBCursorImpl::value() const {
  SerializedScriptValue value;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorValue(idb_cursor_id_, &value));
  return value;
}

void RendererWebIDBCursorImpl::update(const WebSerializedScriptValue& value,
                                      WebIDBCallbacks* callback) {
  // TODO(bulach): implement this.
  NOTREACHED();
}

void RendererWebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                                WebIDBCallbacks* callback) {
  // TODO(bulach): implement this.
  NOTREACHED();
}

void RendererWebIDBCursorImpl::remove(WebIDBCallbacks* callback) {
  // TODO(bulach): implement this.
  NOTREACHED();
}
