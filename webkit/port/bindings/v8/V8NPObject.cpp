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

#include "v8_custom.h"
#include "v8_helpers.h"
#include "V8NPObject.h"
#include "V8NPUtils.h"
#include "NPV8Object.h"
#include "npruntime_priv.h"
#include "v8_proxy.h"
#include "V8DOMMap.h"
#include "HTMLPlugInElement.h"
#include "V8HTMLAppletElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLObjectElement.h"

using namespace WebCore;

enum InvokeFunctionType {
    INVOKE_METHOD = 1,
    INVOKE_DEFAULT = 2
};

// TODO(mbelshe): need comments.
// Params: holder could be HTMLEmbedElement or NPObject
static v8::Handle<v8::Value> NPObjectInvokeImpl(const v8::Arguments& args, InvokeFunctionType funcId)
{
    NPObject* npobject;

    // These three types are subtypes of HTMLPlugInElement.
    if (V8HTMLAppletElement::HasInstance(args.Holder()) ||
        V8HTMLEmbedElement::HasInstance(args.Holder()) ||
        V8HTMLObjectElement::HasInstance(args.Holder())) {
        // The holder object is a subtype of HTMLPlugInElement.
        HTMLPlugInElement* imp = V8Proxy::DOMWrapperToNode<HTMLPlugInElement>(args.Holder());
        ScriptInstance scriptInstance = imp->getInstance();
        if (scriptInstance)
            npobject = V8Proxy::ToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, scriptInstance->instance());
        else
          npobject = NULL;
    } else {
        // The holder object is not a subtype of HTMLPlugInElement, it
        // must be an NPObject which has three internal fields.
        if (args.Holder()->InternalFieldCount() != V8Custom::kNPObjectInternalFieldCount) {
            V8Proxy::ThrowError(V8Proxy::REFERENCE_ERROR, "NPMethod called on non-NPObject");
            return v8::Undefined();
        }
        npobject = V8Proxy::ToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, args.Holder());
    }

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npobject || !_NPN_IsAlive(npobject)) {
        V8Proxy::ThrowError(V8Proxy::REFERENCE_ERROR, "NPObject deleted");
        return v8::Undefined();
    }

    // wrap up parameters
    int argc = args.Length();
    NPVariant* npArgs = new NPVariant[argc];

    for (int i = 0; i < argc; i++)
        convertV8ObjectToNPVariant(args[i], npobject, &npArgs[i]);

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    switch (funcId) {
    case INVOKE_METHOD:
        if (npobject->_class->invoke) {
            v8::Handle<v8::String> function_name(v8::String::Cast(*args.Data()));
            NPIdentifier ident = getStringIdentifier(function_name);
            npobject->_class->invoke(npobject, ident, npArgs, argc, &result);
        }
        break;
    case INVOKE_DEFAULT:
        if (npobject->_class->invokeDefault)
            npobject->_class->invokeDefault(npobject, npArgs, argc, &result);
        // The call might be a construct call on an NPObject.
        // See http://code.google.com/p/chromium/issues/detail?id=3285
        //
        // TODO: when V8 passes in the correct flag args.is_construct_call_,
        // make a separate NPN_Construct case.
        else if (npobject->_class->construct)
            npobject->_class->construct(npobject, npArgs, argc, &result);
        break;
    default:
        break;
    }

    for (int i=0; i < argc; i++)
        NPN_ReleaseVariantValue(&npArgs[i]);
    delete[] npArgs;

    // unwrap return values
    v8::Handle<v8::Value> rv = convertNPVariantToV8Object(&result, npobject);
    NPN_ReleaseVariantValue(&result);

    return rv;
}


v8::Handle<v8::Value> NPObjectMethodHandler(const v8::Arguments& args)
{
    return NPObjectInvokeImpl(args, INVOKE_METHOD);
}


v8::Handle<v8::Value> NPObjectInvokeDefaultHandler(const v8::Arguments& args)
{
    return NPObjectInvokeImpl(args, INVOKE_DEFAULT);
}


static void WeakTemplateCallback(v8::Persistent<v8::Value> obj, void* param);

// NPIdentifier is PrivateIdentifier*.
static WeakReferenceMap<PrivateIdentifier, v8::FunctionTemplate> \
    static_template_map(&WeakTemplateCallback);

static void WeakTemplateCallback(v8::Persistent<v8::Value> obj, void* param)
{
    PrivateIdentifier* iden = static_cast<PrivateIdentifier*>(param);
    ASSERT(iden != NULL);
    ASSERT(static_template_map.contains(iden));

    static_template_map.forget(iden);
}


static v8::Handle<v8::Value> NPObjectGetProperty(v8::Local<v8::Object> self,
                                                 NPIdentifier ident,
                                                 v8::Local<v8::Value> key)
{
    NPObject* npobject = V8Proxy::ToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, self);

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npobject || !_NPN_IsAlive(npobject)) {
        V8Proxy::ThrowError(V8Proxy::REFERENCE_ERROR, "NPObject deleted");
        return v8::Handle<v8::Value>();
    }

    if (npobject->_class->hasProperty &&
        npobject->_class->hasProperty(npobject, ident) &&
        npobject->_class->getProperty) {

        NPVariant result;
        VOID_TO_NPVARIANT(result);
        if (!npobject->_class->getProperty(npobject, ident, &result))
            return v8::Handle<v8::Value>();

        v8::Handle<v8::Value> rv = convertNPVariantToV8Object(&result, npobject);
        NPN_ReleaseVariantValue(&result);
        return rv;

    } else if (key->IsString() &&
               npobject->_class->hasMethod &&
               npobject->_class->hasMethod(npobject, ident)) {

        PrivateIdentifier* id = static_cast<PrivateIdentifier*>(ident);
        v8::Persistent<v8::FunctionTemplate> desc = static_template_map.get(id);
        // Cache templates using identifier as the key.
        if (desc.IsEmpty()) {
            // Create a new template
            v8::Local<v8::FunctionTemplate> temp = v8::FunctionTemplate::New();
            temp->SetCallHandler(NPObjectMethodHandler, key);
            desc = v8::Persistent<v8::FunctionTemplate>::New(temp);
            static_template_map.set(id, desc);
        }

        // FunctionTemplate caches function for each context.
        v8::Local<v8::Function> func = desc->GetFunction();
        func->SetName(v8::Handle<v8::String>::Cast(key));
        return func;
    }

    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> NPObjectNamedPropertyGetter(v8::Local<v8::String> name,
                                                  const v8::AccessorInfo& info)
{
    NPIdentifier ident = getStringIdentifier(name);
    return NPObjectGetProperty(info.Holder(), ident, name);
}

v8::Handle<v8::Value> NPObjectIndexedPropertyGetter(uint32_t index,
                                                    const v8::AccessorInfo& info)
{
    NPIdentifier ident = NPN_GetIntIdentifier(index);
    return NPObjectGetProperty(info.Holder(), ident, v8::Number::New(index));
}

v8::Handle<v8::Value> NPObjectGetNamedProperty(v8::Local<v8::Object> self,
                                               v8::Local<v8::String> name)
{
    NPIdentifier ident = getStringIdentifier(name);
    return NPObjectGetProperty(self, ident, name);
}

v8::Handle<v8::Value> NPObjectGetIndexedProperty(v8::Local<v8::Object> self,
                                                 uint32_t index)
{
    NPIdentifier ident = NPN_GetIntIdentifier(index);
    return NPObjectGetProperty(self, ident, v8::Number::New(index));
}

static v8::Handle<v8::Value> NPObjectSetProperty(v8::Local<v8::Object> self,
                                                 NPIdentifier ident,
                                                 v8::Local<v8::Value> value)
{
    NPObject* npobject = V8Proxy::ToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, self);

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npobject || !_NPN_IsAlive(npobject)) {
      V8Proxy::ThrowError(V8Proxy::REFERENCE_ERROR, "NPObject deleted");
      return value;  // intercepted, but an exception was thrown
    }

    if (npobject->_class->hasProperty &&
        npobject->_class->hasProperty(npobject, ident) &&
        npobject->_class->setProperty) {

        NPVariant npvalue;
        VOID_TO_NPVARIANT(npvalue);
        convertV8ObjectToNPVariant(value, npobject, &npvalue);
        bool succ = npobject->_class->setProperty(npobject, ident, &npvalue);
        NPN_ReleaseVariantValue(&npvalue);
        if (succ)
            return value;  // intercept the call
    }
    return v8::Local<v8::Value>();  // do not intercept the call
}


v8::Handle<v8::Value> NPObjectNamedPropertySetter(v8::Local<v8::String> name,
                                                  v8::Local<v8::Value> value,
                                                  const v8::AccessorInfo& info)
{
    NPIdentifier ident = getStringIdentifier(name);
    return NPObjectSetProperty(info.Holder(), ident, value);
}


v8::Handle<v8::Value> NPObjectIndexedPropertySetter(uint32_t index,
                                                    v8::Local<v8::Value> value,
                                                    const v8::AccessorInfo& info)
{
    NPIdentifier ident = NPN_GetIntIdentifier(index);
    return NPObjectSetProperty(info.Holder(), ident, value);
}

v8::Handle<v8::Value> NPObjectSetNamedProperty(v8::Local<v8::Object> self,
                                               v8::Local<v8::String> name,
                                               v8::Local<v8::Value> value)
{
    NPIdentifier ident = getStringIdentifier(name);
    return NPObjectSetProperty(self, ident, value);
}

v8::Handle<v8::Value> NPObjectSetIndexedProperty(v8::Local<v8::Object> self,
                                                 uint32_t index,
                                                 v8::Local<v8::Value> value)
{
    NPIdentifier ident = NPN_GetIntIdentifier(index);
    return NPObjectSetProperty(self, ident, value);
}


static void WeakNPObjectCallback(v8::Persistent<v8::Value> obj, void* param);

static DOMWrapperMap<NPObject> staticNpobjectMap(&WeakNPObjectCallback);

static void WeakNPObjectCallback(v8::Persistent<v8::Value> obj, void* param)
{
    NPObject* npobject = static_cast<NPObject*>(param);
    ASSERT(staticNpobjectMap.contains(npobject));
    ASSERT(npobject != NULL);

    // Must remove from our map before calling NPN_ReleaseObject().
    // NPN_ReleaseObject can call ForgetV8ObjectForNPObject, which
    // uses the table as well.
    staticNpobjectMap.forget(npobject);

    if (_NPN_IsAlive(npobject))
        NPN_ReleaseObject(npobject);
}


v8::Local<v8::Object> CreateV8ObjectForNPObject(NPObject* object, NPObject* root)
{
    static v8::Persistent<v8::FunctionTemplate> npObjectDesc;

    ASSERT(v8::Context::InContext());

    // If this is a v8 object, just return it.
    if (object->_class == npScriptObjectClass) {
        V8NPObject* v8npobject = reinterpret_cast<V8NPObject*>(object);
        return v8::Local<v8::Object>::New(v8npobject->v8Object);
    }

    // If we've already wrapped this object, just return it.
    if (staticNpobjectMap.contains(object))
        return v8::Local<v8::Object>::New(staticNpobjectMap.get(object));

    // TODO: we should create a Wrapper type as a subclass of JSObject.
    // It has two internal fields, field 0 is the wrapped pointer,
    // and field 1 is the type. There should be an api function that
    // returns unused type id.
    // The same Wrapper type can be used by DOM bindings.
    if (npObjectDesc.IsEmpty()) {
        npObjectDesc = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New());
        npObjectDesc->InstanceTemplate()->SetInternalFieldCount(V8Custom::kNPObjectInternalFieldCount);
        npObjectDesc->InstanceTemplate()->SetNamedPropertyHandler(NPObjectNamedPropertyGetter, NPObjectNamedPropertySetter);
        npObjectDesc->InstanceTemplate()->SetIndexedPropertyHandler(NPObjectIndexedPropertyGetter, NPObjectIndexedPropertySetter);
        npObjectDesc->InstanceTemplate()->SetCallAsFunctionHandler(NPObjectInvokeDefaultHandler);
    }

    v8::Handle<v8::Function> func = npObjectDesc->GetFunction();
    v8::Local<v8::Object> value = SafeAllocation::NewInstance(func);
    
    // If we were unable to allocate the instance we avoid wrapping 
    // and registering the NP object. 
    if (value.IsEmpty()) 
        return value;

    WrapNPObject(value, object);

    // KJS retains the object as part of its wrapper (see Bindings::CInstance)
    NPN_RetainObject(object);

    _NPN_RegisterObject(object, root);

    // Maintain a weak pointer for v8 so we can cleanup the object.
    v8::Persistent<v8::Object> weakRef = v8::Persistent<v8::Object>::New(value);
    staticNpobjectMap.set(object, weakRef);

    return value;
}

void ForgetV8ObjectForNPObject(NPObject* object)
{
    if (staticNpobjectMap.contains(object)) {
        v8::HandleScope scope;
        v8::Persistent<v8::Object> handle(staticNpobjectMap.get(object));
        WebCore::V8Proxy::SetDOMWrapper(handle, WebCore::V8ClassIndex::NPOBJECT, NULL);
        staticNpobjectMap.forget(object);
        NPN_ReleaseObject(object);
    }
}
