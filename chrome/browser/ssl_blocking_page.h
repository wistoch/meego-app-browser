// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_SSL_BLOCKING_PAGE_H__
#define CHROME_BROWSER_SSL_BLOCKING_PAGE_H__

#include <string>

#include "chrome/browser/ssl_manager.h"
#include "chrome/views/decision.h"

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
class SSLBlockingPage : public NotificationObserver {
 public:
  // An interface that classes that want to interact with the SSLBlockingPage
  // should implement.
  class Delegate {
   public:
    // Should return the information about the error that causes this blocking
    // page.
    virtual SSLErrorInfo GetSSLErrorInfo(SSLManager::CertError* error) = 0;

    // Notification that the user chose to reject the certificate.
    virtual void OnDenyCertificate(SSLManager::CertError* error) = 0;

    // Notification that the user chose to accept the certificate.
    virtual void OnAllowCertificate(SSLManager::CertError* error) = 0;
  };

  SSLBlockingPage(SSLManager::CertError* error,
                  Delegate* delegate);
  virtual ~SSLBlockingPage();

  void Show();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Invoked when the user clicks on proceed.
  // Warning: 'this' has been deleted when this method returns.
  void Proceed();

  // Invoked when the user clicks on "take me out of here".
  // Warning: 'this' has been deleted when this method returns.
  void DontProceed();

  // Retrieves the SSLBlockingPage if any associated with the specified
  // |tab_contents| (used by ui tests).
  static SSLBlockingPage* GetSSLBlockingPage(TabContents* tab_contents);

  // A method that sets strings in the specified dictionary from the passed
  // vector so that they can be used to resource the ssl_roadblock.html/
  // ssl_error.html files.
  // Note: there can be up to 5 strings in |extra_info|.
  static void SetExtraInfo(DictionaryValue* strings,
                           const std::vector<std::wstring>& extra_info);
 private:
   typedef std::map<TabContents*,SSLBlockingPage*> SSLBlockingPageMap;

   void NotifyDenyCertificate();

   void NotifyAllowCertificate();

  // Initializes tab_to_blocking_page_ in a thread-safe manner.  Should be
  // called before accessing tab_to_blocking_page_.
  static void InitSSLBlockingPageMap();

  // The error we represent.  We will either call CancelRequest() or
  // ContinueRequest() on this object.
  scoped_refptr<SSLManager::CertError> error_;

  // Our delegate.  It provides useful information, like the title and details
  // about this error.
  Delegate* delegate_;

  // A flag to indicate if we've notified |delegate_| of the user's decision.
  bool delegate_has_been_notified_;

  // A flag used to know whether we should remove the last navigation entry from
  // the navigation controller.
  bool remove_last_entry_;

  // The tab in which we are displayed.
  TabContents* tab_;

  // Whether we created a fake navigation entry as part of showing the
  // interstitial page.
  bool created_nav_entry_;

  // We keep a map of the various blocking pages shown as the UI tests need to
  // be able to retrieve them.
  static SSLBlockingPageMap* tab_to_blocking_page_;

  DISALLOW_EVIL_CONSTRUCTORS(SSLBlockingPage);
};


#endif  // #ifndef CHROME_BROWSER_SSL_BLOCKING_PAGE_H__