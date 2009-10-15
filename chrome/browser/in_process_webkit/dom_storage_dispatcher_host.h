// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_

#include "base/process.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/common/dom_storage_type.h"
#include "ipc/ipc_message.h"

class DOMStorageContext;
class WebKitThread;

// This class handles the logistics of DOM Storage within the browser process.
// It mostly ferries information between IPCs and the WebKit implementations,
// but it also handles some special cases like when renderer processes die.
class DOMStorageDispatcherHost :
    public base::RefCountedThreadSafe<DOMStorageDispatcherHost> {
 public:
  // Only call the constructor from the UI thread.
  DOMStorageDispatcherHost(IPC::Message::Sender* message_sender,
      WebKitContext* webkit_context, WebKitThread* webkit_thread);

  // Only call from ResourceMessageFilter on the IO thread.
  void Init(base::ProcessHandle process_handle);

  // Only call from ResourceMessageFilter on the IO thread.  Calls self on the
  // WebKit thread in some cases.
  void Shutdown();

  // Only call from ResourceMessageFilter on the IO thread.
  bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);

  // Send a message to the renderer process associated with our
  // message_sender_ via the IO thread.  May be called from any thread.
  void Send(IPC::Message* message);

  // Only call on the WebKit thread.
  static void DispatchStorageEvent(const string16& key,
      const NullableString16& old_value, const NullableString16& new_value,
      const string16& origin, bool is_local_storage);

 private:
  friend class base::RefCountedThreadSafe<DOMStorageDispatcherHost>;
  ~DOMStorageDispatcherHost();

  // Message Handlers.
  void OnNamespaceId(DOMStorageType storage_type, IPC::Message* reply_msg);
  void OnCloneNamespaceId(int64 namespace_id, IPC::Message* reply_msg);
  void OnStorageAreaId(int64 namespace_id, const string16& origin,
                       IPC::Message* reply_msg);
  void OnLength(int64 storage_area_id, IPC::Message* reply_msg);
  void OnKey(int64 storage_area_id, unsigned index, IPC::Message* reply_msg);
  void OnGetItem(int64 storage_area_id, const string16& key,
                 IPC::Message* reply_msg);
  void OnSetItem(int64 storage_area_id, const string16& key,
                 const string16& value, IPC::Message* reply_msg);
  void OnRemoveItem(int64 storage_area_id, const string16& key);
  void OnClear(int64 storage_area_id);

  // Only call on the IO thread.
  void OnStorageEvent(const string16& key, const NullableString16& old_value,
      const NullableString16& new_value, const string16& origin,
      bool is_local_storage);

  // A shortcut for accessing our context.
  DOMStorageContext* Context() {
    DCHECK(!shutdown_);
    return webkit_context_->dom_storage_context();
  }

  // Use whenever there's a chance OnStorageEvent will be called.
  class AutoSetCurrentDispatcherHost {
   public:
    AutoSetCurrentDispatcherHost(DOMStorageDispatcherHost* dispatcher_host);
    ~AutoSetCurrentDispatcherHost();
  };

  // Only access on the WebKit thread!  Used for storage events.
  static DOMStorageDispatcherHost* current_;

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // ResourceDispatcherHost takes care of destruction.  Immutable.
  WebKitThread* webkit_thread_;

  // Only set on the IO thread.
  IPC::Message::Sender* message_sender_;

  // If we get a corrupt message from a renderer, we need to kill it using this
  // handle.
  base::ProcessHandle process_handle_;

  // Has this dispatcher ever handled a message.  If not, then we can skip
  // the entire shutdown procedure.  This is only set to true on the IO thread
  // and must be true if we're reading it on the WebKit thread.
  bool ever_used_;

  // This is set once the Shutdown routine runs on the WebKit thread.  Once
  // set, we should not process any more messages because storage_area_map_
  // and storage_namespace_map_ contain pointers to deleted objects.
  bool shutdown_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
