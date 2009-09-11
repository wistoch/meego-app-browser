// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webstoragearea_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"

using WebKit::WebString;

RendererWebStorageAreaImpl::RendererWebStorageAreaImpl(
    int64 namespace_id, const WebString& origin) {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageStorageAreaId(namespace_id, origin,
                                              &storage_area_id_));
}

RendererWebStorageAreaImpl::~RendererWebStorageAreaImpl() {
}

unsigned RendererWebStorageAreaImpl::length() {
  unsigned length;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageLength(storage_area_id_, &length));
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageKey(storage_area_id_, index, &key));
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageGetItem(storage_area_id_, key, &value));
  return value;
}

void RendererWebStorageAreaImpl::setItem(const WebString& key,
                                         const WebString& value,
                                         bool& quota_exception) {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageSetItem(storage_area_id_, key, value));
}

void RendererWebStorageAreaImpl::removeItem(const WebString& key) {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageRemoveItem(storage_area_id_, key));
}

void RendererWebStorageAreaImpl::clear() {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageClear(storage_area_id_));
}
