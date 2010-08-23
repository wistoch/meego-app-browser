// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbtransaction_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransactionCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransactionCallbacks;
using WebKit::WebString;

RendererWebIDBTransactionImpl::RendererWebIDBTransactionImpl(
    int32 idb_transaction_id)
    : idb_transaction_id_(idb_transaction_id) {
}

RendererWebIDBTransactionImpl::~RendererWebIDBTransactionImpl() {
  RenderThread::current()->Send(new ViewHostMsg_IDBTransactionDestroyed(
      idb_transaction_id_));
}

int RendererWebIDBTransactionImpl::mode() const
{
  // TODO: implement
  DCHECK(false);
  return 0;
}

WebIDBObjectStore* RendererWebIDBTransactionImpl::objectStore(
    const WebString& name)
{
  // TODO: implement
  DCHECK(false);
  return 0;
}

void RendererWebIDBTransactionImpl::abort()
{
  // TODO: implement
  DCHECK(false);
}

int RendererWebIDBTransactionImpl::id() const
{
  return idb_transaction_id_;
}

void RendererWebIDBTransactionImpl::setCallbacks(
    WebIDBTransactionCallbacks* callbacks)
{
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBTransactionSetCallbacks(callbacks);
}
