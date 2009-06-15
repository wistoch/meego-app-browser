// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
#define WEBKIT_GLUE_WEBDATASOURCE_H_

#include <vector>

#include "base/string16.h"

class GURL;
class WebFrame;
class WebRequest;
class WebResponse;

namespace base {
class Time;
}

struct PasswordForm;

enum WebNavigationType {
  WebNavigationTypeLinkClicked,
  WebNavigationTypeFormSubmitted,
  WebNavigationTypeBackForward,
  WebNavigationTypeReload,
  WebNavigationTypeFormResubmitted,
  WebNavigationTypeOther
};

class WebDataSource {
 public:
  // A base class for extra data that may be associated with this datasource.
  // See Set/GetExtraData below.
  class ExtraData {
   public:
    virtual ~ExtraData() {}
  };

  virtual ~WebDataSource() {}

  // Returns a reference to the original request data that created the
  // datasource. This request will be unmodified by WebKit.
  //
  // Note that this will be a different physical object than the WebRequest
  // that was specified in the load request initiated by the embedder, but the
  // data members will be copied.
  //
  // This call will update the request with the latest information from WebKit,
  // so it is important that the caller not cache the result or keep the
  // reference across entries into WebKit.
  virtual const WebRequest& GetInitialRequest() const = 0;

  // Returns the request that was used to create this datasource. This may
  // be modified by WebKit.  This is the same as what GetInitialRequest
  // returns unless there was a redirect.
  //
  // Note that this will be a different physical object than the WebRequest
  // that was specified in the load request initiated by the embedder.
  //
  // This call will update the request with the latest information from WebKit,
  // so it is important that the caller not cache the result or keep the
  // reference across entries into WebKit.
  virtual const WebRequest& GetRequest() const = 0;

  // Returns the response associated to this datasource.
  virtual const WebResponse& GetResponse() const = 0;

  // Returns the unreachable URL for which this datasource is showing alternate
  // content.  See WebFrame::LoadAlternateHTML{ErrorPage,String}.
  virtual GURL GetUnreachableURL() const = 0;

  // Returns true if there is a non-null unreachable URL.
  virtual bool HasUnreachableURL() const = 0;

  // Returns all redirects that occurred (both client and server) before at
  // last committing the current page. This will contain one entry for each
  // intermediate URL, and one entry for the last URL (so if there are no
  // redirects, it will contain exactly the current URL, and if there is one
  // redirect, it will contain the source and destination URL).
  virtual const std::vector<GURL>& GetRedirectChain() const = 0;

  // Returns the page title.
  virtual string16 GetPageTitle() const = 0;

  // Returns the time the document was request by the user.
  virtual base::Time GetRequestTime() const = 0;

  // Sets the request time. This is used to override the default behavior
  // if the client knows more about the origination of the request than the
  // underlying mechanism could.
  virtual void SetRequestTime(base::Time time) = 0;

  // Returns the time we started loading the page. This corresponds to
  // the DidStartProvisionalLoadForFrame delegate notification.
  virtual base::Time GetStartLoadTime() const = 0;

  // Returns the time the document itself was finished loading. This corresponds
  // to the DidFinishDocumentLoadForFrame delegate notification.
  virtual base::Time GetFinishDocumentLoadTime() const = 0;

  // Returns the time all dependent resources have been loaded and onload()
  // has been called. This corresponds to the DidFinishLoadForFrame delegate
  // notification.
  virtual base::Time GetFinishLoadTime() const = 0;

  // Returns the first time a layout was performed
  virtual base::Time GetFirstLayoutTime() const = 0;

  // Returns the reason the document was loaded.
  virtual WebNavigationType GetNavigationType() const = 0;

  // Extra data associated with this datasource.  If non-null, the extra data
  // pointer will be deleted when the datasource is destroyed.  Setting the
  // extra data pointer will cause any existing non-null extra data pointer to
  // be deleted.
  virtual ExtraData* GetExtraData() const = 0;
  virtual void SetExtraData(ExtraData* extra_data) = 0;
};

#endif  // #ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
