// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>
#include <map>
#include <string>

#include "base/lock.h"
#include "base/scoped_comptr_win.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"

class UrlmonUrlRequest;

class UrlmonUrlRequestManager
    : public PluginUrlRequestManager,
      public PluginUrlRequestDelegate {
 public:
  // Contains the privacy information for all requests issued by this instance.
  struct PrivacyInfo {
   public:
    struct PrivacyEntry {
      PrivacyEntry() : flags(0) {}
      std::wstring policy_ref;
      int32 flags;
    };

    typedef std::map<std::wstring, PrivacyEntry> PrivacyRecords;

    PrivacyInfo() : privacy_impacted(false) {}

    bool privacy_impacted;
    PrivacyRecords privacy_records;
  };

  UrlmonUrlRequestManager();
  ~UrlmonUrlRequestManager();

  // Use specific bind context when Chrome request this url.
  // Used from ChromeActiveDocument's implementation of IPersistMoniker::Load().
  void SetInfoForUrl(const std::wstring& url,
                     IMoniker* moniker, LPBC bind_context);

  // Returns a copy of the url privacy information for this instance.
  PrivacyInfo privacy_info() {
    return privacy_info_;
  }

  virtual void AddPrivacyDataForUrl(const std::string& url,
                                    const std::string& policy_ref,
                                    int32 flags);

  // This function passes the window on which notifications are to be fired.
  void put_notification_window(HWND window) {
    notification_window_ = window;
  }

  // This function passes information on whether ChromeFrame is running in
  // privileged mode.
  void set_privileged_mode(bool privileged_mode) {
    privileged_mode_ = privileged_mode;
  }

 private:
  friend class MessageLoop;
  friend struct RunnableMethodTraits<UrlmonUrlRequestManager>;
  static bool ImplementsThreadSafeReferenceCounting() { return true; }
  void AddRef() {}
  void Release() {}

  // PluginUrlRequestManager implementation.
  virtual bool IsThreadSafe();
  virtual void StartRequest(int request_id,
                            const IPC::AutomationURLRequest& request_info);
  virtual void ReadRequest(int request_id, int bytes_to_read);
  virtual void EndRequest(int request_id);
  virtual void DownloadRequestInHost(int request_id);
  virtual void StopAll();

  virtual bool GetCookiesForUrl(int tab_handle, const GURL& url,
                                int cookie_id);
  virtual bool SetCookiesForUrl(int tab_handle, const GURL& url,
                                const std::string& cookie);

  // PluginUrlRequestDelegate implementation
  virtual void OnResponseStarted(int request_id, const char* mime_type,
                                 const char* headers, int size,
                                 base::Time last_modified,
                                 const std::string& redirect_url,
                                 int redirect_status);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id, const URLRequestStatus& status);

  // Map for (request_id <-> UrlmonUrlRequest)
  typedef std::map<int, scoped_refptr<UrlmonUrlRequest> > RequestMap;
  RequestMap request_map_;
  scoped_refptr<UrlmonUrlRequest> LookupRequest(int request_id);

  struct UrlInfo {
    void Clear() {
      url_ = GURL::EmptyGURL();
      bind_ctx_.Release();
      moniker_.Release();
    }

    void Set(const std::wstring& url, IMoniker* moniker, LPBC bc) {
      DCHECK(bind_ctx_.get() == NULL);
      DCHECK(moniker_.get() == NULL);
      url_ = GURL(url);
      moniker_ = moniker;
      bind_ctx_ = bc;
    }

    bool IsForUrl(const std::string& url) {
      return GURL(url) == url_;
    }

    GURL url_;
    ScopedComPtr<IBindCtx> bind_ctx_;
    ScopedComPtr<IMoniker> moniker_;
  } url_info_;

  bool stopping_;
  int calling_delegate_;  // re-entrancy protection (debug only check)

  PrivacyInfo privacy_info_;
  // The window to be used to fire notifications on.
  HWND notification_window_;
  // Set to true if the ChromeFrame instance is running in privileged mode.
  bool privileged_mode_;
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_H_

