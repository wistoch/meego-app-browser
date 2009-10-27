// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains URLFetcher, a wrapper around URLRequest that handles
// low-level details like thread safety, ref counting, and incremental buffer
// reading.  This is useful for callers who simply want to get the data from a
// URL and don't care about all the nitty-gritty details.

#ifndef CHROME_BROWSER_URL_FETCHER_H_
#define CHROME_BROWSER_URL_FETCHER_H_

#include "base/leak_tracker.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/net/url_fetcher_protect.h"

class GURL;
typedef std::vector<std::string> ResponseCookies;
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;

namespace net {
class HttpResponseHeaders;
}

// To use this class, create an instance with the desired URL and a pointer to
// the object to be notified when the URL has been loaded:
//   URLFetcher* fetcher = new URLFetcher("http://www.google.com", this);
//
// Then, optionally set properties on this object, like the request context or
// extra headers:
//   fetcher->SetExtraRequestHeaders("X-Foo: bar");
//
// Finally, start the request:
//   fetcher->Start();
//
// The object you supply as a delegate must inherit from URLFetcher::Delegate;
// when the fetch is completed, OnURLFetchComplete() will be called with the
// resulting status and (if applicable) HTTP response code.  From that point
// until the original URLFetcher instance is destroyed, you may examine the
// provided status and data for the URL.  (You should copy these objects if you
// need them to live longer than the URLFetcher instance.)  If the URLFetcher
// instance is destroyed before the callback happens, the fetch will be
// canceled and no callback will occur.
//
// You may create the URLFetcher instance on any thread; OnURLFetchComplete()
// will be called back on the same thread you use to create the instance.
//
//
// NOTE: By default URLFetcher requests are NOT intercepted, except when
// interception is explicitly enabled in tests.

class URLFetcher {
 public:
  enum RequestType {
    GET,
    POST,
    HEAD,
  };

  class Delegate {
   public:
    // This will be called when the URL has been fetched, successfully or not.
    // |response_code| is the HTTP response code (200, 404, etc.) if
    // applicable.  |url|, |status| and |data| are all valid until the
    // URLFetcher instance is destroyed.
    virtual void OnURLFetchComplete(const URLFetcher* source,
                                    const GURL& url,
                                    const URLRequestStatus& status,
                                    int response_code,
                                    const ResponseCookies& cookies,
                                    const std::string& data) = 0;
  };

  // URLFetcher::Create uses the currently registered Factory to create the
  // URLFetcher. Factory is intended for testing.
  class Factory {
   public:
    virtual URLFetcher* CreateURLFetcher(int id,
                                         const GURL& url,
                                         RequestType request_type,
                                         Delegate* d) = 0;
  };

  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  URLFetcher(const GURL& url, RequestType request_type, Delegate* d);

  virtual ~URLFetcher();

  // Sets the factory used by the static method Create to create a URLFetcher.
  // URLFetcher does not take ownership of |factory|. A value of NULL results
  // in a URLFetcher being created directly.
#if defined(UNIT_TEST)
  static void set_factory(Factory* factory) { factory_ = factory; }
#endif

  // Normally interception is disabled for URLFetcher, but you can use this
  // to enable it for tests. Also see the set_factory method for another way
  // of testing code that uses an URLFetcher.
  static void enable_interception_for_tests(bool enabled) {
    g_interception_enabled = enabled;
  }

  // Creates a URLFetcher, ownership returns to the caller. If there is no
  // Factory (the default) this creates and returns a new URLFetcher. See the
  // constructor for a description of the args. |id| may be used during testing
  // to identify who is creating the URLFetcher.
  static URLFetcher* Create(int id, const GURL& url, RequestType request_type,
                            Delegate* d);

  // Sets data only needed by POSTs.  All callers making POST requests should
  // call this before the request is started.  |upload_content_type| is the MIME
  // type of the content, while |upload_content| is the data to be sent (the
  // Content-Length header value will be set to the length of this data).
  void set_upload_data(const std::string& upload_content_type,
                       const std::string& upload_content);

  // Set one or more load flags as defined in net/base/load_flags.h.  Must be
  // called before the request is started.
  void set_load_flags(int load_flags);

  // Returns the current load flags.
  int load_flags() const;

  // Set extra headers on the request.  Must be called before the request
  // is started.
  void set_extra_request_headers(const std::string& extra_request_headers);

  // Set the URLRequestContext on the request.  Must be called before the
  // request is started.
  void set_request_context(URLRequestContextGetter* request_context_getter);

  // Retrieve the response headers from the request.  Must only be called after
  // the OnURLFetchComplete callback has run.
  net::HttpResponseHeaders* response_headers() const;

  // Start the request.  After this is called, you may not change any other
  // settings.
  virtual void Start();

  // Return the URL that this fetcher is processing.
  const GURL& url() const;

 protected:
  // Returns the delegate.
  Delegate* delegate() const;

 private:
  class Core;

  scoped_refptr<Core> core_;

  static Factory* factory_;

  base::LeakTracker<URLFetcher> leak_tracker_;

  static bool g_interception_enabled;

  DISALLOW_EVIL_CONSTRUCTORS(URLFetcher);
};

#endif  // CHROME_BROWSER_URL_FETCHER_H_
