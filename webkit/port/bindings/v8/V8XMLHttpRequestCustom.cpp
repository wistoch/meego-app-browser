/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#include "ExceptionCode.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8Document.h"
#include "V8CustomBinding.h"
#include "V8HTMLDocument.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestUpload.h"

#include <wtf/Assertions.h>

namespace WebCore {

CALLBACK_FUNC_DECL(XMLHttpRequestConstructor)
{
    INC_STATS("DOM.XMLHttpRequest.Constructor");

    if (!args.IsConstructCall()) {
        V8Proxy::ThrowError(V8Proxy::TYPE_ERROR, "DOM object constructor cannot be called as a function.");
        return v8::Undefined();
    }

    // Expect no parameters.
    // Allocate a XMLHttpRequest object as its internal field.
    Document* doc = V8Proxy::retrieveFrame()->document();
    RefPtr<XMLHttpRequest> xmlHttpRequest = XMLHttpRequest::create(doc);
    V8Proxy::SetDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::XMLHTTPREQUEST), xmlHttpRequest.get());

    // Add object to the wrapper map.
    xmlHttpRequest->ref();
    V8Proxy::SetJSWrapperForActiveDOMObject(xmlHttpRequest.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

// XMLHttpRequest --------------------------------------------------------------

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xmlHttpRequest object.
static void CreateHiddenXHRDependency(v8::Local<v8::Object> xmlHttpRequest, v8::Local<v8::Value> value)
{
    ASSERT(V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUEST || V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
    v8::Local<v8::Value> cache = xmlHttpRequest->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
    if (cache->IsNull() || cache->IsUndefined()) {
        cache = v8::Array::New();
        xmlHttpRequest->SetInternalField(V8Custom::kXMLHttpRequestCacheIndex, cache);
    }

    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    cacheArray->Set(v8::Integer::New(cacheArray->Length()), value);
}

static void RemoveHiddenXHRDependency(v8::Local<v8::Object> xmlHttpRequest, v8::Local<v8::Value> value)
{
    ASSERT(V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUEST || V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
    v8::Local<v8::Value> cache = xmlHttpRequest->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
    ASSERT(cache->IsArray());
    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    for (int i = cacheArray->Length() - 1; i >= 0; i--) {
        v8::Local<v8::Value> cached = cacheArray->Get(v8::Integer::New(i));
        if (cached->StrictEquals(value)) {
            cacheArray->Delete(i);
            return;
        }
    }

    // We should only get here if we try to remove an event listener that was
    // never added.
    ASSERT_NOT_REACHED();
}

ACCESSOR_GETTER(XMLHttpRequestOnabort)
{
    INC_STATS("DOM.XMLHttpRequest.onabort._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onabort()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onabort());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnabort)
{
    INC_STATS("DOM.XMLHttpRequest.onabort._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onabort()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onabort());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequest->setOnabort(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnabort(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestOnerror)
{
    INC_STATS("DOM.XMLHttpRequest.onerror._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onerror()) {
        RefPtr<V8ObjectEventListener> listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnerror)
{
    INC_STATS("DOM.XMLHttpRequest.onerror._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onerror()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onerror());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequest->setOnerror(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnerror(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestOnload)
{
    INC_STATS("DOM.XMLHttpRequest.onload._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onload()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onload());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnload)
{
    INC_STATS("DOM.XMLHttpRequest.onload._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onload()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onload());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        xmlHttpRequest->setOnload(0);

    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnload(listener.get());
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequest.onloadstart._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onloadstart()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onloadstart());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequest.onloadstart._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onloadstart()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onloadstart());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequest->setOnloadstart(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnloadstart(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestOnprogress)
{
    INC_STATS("DOM.XMLHttpRequest.onprogress._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onprogress()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onprogress());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnprogress)
{
    INC_STATS("DOM.XMLHttpRequest.onprogress._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onprogress()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onprogress());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequest->setOnprogress(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnprogress(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestOnreadystatechange)
{
    INC_STATS("DOM.XMLHttpRequest.onreadystatechange._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (xmlHttpRequest->onreadystatechange()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onreadystatechange());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnreadystatechange)
{
    INC_STATS("DOM.XMLHttpRequest.onreadystatechange._set");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequest->onreadystatechange()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onreadystatechange());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequest->setOnreadystatechange(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequest->setOnreadystatechange(listener.get());
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestResponseText)
{
    // FIXME: This is only needed because webkit set this getter as custom.
    // So we need a custom method to avoid forking the IDL file.
    INC_STATS("DOM.XMLHttpRequest.responsetext._get");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, info.Holder());
    return v8StringOrNull(xmlHttpRequest->responseText());
}

CALLBACK_FUNC_DECL(XMLHttpRequestAddEventListener)
{
    INC_STATS("DOM.XMLHttpRequest.addEventListener()");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());

    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(args[1], false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequest->addEventListener(type, listener, useCapture);

        CreateHiddenXHRDependency(args.Holder(), args[1]);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestRemoveEventListener)
{
    INC_STATS("DOM.XMLHttpRequest.removeEventListener()");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());

    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined(); // Probably leaked.

    RefPtr<EventListener> listener = proxy->FindObjectEventListener(args[1], false);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequest->removeEventListener(type, listener.get(), useCapture);

        RemoveHiddenXHRDependency(args.Holder(), args[1]);
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestOpen)
{
    INC_STATS("DOM.XMLHttpRequest.open()");
    // Four cases:
    // open(method, url)
    // open(method, url, async)
    // open(method, url, async, user)
    // open(method, url, async, user, passwd)

    if (args.Length() < 2) {
        V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
        return v8::Undefined();
    }

    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());

    String method = toWebCoreString(args[0]);
    String urlstring = toWebCoreString(args[1]);
    V8Proxy* proxy = V8Proxy::retrieve();
    KURL url = proxy->frame()->document()->completeURL(urlstring);

    bool async = (args.Length() < 3) ? true : args[2]->BooleanValue();

    ExceptionCode ec = 0;
    String user, passwd;
    if (args.Length() >= 4 && !args[3]->IsUndefined()) {
        user = valueToStringWithNullCheck(args[3]);

        if (args.Length() >= 5 && !args[4]->IsUndefined()) {
            passwd = valueToStringWithNullCheck(args[4]);
            xmlHttpRequest->open(method, url, async, user, passwd, ec);
        } else
            xmlHttpRequest->open(method, url, async, user, ec);
    } else
        xmlHttpRequest->open(method, url, async, ec);

    if (ec) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
    }

    return v8::Undefined();
}

static bool IsDocumentType(v8::Handle<v8::Value> value)
{
    // FIXME: add other document types.
    return V8Document::HasInstance(value) || V8HTMLDocument::HasInstance(value);
}

CALLBACK_FUNC_DECL(XMLHttpRequestSend)
{
    INC_STATS("DOM.XMLHttpRequest.send()");
    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());

    ExceptionCode ec = 0;
    if (args.Length() < 1)
        xmlHttpRequest->send(ec);
    else {
        v8::Handle<v8::Value> arg = args[0];
        // FIXME: upstream handles "File" objects too.
        if (IsDocumentType(arg)) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            Document* document = V8Proxy::DOMWrapperToNode<Document>(object);
            ASSERT(document);
            xmlHttpRequest->send(document, ec);
        } else
            xmlHttpRequest->send(valueToStringWithNullCheck(arg), ec);
    }

    if (ec) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestSetRequestHeader) {
    INC_STATS("DOM.XMLHttpRequest.setRequestHeader()");
    if (args.Length() < 2) {
        V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
        return v8::Undefined();
    }

    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());
    ExceptionCode ec = 0;
    String header = toWebCoreString(args[0]);
    String value = toWebCoreString(args[1]);
    xmlHttpRequest->setRequestHeader(header, value, ec);
    if (ec) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestGetResponseHeader)
{
    INC_STATS("DOM.XMLHttpRequest.getResponseHeader()");
    if (args.Length() < 1) {
        V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
        return v8::Undefined();
    }

    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());
    ExceptionCode ec = 0;
    String header = toWebCoreString(args[0]);
    String result = xmlHttpRequest->getResponseHeader(header, ec);
    if (ec) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8StringOrNull(result);
}

CALLBACK_FUNC_DECL(XMLHttpRequestOverrideMimeType)
{
    INC_STATS("DOM.XMLHttpRequest.overrideMimeType()");
    if (args.Length() < 1) {
        V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
        return v8::Undefined();
    }

    XMLHttpRequest* xmlHttpRequest = V8Proxy::ToNativeObject<XMLHttpRequest>(V8ClassIndex::XMLHTTPREQUEST, args.Holder());
    String value = toWebCoreString(args[0]);
    xmlHttpRequest->overrideMimeType(value);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestDispatchEvent)
{
    INC_STATS("DOM.XMLHttpRequest.dispatchEvent()");
    return v8::Undefined();
}


// XMLHttpRequestUpload --------------------------------------------------------

ACCESSOR_GETTER(XMLHttpRequestUploadOnabort)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onabort._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onabort()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onabort());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnabort)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onabort._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onabort()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onabort());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnabort(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnabort(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnerror)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onerror._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onerror()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnerror)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onerror._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onerror()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onerror());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnerror(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnerror(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnload)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onload._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onload()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onload());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnload)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onload._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onload()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onload());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnload(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnload(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onloadstart()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onloadstart());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnloadstart)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onloadstart()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onloadstart());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnloadstart(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnloadstart(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnprogress)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onprogress._get");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (xmlHttpRequestUpload->onprogress()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onprogress());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnprogress)
{
    INC_STATS("DOM.XMLHttpRequestUpload.onprogress._set");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
    if (value->IsNull()) {
        if (xmlHttpRequestUpload->onprogress()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequestUpload->onprogress());
            v8::Local<v8::Object> v8Listener = listener->getListenerObject();
            RemoveHiddenXHRDependency(info.Holder(), v8Listener);
        }

        // Clear the listener.
        xmlHttpRequestUpload->setOnprogress(0);
    } else {
        XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
        V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            xmlHttpRequestUpload->setOnprogress(listener);
            CreateHiddenXHRDependency(info.Holder(), value);
        }
    }
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadAddEventListener)
{
    INC_STATS("DOM.XMLHttpRequestUpload.addEventListener()");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

    XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(args[1], false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequestUpload->addEventListener(type, listener, useCapture);

        CreateHiddenXHRDependency(args.Holder(), args[1]);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadRemoveEventListener)
{
    INC_STATS("DOM.XMLHttpRequestUpload.removeEventListener()");
    XMLHttpRequestUpload* xmlHttpRequestUpload = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

    XMLHttpRequest* xmlHttpRequest = xmlHttpRequestUpload->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlHttpRequest->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined(); // Probably leaked.

    RefPtr<EventListener> listener = proxy->FindObjectEventListener(args[1], false);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        xmlHttpRequestUpload->removeEventListener(type, listener.get(), useCapture);

        RemoveHiddenXHRDependency(args.Holder(), args[1]);
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadDispatchEvent)
{
    INC_STATS("DOM.XMLHttpRequestUpload.dispatchEvent()");
    V8Proxy::SetDOMException(NOT_SUPPORTED_ERR);
    return v8::Undefined();
}

} // namespace WebCore
