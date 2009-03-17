/*
 * Copyright (C) 009 Google Inc. All rights reserved.
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


#ifndef WorkerContextExecutionProxy_h
#define WorkerContextExecutionProxy_h

#if ENABLE(WORKERS)

#include <list>
#include "v8.h"
#include "v8_index.h"
#include <wtf/HashSet.h>

namespace WebCore {

    class Event;
    class EventTarget;
    class V8EventListener;
    class WorkerContext;

    class WorkerContextExecutionProxy {
    public:
        WorkerContextExecutionProxy(WorkerContext*);
        ~WorkerContextExecutionProxy();

        // Returns a local handle of the context.
        v8::Local<v8::Context> GetContext() { return v8::Local<v8::Context>::New(m_context); }

        // Returns the dom constructor function for the given node type.
        v8::Local<v8::Function> GetConstructor(V8ClassIndex::V8WrapperType);

        // Finds or creates an event listener;
        PassRefPtr<V8EventListener> FindOrCreateEventListener(v8::Local<v8::Value> listener, bool isInline, bool findOnly);

        // Removes an event listener;
        void RemoveEventListener(V8EventListener*);

        // Track the event so that we can detach it from the JS wrapper when a worker
        // terminates. This is needed because we need to be able to dispose these
        // events and releases references to their event targets: WorkerContext.
        void TrackEvent(Event*);

        // Evaluate a script file in the current execution environment.
        v8::Local<v8::Value> evaluate(const String& str, const String& fileName, int baseLine);

        // Returns WorkerContext object.
        WorkerContext* workerContext() { return m_workerContext; }

        // Returns WorkerContextExecutionProxy object of the currently executing context.
        static WorkerContextExecutionProxy* retrieve();

        // Returns the JS wrapper of object.
        static v8::Handle<v8::Value> ToV8Object(V8ClassIndex::V8WrapperType type, void* impl);
        static v8::Handle<v8::Value> EventToV8Object(Event* event);
        static v8::Handle<v8::Value> EventTargetToV8Object(EventTarget* target);
        static v8::Handle<v8::Value> WorkerContextToV8Object(WorkerContext* wc);

    private:
        void initContextIfNeeded();
        void dispose();

        // Run an already compiled script.
        v8::Local<v8::Value> runScript(v8::Handle<v8::Script>);

        static v8::Local<v8::Object> instantiateV8Object(V8ClassIndex::V8WrapperType descType, V8ClassIndex::V8WrapperType cptrType, void* impl);

        static bool forgetV8EventObject(Event*);

        WorkerContext* m_workerContext;
        v8::Persistent<v8::Context> m_context;
        int m_recursion;

        typedef std::list<V8EventListener*> EventListenerList;
        EventListenerList m_listeners;

        typedef HashSet<Event*> EventSet;
        EventSet m_events;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerContextExecutionProxy_h
