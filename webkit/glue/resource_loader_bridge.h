// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The intent of this file is to provide a type-neutral abstraction between
// Chrome and WebKit for resource loading. This pure-virtual interface is
// implemented by the embedder, which also provides a factory method Create
// to instantiate this object.
//
// One of these objects will be created by WebKit for each request. WebKit
// will own the pointer to the bridge, and will delete it when the request is
// no longer needed.
//
// In turn, the bridge's owner on the WebKit end will implement the Peer
// interface, which we will use to communicate notifications back.

#ifndef WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_
#define WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_

#include "build/build_config.h"
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_type.h"

namespace net {
class HttpResponseHeaders;
}

class FilePath;

namespace webkit_glue {

class ResourceLoaderBridge {
 public:
  // Structure used when calling ResourceLoaderBridge::Create().
  struct RequestInfo {
    RequestInfo();
    ~RequestInfo();

    // HTTP-style method name (e.g., "GET" or "POST").
    std::string method;

    // Absolute URL encoded in ASCII per the rules of RFC-2396.
    GURL url;

    // URL of the document in the top-level window, which may be checked by the
    // third-party cookie blocking policy.
    GURL first_party_for_cookies;

    // Optional parameter, a URL with similar constraints in how it must be
    // encoded as the url member.
    GURL referrer;

    std::string frame_origin;
    std::string main_frame_origin;

    // For HTTP(S) requests, the headers parameter can be a \r\n-delimited and
    // \r\n-terminated list of MIME headers.  They should be ASCII-encoded using
    // the standard MIME header encoding rules.  The headers parameter can also
    // be null if no extra request headers need to be set.
    std::string headers;

    // Composed of the values defined in url_request_load_flags.h.
    int load_flags;

    // Process id of the process making the request.
    int requestor_pid;

    // Indicates if the current request is the main frame load, a sub-frame
    // load, or a sub objects load.
    ResourceType::Type request_type;

    // Used for plugin to browser requests.
    uint32 request_context;

    // Identifies what appcache host this request is associated with.
    int appcache_host_id;

    // Used to associated the bridge with a frame's network context.
    int routing_id;
  };

  struct ResponseInfo {
    ResponseInfo();
    ~ResponseInfo();

    // The time at which the request was made that resulted in this response.
    // For cached responses, this time could be "far" in the past.
    base::Time request_time;

    // The time at which the response headers were received.  For cached
    // responses, this time could be "far" in the past.
    base::Time response_time;

    // The response headers or NULL if the URL type does not support headers.
    scoped_refptr<net::HttpResponseHeaders> headers;

    // The mime type of the response.  This may be a derived value.
    std::string mime_type;

    // The character encoding of the response or none if not applicable to the
    // response's mime type.  This may be a derived value.
    std::string charset;

    // An opaque string carrying security information pertaining to this
    // response.  This may include information about the SSL connection used.
    std::string security_info;

    // Content length if available. -1 if not available
    int64 content_length;

    // The appcache this response was loaded from, or kNoCacheId.
    int64 appcache_id;

    // The manifest url of the appcache this response was loaded from.
    // Note: this value is only populated for main resource requests.
    GURL appcache_manifest_url;

    // True if the response was delivered using SPDY.
    bool was_fetched_via_spdy;

    // True if the response was delivered after NPN is negotiated.
    bool was_npn_negotiated;

    // True if response could use alternate protocol. However, browser will
    // ignore the alternate protocol when spdy is not enabled on browser side.
    bool was_alternate_protocol_available;

    // True if the response was fetched via an explicit proxy (as opposed to a
    // transparent proxy). The proxy could be any type of proxy, HTTP or SOCKS.
    // Note: we cannot tell if a transparent proxy may have been involved.
    bool was_fetched_via_proxy;
  };

  // See the SyncLoad method declared below.  (The name of this struct is not
  // suffixed with "Info" because it also contains the response data.)
  struct SyncLoadResponse : ResponseInfo {
    SyncLoadResponse();
    ~SyncLoadResponse();

    // The response status.
    URLRequestStatus status;

    // The final URL of the response.  This may differ from the request URL in
    // the case of a server redirect.
    GURL url;

    // The response data.
    std::string data;
  };

  // Generated by the bridge. This is implemented by our custom resource loader
  // within webkit. The Peer and it's bridge should have identical lifetimes
  // as they represent each end of a communication channel.
  //
  // These callbacks mirror URLRequest::Delegate and the order and conditions
  // in which they will be called are identical. See url_request.h for more
  // information.
  class Peer {
   public:
    virtual ~Peer() {}

    // Called as upload progress is made.
    // note: only for requests with LOAD_ENABLE_UPLOAD_PROGRESS set
    virtual void OnUploadProgress(uint64 position, uint64 size) = 0;

    // Called when a redirect occurs.  The implementation may return false to
    // suppress the redirect.  The given ResponseInfo provides complete
    // information about the redirect, and new_url is the URL that will be
    // loaded if this method returns true.  If this method returns true, the
    // output parameter *has_new_first_party_for_cookies indicates whether the
    // output parameter *new_first_party_for_cookies contains the new URL that
    // should be consulted for the third-party cookie blocking policy.
    virtual bool OnReceivedRedirect(const GURL& new_url,
                                    const ResponseInfo& info,
                                    bool* has_new_first_party_for_cookies,
                                    GURL* new_first_party_for_cookies) = 0;

    // Called when response headers are available (after all redirects have
    // been followed).  |content_filtered| is set to true if the contents is
    // altered or replaced (usually for security reasons when the resource is
    // deemed unsafe).
    virtual void OnReceivedResponse(const ResponseInfo& info,
                                    bool content_filtered) = 0;

    // Called when a chunk of response data is available. This method may
    // be called multiple times or not at all if an error occurs.
    virtual void OnReceivedData(const char* data, int len) = 0;

    // Called when metadata generated by the renderer is retrieved from the
    // cache. This method may be called zero or one times.
    virtual void OnReceivedCachedMetadata(const char* data, int len) { }

    // Called when the response is complete.  This method signals completion of
    // the resource load.ff
    virtual void OnCompletedRequest(const URLRequestStatus& status,
                                    const std::string& security_info) = 0;

    // Returns the URL of the request, which allows us to display it in
    // debugging situations.
    virtual GURL GetURLForDebugging() const = 0;
  };

  // use Create() for construction, but anybody can delete at any time,
  // INCLUDING during processing of callbacks.
  virtual ~ResourceLoaderBridge();

  // Call this method to make a new instance.
  //
  // For HTTP(S) POST requests, the AppendDataToUpload and AppendFileToUpload
  // methods may be called to construct the body of the request.
  static ResourceLoaderBridge* Create(const RequestInfo& request_info);

  // Call this method before calling Start() to append a chunk of binary data
  // to the request body.  May only be used with HTTP(S) POST requests.
  virtual void AppendDataToUpload(const char* data, int data_len) = 0;

  // Call this method before calling Start() to append the contents of a file
  // to the request body.  May only be used with HTTP(S) POST requests.
  void AppendFileToUpload(const FilePath& file_path) {
    AppendFileRangeToUpload(file_path, 0, kuint64max, base::Time());
  }

  // Call this method before calling Start() to append the contents of a file
  // to the request body.  May only be used with HTTP(S) POST requests.
  virtual void AppendFileRangeToUpload(
      const FilePath& file_path,
      uint64 offset,
      uint64 length,
      const base::Time& expected_modification_time) = 0;

  // Call this method before calling Start() to assign an upload identifier to
  // this request.  This is used to enable caching of POST responses.  A value
  // of 0 implies the unspecified identifier.
  virtual void SetUploadIdentifier(int64 identifier) = 0;

  // Call this method to initiate the request.  If this method succeeds, then
  // the peer's methods will be called asynchronously to report various events.
  virtual bool Start(Peer* peer) = 0;

  // Call this method to cancel a request that is in progress.  This method
  // causes the request to immediately transition into the 'done' state. The
  // OnCompletedRequest method will be called asynchronously; this assumes
  // the peer is still valid.
  virtual void Cancel() = 0;

  // Call this method to suspend or resume a load that is in progress.  This
  // method may only be called after a successful call to the Start method.
  virtual void SetDefersLoading(bool value) = 0;

  // Call this method to load the resource synchronously (i.e., in one shot).
  // This is an alternative to the Start method.  Be warned that this method
  // will block the calling thread until the resource is fully downloaded or an
  // error occurs.  It could block the calling thread for a long time, so only
  // use this if you really need it!  There is also no way for the caller to
  // interrupt this method.  Errors are reported via the status field of the
  // response parameter.
  virtual void SyncLoad(SyncLoadResponse* response) = 0;

 protected:
  // construction must go through Create()
  ResourceLoaderBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_RESOURCE_LOADER_BRIDGE_H_
