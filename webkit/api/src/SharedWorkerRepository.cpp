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

#include "config.h"

#if ENABLE(SHARED_WORKERS)

#include "SharedWorkerRepository.h"

#include "Event.h"
#include "EventNames.h"
#include "MessagePortChannel.h"
#include "SharedWorker.h"
#include "PlatformMessagePortChannel.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMessagePortChannel.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerRepository.h"
#include "WebString.h"
#include "WebURL.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"

namespace WebCore {

class Document;
using WebKit::WebMessagePortChannel;
using WebKit::WebSharedWorker;
using WebKit::WebSharedWorkerRepository;

// Callback class that keeps the Worker object alive while loads are potentially happening, and also translates load errors into error events on the worker.
class SharedWorkerScriptLoader : public RefCounted<SharedWorkerScriptLoader>, private WorkerScriptLoaderClient {
public:
    SharedWorkerScriptLoader(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, PassOwnPtr<WebSharedWorker> webWorker)
            : m_worker(worker),
              m_webWorker(webWorker),
              m_port(port)
    {
    }

    void load(const KURL&);

private:
    // WorkerScriptLoaderClient callback
    virtual void notifyFinished();

    RefPtr<SharedWorker> m_worker;
    OwnPtr<WebSharedWorker> m_webWorker;
    OwnPtr<MessagePortChannel> m_port;
    WorkerScriptLoader m_scriptLoader;
};

void SharedWorkerScriptLoader::load(const KURL& url)
{
    m_scriptLoader.loadAsynchronously(m_worker->scriptExecutionContext(), url, DenyCrossOriginRequests, this);
}

// Extracts a WebMessagePortChannel from a MessagePortChannel.
static WebMessagePortChannel* getWebPort(PassOwnPtr<MessagePortChannel> port) {
    // Extract the WebMessagePortChannel to send to the worker.
    PlatformMessagePortChannel* platformChannel = port->channel();
    WebMessagePortChannel* webPort = platformChannel->webChannelRelease();
    webPort->setClient(0);
    return webPort;
}

void SharedWorkerScriptLoader::notifyFinished()
{
    if (m_scriptLoader.failed())
        m_worker->dispatchEvent(Event::create(eventNames().errorEvent, false, true));
    else {
        m_webWorker->startWorkerContext(m_scriptLoader.url(), m_worker->scriptExecutionContext()->userAgent(m_scriptLoader.url()), m_scriptLoader.script());
        m_webWorker->connect(getWebPort(m_port.release()));
    }

    // Connect event has been sent, so free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

bool SharedWorkerRepository::isAvailable()
{
    // SharedWorkers are disabled for now until the implementation is further along.
    // TODO(atwilson): Add code to check for a runtime flag like so:
    // return commandLineFlag && WebKit::webKitClient()->sharedWorkerRepository();
    return false;
}

static WebSharedWorkerRepository::DocumentID getId(void* document)
{
    ASSERT(document);
    return reinterpret_cast<WebSharedWorkerRepository::DocumentID>(document);
}

void SharedWorkerRepository::connect(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    ScriptExecutionContext* context = worker->scriptExecutionContext();
    // No nested workers (for now) - connect() should only be called from document context.
    ASSERT(context->isDocument());
    OwnPtr<WebSharedWorker> webWorker;
    ASSERT(WebKit::webKitClient()->sharedWorkerRepository());
    webWorker = WebKit::webKitClient()->sharedWorkerRepository()->lookup(url, name, getId(context));

    if (!webWorker) {
        // Existing worker does not match this url, so return an error back to the caller.
        ec = URL_MISMATCH_ERR;
        return;
    }

    if (!webWorker->isStarted()) {
        // Need to kick off a load for the worker. The loader will connect to the worker once the script has been loaded, then free itself.
        SharedWorkerScriptLoader* loader = new SharedWorkerScriptLoader(worker, port.release(), webWorker.release());
        loader->load(url);
    } else
        webWorker->connect(getWebPort(port.release()));
}

void SharedWorkerRepository::documentDetached(Document* document)
{
    WebKit::WebSharedWorkerRepository* repo = WebKit::webKitClient()->sharedWorkerRepository();
    if (repo)
        repo->documentDetached(getId(document));
}

bool SharedWorkerRepository::hasSharedWorkers(Document* document)
{
    WebKit::WebSharedWorkerRepository* repo = WebKit::webKitClient()->sharedWorkerRepository();
    return repo && repo->hasSharedWorkers(getId(document));
}



} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)
