// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/blob_dispatcher_host.h"

#include "chrome/browser/chrome_blob_storage_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

BlobDispatcherHost::BlobDispatcherHost(
    ChromeBlobStorageContext* blob_storage_context)
    : blob_storage_context_(blob_storage_context) {
}

BlobDispatcherHost::~BlobDispatcherHost() {
}

void BlobDispatcherHost::Shutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Unregister all the blob URLs that are previously registered in this
  // process.
  for (base::hash_set<std::string>::const_iterator iter = blob_urls_.begin();
       iter != blob_urls_.end(); ++iter) {
    blob_storage_context_->controller()->UnregisterBlobUrl(GURL(*iter));
  }
}

bool BlobDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                           bool* msg_is_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  *msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BlobDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterBlobUrl, OnRegisterBlobUrl)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterBlobUrlFrom, OnRegisterBlobUrlFrom)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnregisterBlobUrl, OnUnregisterBlobUrl)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BlobDispatcherHost::OnRegisterBlobUrl(
    const GURL& url, const scoped_refptr<webkit_blob::BlobData>& blob_data) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  blob_storage_context_->controller()->RegisterBlobUrl(url, blob_data);
  blob_urls_.insert(url.spec());
}

void BlobDispatcherHost::OnRegisterBlobUrlFrom(
    const GURL& url, const GURL& src_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  blob_storage_context_->controller()->RegisterBlobUrlFrom(url, src_url);
  blob_urls_.insert(src_url.spec());
}

void BlobDispatcherHost::OnUnregisterBlobUrl(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  blob_storage_context_->controller()->UnregisterBlobUrl(url);
  blob_urls_.erase(url.spec());
}
