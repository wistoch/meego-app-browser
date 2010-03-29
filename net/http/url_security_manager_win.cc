// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"

// The Windows implementation of URLSecurityManager uses WinINet/IE's
// URL security zone manager.  See the MSDN page "URL Security Zones" at
// http://msdn.microsoft.com/en-us/library/ms537021(VS.85).aspx for more
// info on the Internet Security Manager and Internet Zone Manager objects.

namespace net {

class URLSecurityManagerWin : public URLSecurityManager {
 public:
  URLSecurityManagerWin();

  // URLSecurityManager methods:
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const;

 private:
  ScopedComPtr<IInternetSecurityManager> security_manager_;
};

URLSecurityManagerWin::URLSecurityManagerWin() {
  HRESULT hr = CoInternetCreateSecurityManager(NULL,
                                               security_manager_.Receive(),
                                               NULL);
  DCHECK(SUCCEEDED(hr));
}


bool URLSecurityManagerWin::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  if (!security_manager_) {
    NOTREACHED();  // The code in the constructor failed.
    return false;
  }

  std::wstring url_w = ASCIIToWide(auth_origin.spec());
  DWORD policy = 0;
  HRESULT hr;
  hr = security_manager_->ProcessUrlAction(url_w.c_str(),
                                           URLACTION_CREDENTIALS_USE,
                                           reinterpret_cast<BYTE*>(&policy),
                                           sizeof(policy), NULL, 0,
                                           PUAF_NOUI, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "IInternetSecurityManager::ProcessUrlAction failed: " << hr;
    return false;
  }

  // Four possible policies for URLACTION_CREDENTIALS_USE.  See the MSDN page
  // "About URL Security Zones" at
  // http://msdn.microsoft.com/en-us/library/ms537183(VS.85).aspx
  switch (policy) {
    case URLPOLICY_CREDENTIALS_SILENT_LOGON_OK:
      return true;
    case URLPOLICY_CREDENTIALS_CONDITIONAL_PROMPT: {
      // This policy means "prompt the user for permission if the resource is
      // not located in the Intranet zone".  TODO(wtc): Note that it's
      // prompting for permission (to use the default credentials), as opposed
      // to prompting the user to enter a user name and password.

      // URLZONE_LOCAL_MACHINE 0
      // URLZONE_INTRANET      1
      // URLZONE_TRUSTED       2
      // URLZONE_INTERNET      3
      // URLZONE_UNTRUSTED     4
      DWORD zone = 0;
      hr = security_manager_->MapUrlToZone(url_w.c_str(), &zone, 0);
      if (FAILED(hr)) {
        LOG(ERROR) << "IInternetSecurityManager::MapUrlToZone failed: " << hr;
        return false;
      }
      return zone <= URLZONE_INTRANET;
    }
    case URLPOLICY_CREDENTIALS_MUST_PROMPT_USER:
      return false;
    case URLPOLICY_CREDENTIALS_ANONYMOUS_ONLY:
      // TODO(wtc): we should fail the authentication.
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

// static
URLSecurityManager* URLSecurityManager::Create() {
  return new URLSecurityManagerWin;
}

}  //  namespace net
