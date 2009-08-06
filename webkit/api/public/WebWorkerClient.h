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

#ifndef WebWorkerClient_h
#define WebWorkerClient_h

#include "WebCommon.h"

namespace WebKit {
    class WebMessagePortChannel;
    class WebString;
    class WebWorker;

    // Provides an interface back to the in-page script object for a worker.
    // All functions are expected to be called back on the thread that created
    // the Worker object, unless noted.
    class WebWorkerClient {
    public:
        virtual void postMessageToWorkerObject(const WebString&,
                                               WebMessagePortChannel*) = 0;

        virtual void postExceptionToWorkerObject(
            const WebString& errorString, int lineNumber,
            const WebString& sourceURL) = 0;

        virtual void postConsoleMessageToWorkerObject(
            int destinationIdentifier,
            int sourceIdentifier,
            int messageType,
            int messageLevel,
            const WebString& message,
            int lineNumber,
            const WebString& sourceURL) = 0;

        virtual void confirmMessageFromWorkerObject(bool hasPendingActivity) = 0;
        virtual void reportPendingActivity(bool hasPendingActivity) = 0;

        virtual void workerContextDestroyed() = 0;

        // This can be called on any thread to create a nested worker.
        virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client) = 0;
    };

} // namespace WebKit

#endif
