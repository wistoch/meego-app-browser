// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_

#include "base/file_path.h"
#include "base/hash_tables.h"

class StorageArea;
class StorageNamespace;
class WebKitContext;

// This is owned by WebKitContext and is all the dom storage information that's
// shared by all the ResourceMessageFilter/DOMStorageDispatcherHosts that share
// the same profile.  The specifics of responsibilities are fairly well
// documented here and in StorageNamespace and StorageArea.
class DOMStorageContext {
 public:
  explicit DOMStorageContext(WebKitContext* webkit_context);
  ~DOMStorageContext();

  // Get the local storage instance.  The pointer is owned by this class.
  StorageNamespace* LocalStorage();

  // Get a new session storage namespace (but it's still owned by this class).
  StorageNamespace* NewSessionStorage();

  // Allocate a new storage ___ id.
  int64 AllocateStorageAreaId() { return ++last_storage_area_id_; }
  int64 AllocateStorageNamespaceId() { return ++last_storage_namespace_id_; }

  // Various storage area methods.  The storage area is owned by one of the
  // namespaces that's owned by this class.
  void RegisterStorageArea(StorageArea* storage_area);
  void UnregisterStorageArea(StorageArea* storage_area);
  StorageArea* GetStorageArea(int64 id);

  // Get a namespace from an id.  What's returned is owned by this class.  The
  // caller of GetStorageNamespace must immediately register itself with the
  // returned StorageNamespace.
  void RegisterStorageNamespace(StorageNamespace* storage_namespace);
  void UnregisterStorageNamespace(StorageNamespace* storage_namespace);
  StorageNamespace* GetStorageNamespace(int64 id);

  // The special ID used for local storage.
  static const int64 kLocalStorageNamespaceId = 0;

 private:
  // The last used storage_area_id and storage_namespace_id's.
  static const int64 kFirstStorageAreaId = 1;
  int64 last_storage_area_id_;
  static const int64 kFirstStorageNamespaceId = 1;
  int64 last_storage_namespace_id_;

  // We're owned by this WebKit context.  Used while instantiating LocalStorage.
  WebKitContext* webkit_context_;

  // Maps ids to StorageAreas.  We do NOT own these objects.  StorageNamespace
  // (which does own them) will notify us when we should remove the entries.
  typedef base::hash_map<int64, StorageArea*> StorageAreaMap;
  StorageAreaMap storage_area_map_;

  // Maps ids to StorageNamespaces.  We own these objects.
  typedef base::hash_map<int64, StorageNamespace*> StorageNamespaceMap;
  StorageNamespaceMap storage_namespace_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContext);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_
