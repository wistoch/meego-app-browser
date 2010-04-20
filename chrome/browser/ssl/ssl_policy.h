// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_H_

#include <string>

#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/common/filter_policy.h"
#include "webkit/glue/resource_type.h"

class NavigationEntry;
class SSLCertErrorHandler;
class SSLPolicyBackend;
class SSLRequestInfo;

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLPolicyBackend to actually enact
// the decisions it reaches.
//
class SSLPolicy : public SSLBlockingPage::Delegate {
 public:
  explicit SSLPolicy(SSLPolicyBackend* backend);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(SSLCertErrorHandler* handler);

  void DidDisplayInsecureContent(NavigationEntry* entry);
  void DidRunInsecureContent(NavigationEntry* entry,
                             const std::string& security_origin);

  // We have started a resource request with the given info.
  void OnRequestStarted(SSLRequestInfo* info);

  // Update the SSL information in |entry| to match the current state.
  void UpdateEntry(NavigationEntry* entry);

  SSLPolicyBackend* backend() const { return backend_; }

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLCertErrorHandler* handler);
  virtual void OnDenyCertificate(SSLCertErrorHandler* handler);
  virtual void OnAllowCertificate(SSLCertErrorHandler* handler);

 private:
  // Helper method for derived classes handling certificate errors.
  // If the error can be overridden by the user, pass overriable=true, which
  // shows a blocking page and lets the user continue or cancel the request.
  // For fatal certificate errors, pass overridable=false, which show an error
  // page.
  void OnCertErrorInternal(SSLCertErrorHandler* handler, bool overridable);

  // If the security style of |entry| has not been initialized, then initialize
  // it with the default style for its URL.
  void InitializeEntryIfNeeded(NavigationEntry* entry);

  // Mark |origin| as containing insecure content in the process with ID |pid|.
  void MarkOriginAsBroken(const std::string& origin, int pid);

  // Called after we've decided that |info| represents a request for mixed
  // content.  Updates our internal state to reflect that we've loaded |info|.
  void UpdateStateForMixedContent(SSLRequestInfo* info);

  // Called after we've decided that |info| represents a request for unsafe
  // content.  Updates our internal state to reflect that we've loaded |info|.
  void UpdateStateForUnsafeContent(SSLRequestInfo* info);

  // The backend we use to enact our decisions.
  SSLPolicyBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_H_
