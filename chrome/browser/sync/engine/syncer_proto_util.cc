// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_proto_util.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"

using std::string;
using std::stringstream;
using syncable::BASE_VERSION;
using syncable::CTIME;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::MTIME;
using syncable::PARENT_ID;
using syncable::ScopedDirLookup;
using syncable::SyncName;

namespace browser_sync {
using sessions::SyncSession;

namespace {

// Time to backoff syncing after receiving a throttled response.
static const int kSyncDelayAfterThrottled = 2 * 60 * 60;  // 2 hours

void LogResponseProfilingData(const ClientToServerResponse& response) {
  if (response.has_profiling_data()) {
    stringstream response_trace;
    response_trace << "Server response trace:";

    if (response.profiling_data().has_user_lookup_time()) {
      response_trace << " " << "user lookup: " <<
          response.profiling_data().user_lookup_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_write_time()) {
      response_trace << " " << "meta write: " <<
          response.profiling_data().meta_data_write_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_read_time()) {
      response_trace << " " << "meta read: " <<
          response.profiling_data().meta_data_read_time() << "ms";
    }

    if (response.profiling_data().has_file_data_write_time()) {
      response_trace << " " << "file write: " <<
          response.profiling_data().file_data_write_time() << "ms";
    }

    if (response.profiling_data().has_file_data_read_time()) {
      response_trace << " " << "file read: " <<
          response.profiling_data().file_data_read_time() << "ms";
    }

    if (response.profiling_data().has_total_request_time()) {
      response_trace << " " << "total time: " <<
          response.profiling_data().total_request_time() << "ms";
    }
    LOG(INFO) << response_trace.str();
  }
}

}  // namespace


// static
bool SyncerProtoUtil::VerifyResponseBirthday(syncable::Directory* dir,
    const ClientToServerResponse* response) {

  std::string local_birthday = dir->store_birthday();

  if (local_birthday.empty()) {
    if (!response->has_store_birthday()) {
      LOG(WARNING) << "Expected a birthday on first sync.";
      return false;
    }

    LOG(INFO) << "New store birthday: " << response->store_birthday();
    dir->set_store_birthday(response->store_birthday());
    return true;
  }

  // Error situation, but we're not stuck.
  if (!response->has_store_birthday()) {
    LOG(WARNING) << "No birthday in server response?";
    return true;
  }

  if (response->store_birthday() != local_birthday) {
    LOG(WARNING) << "Birthday changed, showing syncer stuck";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::AddRequestBirthday(syncable::Directory* dir,
                                         ClientToServerMessage* msg) {
  if (!dir->store_birthday().empty()) {
    msg->set_store_birthday(dir->store_birthday());
  }
}

// static
bool SyncerProtoUtil::PostAndProcessHeaders(ServerConnectionManager* scm,
                                            ClientToServerMessage* msg,
                                            ClientToServerResponse* response) {

  std::string tx, rx;
  msg->SerializeToString(&tx);

  HttpResponse http_response;
  ServerConnectionManager::PostBufferParams params = {
    tx, &rx, &http_response
  };

  ScopedServerStatusWatcher server_status_watcher(scm, &http_response);
  if (!scm->PostBufferWithCachedAuth(&params, &server_status_watcher)) {
    LOG(WARNING) << "Error posting from syncer:" << http_response;
    return false;
  } else {
    if (response->ParseFromString(rx)) {
      // TODO(tim): This is an egregious layering violation (bug 35060).
      switch (response->error_code()) {
        case ClientToServerResponse::ACCESS_DENIED:
        case ClientToServerResponse::AUTH_INVALID:
        case ClientToServerResponse::USER_NOT_ACTIVATED:
          // Fires on ScopedServerStatusWatcher
          http_response.server_status = HttpResponse::SYNC_AUTH_ERROR;
          return false;
        default:
          return true;
      }
    }

    return false;
  }
}

// static
bool SyncerProtoUtil::PostClientToServerMessage(ClientToServerMessage* msg,
    ClientToServerResponse* response, SyncSession* session) {

  CHECK(response);

  ScopedDirLookup dir(session->context()->directory_manager(),
      session->context()->account_name());
  if (!dir.good()) {
    return false;
  }

  AddRequestBirthday(dir, msg);

  if (!PostAndProcessHeaders(session->context()->connection_manager(),
                             msg,
                             response)) {
    return false;
  }

  if (!VerifyResponseBirthday(dir, response)) {
    // TODO(ncarter): Add a unit test for the case where the syncer becomes
    // stuck due to a bad birthday.
    session->status_controller()->set_syncer_stuck(true);
    return false;
  }

  switch (response->error_code()) {
    case ClientToServerResponse::SUCCESS:
      LogResponseProfilingData(*response);
      return true;
    case ClientToServerResponse::NOT_MY_BIRTHDAY:
      LOG(WARNING) << "Server thought we had wrong birthday.";
      return false;
    case ClientToServerResponse::THROTTLED:
      LOG(WARNING) << "Client silenced by server.";
      session->delegate()->OnSilencedUntil(base::TimeTicks::Now() +
          base::TimeDelta::FromSeconds(kSyncDelayAfterThrottled));
      return false;
    case ClientToServerResponse::USER_NOT_ACTIVATED:
    case ClientToServerResponse::AUTH_INVALID:
    case ClientToServerResponse::ACCESS_DENIED:
      // WARNING: PostAndProcessHeaders contains a hack for this case.
      LOG(WARNING) << "SyncerProtoUtil: Authentication expired.";
    default:
      NOTREACHED();
      return false;
  }
}

// static
bool SyncerProtoUtil::Compare(const syncable::Entry& local_entry,
                              const SyncEntity& server_entry) {
  const std::string name = NameFromSyncEntity(server_entry);

  CHECK(local_entry.Get(ID) == server_entry.id()) <<
    " SyncerProtoUtil::Compare precondition not met.";
  CHECK(server_entry.version() == local_entry.Get(BASE_VERSION)) <<
    " SyncerProtoUtil::Compare precondition not met.";
  CHECK(!local_entry.Get(IS_UNSYNCED)) <<
    " SyncerProtoUtil::Compare precondition not met.";

  if (local_entry.Get(IS_DEL) && server_entry.deleted())
    return true;
  if (!ClientAndServerTimeMatch(local_entry.Get(CTIME), server_entry.ctime())) {
    LOG(WARNING) << "ctime mismatch";
    return false;
  }

  // These checks are somewhat prolix, but they're easier to debug than a big
  // boolean statement.
  string client_name = local_entry.Get(syncable::NON_UNIQUE_NAME);
  if (client_name != name) {
    LOG(WARNING) << "Client name mismatch";
    return false;
  }
  if (local_entry.Get(PARENT_ID) != server_entry.parent_id()) {
    LOG(WARNING) << "Parent ID mismatch";
    return false;
  }
  if (local_entry.Get(IS_DIR) != server_entry.IsFolder()) {
    LOG(WARNING) << "Dir field mismatch";
    return false;
  }
  if (local_entry.Get(IS_DEL) != server_entry.deleted()) {
    LOG(WARNING) << "Deletion mismatch";
    return false;
  }
  if (!local_entry.Get(IS_DIR) &&
      !ClientAndServerTimeMatch(local_entry.Get(MTIME),
                                server_entry.mtime())) {
    LOG(WARNING) << "mtime mismatch";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::CopyProtoBytesIntoBlob(const std::string& proto_bytes,
                                             syncable::Blob* blob) {
  syncable::Blob proto_blob(proto_bytes.begin(), proto_bytes.end());
  blob->swap(proto_blob);
}

// static
bool SyncerProtoUtil::ProtoBytesEqualsBlob(const std::string& proto_bytes,
                                           const syncable::Blob& blob) {
  if (proto_bytes.size() != blob.size())
    return false;
  return std::equal(proto_bytes.begin(), proto_bytes.end(), blob.begin());
}

// static
void SyncerProtoUtil::CopyBlobIntoProtoBytes(const syncable::Blob& blob,
                                             std::string* proto_bytes) {
  std::string blob_string(blob.begin(), blob.end());
  proto_bytes->swap(blob_string);
}

// static
const std::string& SyncerProtoUtil::NameFromSyncEntity(
    const SyncEntity& entry) {

  if (entry.has_non_unique_name()) {
      return entry.non_unique_name();
  }

  return entry.name();
}

// static
const std::string& SyncerProtoUtil::NameFromCommitEntryResponse(
    const CommitResponse_EntryResponse& entry) {

  if (entry.has_non_unique_name()) {
      return entry.non_unique_name();
  }

  return entry.name();
}

}  // namespace browser_sync
