// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the CookieAccountant class, which is responsible for observing
// and recording all cookie-related information generated by a particular
// IE browser session. It records and fires cookie change events, it provides
// access to session and persistent cookies.

#ifndef CEEE_IE_PLUGIN_BHO_COOKIE_ACCOUNTANT_H_
#define CEEE_IE_PLUGIN_BHO_COOKIE_ACCOUNTANT_H_

#include <string>

#include "app/win/iat_patch_function.h"
#include "base/singleton.h"
#include "base/time.h"
#include "ceee/ie/plugin/bho/cookie_events_funnel.h"
#include "net/base/cookie_monster.h"

// The class that accounts for all cookie-related activity for a single IE
// browser session context. There should only need to be one of these allocated
// per process; use ProductionCookieAccountant instead of using this class
// directly.
class CookieAccountant {
 public:
  // Patch cookie-related functions to observe IE session cookies.
  void PatchWininetFunctions();

  // Record Set-Cookie changes coming from the HTTP response headers.
  void RecordHttpResponseCookies(
      const std::string& response_headers, const base::Time& current_time);

  // An accessor for the singleton (useful for unit testing).
  static CookieAccountant* GetInstance();

  // InternetSetCookieExA function patch implementation for recording scripted
  // cookie changes.
  static DWORD WINAPI InternetSetCookieExAPatch(
      LPCSTR lpszURL, LPCSTR lpszCookieName, LPCSTR lpszCookieData,
      DWORD dwFlags, DWORD_PTR dwReserved);

  // InternetSetCookieExW function patch implementation for recording scripted
  // cookie changes.
  static DWORD WINAPI InternetSetCookieExWPatch(
      LPCWSTR lpszURL, LPCWSTR lpszCookieName, LPCWSTR lpszCookieData,
      DWORD dwFlags, DWORD_PTR dwReserved);

 protected:
  // Exposed to subclasses mainly for unit testing purposes; production code
  // should use the ProductionCookieAccountant class instead.
  CookieAccountant() {}
  virtual ~CookieAccountant();

  // Records the modification or creation of a cookie. Fires off a
  // cookies.onChanged event to Chrome Frame.
  virtual void RecordCookie(
      const std::string& url, const std::string& cookie_data,
      const base::Time& current_time);

  // Unit test seam.
  virtual CookieEventsFunnel& cookie_events_funnel() {
    return cookie_events_funnel_;
  }

  // Function patches that allow us to intercept scripted cookie changes.
  app::win::IATPatchFunction internet_set_cookie_ex_a_patch_;
  app::win::IATPatchFunction internet_set_cookie_ex_w_patch_;

  // Cached singleton instance. Useful for unit testing.
  static CookieAccountant* singleton_instance_;

 private:
  // Helper functions for extracting cookie information from a scripted cookie
  // being set, to pass to the cookie onChanged event.

  // Sets the cookie domain for a script cookie event.
  void SetScriptCookieDomain(
      const net::CookieMonster::ParsedCookie& parsed_cookie,
      cookie_api::CookieInfo* cookie);

  // Sets the cookie path for a script cookie event.
  void SetScriptCookiePath(
      const net::CookieMonster::ParsedCookie& parsed_cookie,
      cookie_api::CookieInfo* cookie);

  // Sets the cookie expiration date for a script cookie event.
  void SetScriptCookieExpirationDate(
      const net::CookieMonster::ParsedCookie& parsed_cookie,
      const base::Time& current_time,
      cookie_api::CookieInfo* cookie);

  // Sets the cookie store ID for a script cookie event.
  void SetScriptCookieStoreId(cookie_api::CookieInfo* cookie);

  // The funnel for sending cookie events to the broker.
  CookieEventsFunnel cookie_events_funnel_;

  DISALLOW_COPY_AND_ASSIGN(CookieAccountant);
};

// A singleton that initializes and keeps the CookieAccountant used by
// production code. This class is separate so that CookieAccountant can still
// be accessed for unit testing.
class ProductionCookieAccountant : public CookieAccountant,
    public Singleton<ProductionCookieAccountant> {
 private:
  // This ensures no construction is possible outside of the class itself.
  friend struct DefaultSingletonTraits<ProductionCookieAccountant>;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProductionCookieAccountant);
};

#endif  // CEEE_IE_PLUGIN_BHO_COOKIE_ACCOUNTANT_H_
