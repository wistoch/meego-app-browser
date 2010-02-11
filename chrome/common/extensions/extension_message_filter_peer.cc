// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_message_filter_peer.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/webkit_glue.h"

ExtensionMessageFilterPeer::ExtensionMessageFilterPeer(
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    IPC::Message::Sender* message_sender,
    const GURL& request_url)
    : original_peer_(peer),
      message_sender_(message_sender),
      request_url_(request_url) {
}

ExtensionMessageFilterPeer::~ExtensionMessageFilterPeer() {
}

// static
ExtensionMessageFilterPeer*
ExtensionMessageFilterPeer::CreateExtensionMessageFilterPeer(
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    IPC::Message::Sender* message_sender,
    const std::string& mime_type,
    FilterPolicy::Type filter_policy,
    const GURL& request_url) {
  if (filter_policy != FilterPolicy::FILTER_EXTENSION_MESSAGES)
    return NULL;

  if (StartsWithASCII(mime_type, "text/css", false))
    return new ExtensionMessageFilterPeer(peer, message_sender, request_url);

  // Return NULL if content is not text/css or it doesn't belong to extension
  // scheme.
  return NULL;
}

void ExtensionMessageFilterPeer::OnUploadProgress(
    uint64 position, uint64 size) {
  NOTREACHED();
}

bool ExtensionMessageFilterPeer::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  NOTREACHED();
  return false;
}

void ExtensionMessageFilterPeer::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  response_info_ = info;
}

void ExtensionMessageFilterPeer::OnReceivedData(const char* data, int len) {
  data_.append(data, len);
}

void ExtensionMessageFilterPeer::OnCompletedRequest(
    const URLRequestStatus& status, const std::string& security_info) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<ExtensionMessageFilterPeer> this_deleter(this);

  // Give sub-classes a chance at altering the data.
  if (status.status() != URLRequestStatus::SUCCESS) {
    // We failed to load the resource.
    original_peer_->OnReceivedResponse(response_info_, true);
    URLRequestStatus status(URLRequestStatus::CANCELED, net::ERR_ABORTED);
    original_peer_->OnCompletedRequest(status, security_info);
    return;
  }

  ReplaceMessages();

  original_peer_->OnReceivedResponse(response_info_, true);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()));
  original_peer_->OnCompletedRequest(status, security_info);
}

GURL ExtensionMessageFilterPeer::GetURLForDebugging() const {
  return original_peer_->GetURLForDebugging();
}

void ExtensionMessageFilterPeer::ReplaceMessages() {
  if (!message_sender_ || data_.empty())
    return;

  if (!request_url_.is_valid())
    return;

  std::string extension_id = request_url_.host();
  L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    L10nMessagesMap messages;
    message_sender_->Send(new ViewHostMsg_GetExtensionMessageBundle(
        extension_id, &messages));

    // Save messages we got, even if they are empty, so we don't have to
    // ask again.
    ExtensionToL10nMessagesMap& l10n_messages_map =
        *GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = GetL10nMessagesMap(extension_id);
  }

  if (l10n_messages->empty())
    return;

  std::string error;
  if (ExtensionMessageBundle::ReplaceMessagesWithExternalDictionary(
          *l10n_messages, &data_, &error)) {
    data_.resize(data_.size());
  }
}
