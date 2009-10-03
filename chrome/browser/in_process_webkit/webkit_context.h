// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class DOMStorageContext;
class WebKitThread;

// There's one WebKitContext per profile.  Various DispatcherHost classes
// have a pointer to the Context to store shared state.
class WebKitContext : public base::RefCountedThreadSafe<WebKitContext> {
 public:
  WebKitContext(const FilePath& data_path, bool is_incognito);

  const FilePath& data_path() const { return data_path_; }
  bool is_incognito() const { return is_incognito_; }
  DOMStorageContext* dom_storage_context() {
    return dom_storage_context_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<WebKitContext>;
  ~WebKitContext();

  // Copies of profile data that can be accessed on any thread.
  const FilePath data_path_;
  const bool is_incognito_;

  scoped_ptr<DOMStorageContext> dom_storage_context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebKitContext);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_
