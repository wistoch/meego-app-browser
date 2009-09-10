/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFrameClient_h
#define WebFrameClient_h

#include "WebCommon.h"
#include "WebNavigationPolicy.h"
#include "WebNavigationType.h"

namespace WebKit {
    class WebDataSource;
    class WebForm;
    class WebFrame;
    class WebMediaPlayer;
    class WebMediaPlayerClient;
    class WebPlugin;
    class WebString;
    class WebURL;
    class WebURLRequest;
    class WebURLResponse;
    class WebWorker;
    class WebWorkerClient;
    struct WebPluginParams;
    struct WebSize;
    struct WebURLError;

    class WebFrameClient {
    public:
        // Factory methods -----------------------------------------------------

        // May return null.
        virtual WebPlugin* createPlugin(WebFrame*, const WebPluginParams&) = 0;

        // May return null.
        virtual WebWorker* createWorker(WebFrame*, WebWorkerClient*) = 0;

        // May return null.
        virtual WebMediaPlayer* createMediaPlayer(WebFrame*, WebMediaPlayerClient*) = 0;


        // General notifications -----------------------------------------------

        // This frame is about to be closed.
        virtual void willClose(WebFrame*) = 0;


        // Load commands -------------------------------------------------------

        // The client should handle the navigation externally.
        virtual void loadURLExternally(
            WebFrame*, const WebURLRequest&, WebNavigationPolicy) = 0;


        // Navigational queries ------------------------------------------------

        // The client may choose to alter the navigation policy.  Otherwise,
        // defaultPolicy should just be returned.
        virtual WebNavigationPolicy decidePolicyForNavigation(
            WebFrame*, const WebURLRequest&, WebNavigationType,
            WebNavigationPolicy defaultPolicy, bool isRedirect) = 0;


        // Navigational notifications ------------------------------------------

        // A form submission is about to occur.
        virtual void willSubmitForm(WebFrame*, const WebForm&) = 0;

        // A client-side redirect will occur.  This may correspond to a <META
        // refresh> or some script activity.
        virtual void willPerformClientRedirect(
            WebFrame*, const WebURL& from, const WebURL& to,
            double interval, double fireTime) = 0;

        // A client-side redirect was cancelled.
        virtual void didCancelClientRedirect(WebFrame*) = 0;

        // A client-side redirect completed.
        virtual void didCompleteClientRedirect(WebFrame*, const WebURL& fromURL) = 0;

        // A datasource has been created for a new navigation.  The given
        // datasource will become the provisional datasource for the frame.
        virtual void didCreateDataSource(WebFrame*, WebDataSource*) = 0;

        // A new provisional load has been started.
        virtual void didStartProvisionalLoad(WebFrame*) = 0;

        // The provisional load was redirected via a HTTP 3xx response.
        virtual void didReceiveServerRedirectForProvisionalLoad(WebFrame*) = 0;

        // The provisional load failed.
        virtual void didFailProvisionalLoad(WebFrame*, const WebURLError&) = 0;

        // Notifies the client to commit data for the given frame.  The client
        // may optionally prevent default processing by setting preventDefault
        // to true before returning.  If default processing is prevented, then
        // it is up to the client to manually call commitDocumentData on the
        // WebFrame.  It is only valid to call commitDocumentData within a call
        // to didReceiveDocumentData.  If commitDocumentData is not called,
        // then an empty document will be loaded.
        virtual void didReceiveDocumentData(
            WebFrame*, const char* data, size_t length, bool& preventDefault) = 0;

        // The provisional datasource is now committed.  The first part of the
        // response body has been received, and the encoding of the response
        // body is known.
        virtual void didCommitProvisionalLoad(WebFrame*, bool isNewNavigation) = 0;

        // The window object for the frame has been cleared of any extra
        // properties that may have been set by script from the previously
        // loaded document.
        virtual void didClearWindowObject(WebFrame*) = 0;

        // The document element has been created.
        virtual void didCreateDocumentElement(WebFrame*) = 0;

        // The page title is available.
        virtual void didReceiveTitle(WebFrame*, const WebString& title) = 0;

        // The frame's document finished loading.
        virtual void didFinishDocumentLoad(WebFrame*) = 0;

        // The 'load' event was dispatched.
        virtual void didHandleOnloadEvents(WebFrame*) = 0;

        // The frame's document or one of its subresources failed to load.
        virtual void didFailLoad(WebFrame*, const WebURLError&) = 0;

        // The frame's document and all of its subresources succeeded to load.
        virtual void didFinishLoad(WebFrame*) = 0;

        // The navigation resulted in scrolling the page to a named anchor instead
        // of downloading a new document.
        virtual void didChangeLocationWithinPage(WebFrame*, bool isNewNavigation) = 0;

        // Called upon update to scroll position, document state, and other
        // non-navigational events related to the data held by WebHistoryItem.
        // WARNING: This method may be called very frequently.
        // FIXME: Enable this method.
        //virtual void didUpdateCurrentHistoryItem(WebFrame*) = 0;


        // Low-level resource notifications ------------------------------------

        // An identifier was assigned to the specified request.  The client
        // should remember this association if interested in subsequent events.
        virtual void assignIdentifierToRequest(
            WebFrame*, unsigned identifier, const WebURLRequest&) = 0;

        // A request is about to be sent out, and the client may modify it.  Request
        // is writable, and changes to the URL, for example, will change the request
        // made.  If this request is the result of a redirect, then redirectResponse
        // will be non-null and contain the response that triggered the redirect.
        virtual void willSendRequest(
            WebFrame*, unsigned identifier, WebURLRequest&,
            const WebURLResponse& redirectResponse) = 0;

        // Response headers have been received for the resource request given
        // by identifier.
        virtual void didReceiveResponse(
            WebFrame*, unsigned identifier, const WebURLResponse&) = 0;

        // The resource request given by identifier succeeded.
        virtual void didFinishResourceLoad(
            WebFrame*, unsigned identifier) = 0;

        // The resource request given by identifier failed.
        virtual void didFailResourceLoad(
            WebFrame*, unsigned identifier, const WebURLError&) = 0;

        // The specified request was satified from WebCore's memory cache.
        virtual void didLoadResourceFromMemoryCache(
            WebFrame*, const WebURLRequest&, const WebURLResponse&) = 0;


        // Script notifications ------------------------------------------------

        // Script in the page tried to allocate too much memory.
        virtual void didExhaustMemoryAvailableForScript(WebFrame*) = 0;


        // Geometry notifications ----------------------------------------------

        // The size of the content area changed.
        virtual void didChangeContentsSize(WebFrame*, const WebSize&) = 0;


        // FIXME need to add:
        // find-in-page

    protected:
        ~WebFrameClient() { }
    };

} // namespace WebKit

#endif
