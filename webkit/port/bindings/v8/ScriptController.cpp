// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"
#include "ScriptController.h"

#include "ChromiumBridge.h"
#include "CString.h"
#include "Document.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "Node.h"
#include "NotImplemented.h"
#include "npruntime_priv.h"
#include "NPV8Object.h"
#include "ScriptSourceCode.h"
#include "Widget.h"

#include "v8_proxy.h"
#include "v8_binding.h"
#include "V8NPObject.h"

NPRuntimeFunctions npruntime_functions = {
    NPN_GetStringIdentifier,
    NPN_GetStringIdentifiers,
    NPN_GetIntIdentifier,
    NPN_IdentifierIsString,
    NPN_UTF8FromIdentifier,
    NPN_IntFromIdentifier,
    NPN_CreateObject,
    NPN_RetainObject,
    NPN_ReleaseObject,
    NPN_Invoke,
    NPN_InvokeDefault,
    NPN_Evaluate,
    NPN_GetProperty,
    NPN_SetProperty,
    NPN_RemoveProperty,
    NPN_HasProperty,
    NPN_HasMethod,
    NPN_ReleaseVariantValue,
    NPN_SetException
};


namespace WebCore {

void ScriptController::setFlags(const char* str, int length)
{
    v8::V8::SetFlagsFromString(str, length);
}

Frame* ScriptController::retrieveActiveFrame()
{
    return V8Proxy::retrieveActiveFrame();
}

bool ScriptController::isSafeScript(Frame* target)
{
    return V8Proxy::CanAccessFrame(target, true);
}

void ScriptController::gcProtectJSWrapper(void* dom_object)
{
    V8Proxy::GCProtect(dom_object);
}

void ScriptController::gcUnprotectJSWrapper(void* dom_object)
{
    V8Proxy::GCUnprotect(dom_object);
}

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_sourceURL(0)
    , m_processingTimerCallback(false)
    , m_paused(false)
    , m_proxy(new V8Proxy(frame))
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_windowScriptNPObject(0)
#endif
{
}

ScriptController::~ScriptController()
{
}

void ScriptController::clearScriptObjects()
{
    PluginObjectMap::iterator it = m_pluginObjects.begin();
    for (; it != m_pluginObjects.end(); ++it) {
        _NPN_UnregisterObject(it->second);
        NPN_ReleaseObject(it->second);
    }
    m_pluginObjects.clear();

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(m_windowScriptNPObject);
        m_windowScriptNPObject = 0;
    }
#endif
}

void ScriptController::updateSecurityOrigin()
{
    m_proxy->updateSecurityOrigin();
}

void ScriptController::updatePlatformScriptObjects()
{
    notImplemented();
}

// Disconnect the proxy from its owner frame;
void ScriptController::disconnectFrame()
{
    m_proxy->disconnectFrame();
}

bool ScriptController::processingUserGesture() const
{
    Frame* active_frame = V8Proxy::retrieveActiveFrame();
    // No script is running, must be run by users.
    if (!active_frame)
        return true;

    V8Proxy* active_proxy = active_frame->script()->proxy();

    v8::HandleScope handle_scope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(active_frame);
    // TODO(fqian): find all cases context can be empty:
    //  1) JS is disabled;
    //  2) page is NULL;
    if (context.IsEmpty())
        return true;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Value> jsevent = global->Get(v8::String::NewSymbol("event"));
    Event* event = V8Proxy::ToNativeEvent(jsevent);

    // Based on code from kjs_bindings.cpp.
    // Note: This is more liberal than Firefox's implementation.
    if (event) {
        const AtomicString& type = event->type();
        bool event_ok =
          // mouse events
          type == eventNames().clickEvent ||
          type == eventNames().mousedownEvent ||
          type == eventNames().mouseupEvent ||
          type == eventNames().dblclickEvent ||
          // keyboard events
          type == eventNames().keydownEvent ||
          type == eventNames().keypressEvent ||
          type == eventNames().keyupEvent ||
          // other accepted events
          type == eventNames().selectEvent ||
          type == eventNames().changeEvent ||
          type == eventNames().focusEvent ||
          type == eventNames().blurEvent ||
          type == eventNames().submitEvent;

        if (event_ok)
          return true;
    } else if (active_proxy->inlineCode() && !active_proxy->timerCallback())
        // This is the <a href="javascript:window.open('...')> case -> we let it
        // through
        return true;

    // This is the <script>window.open(...)</script> case or a timer callback ->
    // block it
    return false;
}

void ScriptController::evaluateInNewContext(
    const Vector<ScriptSourceCode>& sources) {
  m_proxy->evaluateInNewContext(sources);
}

// Evaluate a script file in the environment of this proxy.
ScriptValue ScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    v8::HandleScope hs;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_proxy->frame());
    if (context.IsEmpty())
        return ScriptValue();

    v8::Context::Scope scope(context);
    v8::Local<v8::Value> obj = m_proxy->evaluate(sourceCode, NULL);

    if (obj.IsEmpty() || obj->IsUndefined())
        return ScriptValue();

    return ScriptValue(obj);
}

void ScriptController::disposeJSResult(v8::Persistent<v8::Value> result)
{
    result.Dispose();
    result.Clear();
}

PassRefPtr<EventListener> ScriptController::createInlineEventListener(
    const String& functionName, const String& code, Node* node)
{
    return m_proxy->createInlineEventListener(functionName, code, node);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> ScriptController::createSVGEventHandler(
    const String& functionName, const String& code, Node* node)
{
    return m_proxy->createSVGEventHandler(functionName, code, node);
}
#endif

void ScriptController::setEventHandlerLineno(int lineno)
{
    m_proxy->setEventHandlerLineno(lineno);
}

void ScriptController::finishedWithEvent(Event* evt)
{
    m_proxy->finishedWithEvent(evt);
}

// Create a V8 object with an interceptor of NPObjectPropertyGetter
void ScriptController::BindToWindowObject(Frame* frame, const String& key, NPObject* object)
{
    v8::HandleScope handle_scope;

    v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> value = CreateV8ObjectForNPObject(object, NULL);

    // Attach to the global object
    v8::Handle<v8::Object> global = context->Global();
    global->Set(v8String(key), value);
}

void ScriptController::collectGarbage()
{
    v8::HandleScope hs;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_proxy->frame());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    m_proxy->evaluate(ScriptSourceCode("if (window.gc) void(gc());"), NULL);
}

NPRuntimeFunctions* ScriptController::functions()
{
    return &npruntime_functions;
}


bool ScriptController::haveInterpreter() const
{
    return m_proxy->ContextInitialized();
}

bool ScriptController::isEnabled() const
{
    return m_proxy->isEnabled();
}

PassScriptInstance ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    ASSERT(widget != 0);

    if (widget->isFrameView())
        return 0;

    NPObject* npObject = ChromiumBridge::pluginScriptableObject(widget);
    if (!npObject)
        return 0;

    // Frame Memory Management for NPObjects
    // -------------------------------------
    // NPObjects are treated differently than other objects wrapped by JS.
    // NPObjects can be created either by the browser (e.g. the main
    // window object) or by the plugin (the main plugin object
    // for a HTMLEmbedElement).  Further,
    // unlike most DOM Objects, the frame is especially careful to ensure
    // NPObjects terminate at frame teardown because if a plugin leaks a
    // reference, it could leak its objects (or the browser's objects).
    //
    // The Frame maintains a list of plugin objects (m_pluginObjects)
    // which it can use to quickly find the wrapped embed object.
    //
    // Inside the NPRuntime, we've added a few methods for registering
    // wrapped NPObjects.  The purpose of the registration is because
    // javascript garbage collection is non-deterministic, yet we need to
    // be able to tear down the plugin objects immediately.  When an object
    // is registered, javascript can use it.  When the object is destroyed,
    // or when the object's "owning" object is destroyed, the object will
    // be un-registered, and the javascript engine must not use it.
    //
    // Inside the javascript engine, the engine can keep a reference to the
    // NPObject as part of its wrapper.  However, before accessing the object
    // it must consult the NPN_Registry.

    v8::Local<v8::Object> wrapper = CreateV8ObjectForNPObject(npObject, NULL);

    // Track the plugin object.  We've been given a reference to the object.
    m_pluginObjects.set(widget, npObject);

    return V8ScriptInstance::create(wrapper);
}

void ScriptController::cleanupScriptObjectsForPlugin(void* nativeHandle)
{
    PluginObjectMap::iterator it = m_pluginObjects.find(nativeHandle);
    if (it == m_pluginObjects.end())
        return;
    _NPN_UnregisterObject(it->second);
    NPN_ReleaseObject(it->second);
    m_pluginObjects.remove(it);
}

static NPObject* createNoScriptObject()
{
    notImplemented();
    return 0;
}

static NPObject* createScriptObject(Frame* frame)
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
    if (context.IsEmpty())
        return createNoScriptObject();

    v8::Context::Scope scope(context);
    DOMWindow* window = frame->domWindow();
    v8::Handle<v8::Value> global = V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, window);
    ASSERT(global->IsObject());
    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(global), window);
}

NPObject* ScriptController::windowScriptNPObject()
{
    if (m_windowScriptNPObject)
        return m_windowScriptNPObject;

    if (isEnabled()) {
        // JavaScript is enabled, so there is a JavaScript window object.
        // Return an NPObject bound to the window object.
        m_windowScriptNPObject = createScriptObject(m_frame);
        _NPN_RegisterObject(m_windowScriptNPObject, NULL);
    } else {
        // JavaScript is not enabled, so we cannot bind the NPObject to the
        // JavaScript window object.  Instead, we create an NPObject of a
        // different class, one which is not bound to a JavaScript object.
        m_windowScriptNPObject = createNoScriptObject();
    }
    return m_windowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create NPObjects when JavaScript is disabled
    if (!isEnabled())
        return createNoScriptObject();

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
    if (context.IsEmpty())
        return createNoScriptObject();
    v8::Context::Scope scope(context);

    DOMWindow* window = m_frame->domWindow();
    v8::Handle<v8::Value> v8plugin = V8Proxy::ToV8Object(V8ClassIndex::HTMLEMBEDELEMENT, plugin);
    if (!v8plugin->IsObject())
        return createNoScriptObject();

    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(v8plugin), window);
}


void ScriptController::clearWindowShell()
{
    // V8 binding expects ScriptController::clearWindowShell only be called
    // when a frame is loading a new page. V8Proxy::clearForNavigation
    // creates a new context for the new page.
    m_proxy->clearForNavigation();
}

void ScriptController::attachDebugger(void*)
{
    notImplemented();
}

void ScriptController::updateDocument()
{
    m_proxy->updateDocument();
}

}  // namespace WebCpre
