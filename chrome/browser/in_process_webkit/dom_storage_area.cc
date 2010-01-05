// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_area.h"

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/in_process_webkit/dom_storage_namespace.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

using WebKit::WebStorageArea;
using WebKit::WebURL;

DOMStorageArea::DOMStorageArea(const string16& origin,
                               int64 id,
                               DOMStorageNamespace* owner)
    : origin_(origin),
      id_(id),
      owner_(owner) {
  DCHECK(owner_);
}

DOMStorageArea::~DOMStorageArea() {
}

unsigned DOMStorageArea::Length() {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->length();
}

NullableString16 DOMStorageArea::Key(unsigned index) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->key(index);
}

NullableString16 DOMStorageArea::GetItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->getItem(key);
}

void DOMStorageArea::SetItem(const string16& key, const string16& value,
                             bool* quota_exception) {
  CreateWebStorageAreaIfNecessary();
  storage_area_->setItem(key, value, WebURL(), *quota_exception);
}

void DOMStorageArea::RemoveItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  storage_area_->removeItem(key, WebURL());
}

void DOMStorageArea::Clear() {
  CreateWebStorageAreaIfNecessary();
  storage_area_->clear(WebURL());
}

void DOMStorageArea::PurgeMemory() {
  storage_area_.reset();
}

void DOMStorageArea::CreateWebStorageAreaIfNecessary() {
  if (!storage_area_.get())
    storage_area_.reset(owner_->CreateWebStorageArea(origin_));
}
