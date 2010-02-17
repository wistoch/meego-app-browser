// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/common/content_settings.h"
#include "googleurl/src/gurl.h"

class HostContentSettingsMap;
class Task;

// This class is fully threadsafe.
class DatabasePermissionRequest
    : public base::RefCountedThreadSafe<DatabasePermissionRequest>,
      public CookiePromptModalDialogDelegate {
 public:
  DatabasePermissionRequest(const GURL& url,
                            const string16& database_name,
                            Task* on_allow,
                            Task* on_block,
                            HostContentSettingsMap* settings_map);
  ~DatabasePermissionRequest();

  const GURL& url() const { return url_; }
  const string16& database_name() const { return database_name_; }

  // Start the permission request process.
  void RequestPermission();

  // CookiesPromptViewDelegate methods:
  virtual void AllowSiteData(bool remember, bool session_expire);
  virtual void BlockSiteData(bool remember);

 private:
  void SendResponse(ContentSetting content_setting, bool remember);

  // The URL to get permission for.
  const GURL url_;
  const string16 database_name_;

  // Set on IO, possibly release()ed on UI, destroyed on IO or UI.
  scoped_ptr<Task> on_allow_;
  scoped_ptr<Task> on_block_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Released once we have our answer.
  scoped_refptr<DatabasePermissionRequest> self_ref_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DatabasePermissionRequest);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_
