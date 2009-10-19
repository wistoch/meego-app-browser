/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webkit/tools/pepper_test_plugin/plugin_object.h"

#include <string>

#include "base/logging.h"
#include "webkit/tools/pepper_test_plugin/test_object.h"

NPNetscapeFuncs* browser;

namespace {

// Properties ------------------------------------------------------------------

enum {
  ID_PROPERTY_PROPERTY = 0,
  ID_PROPERTY_TEST_OBJECT,
  NUM_PROPERTY_IDENTIFIERS
};

static NPIdentifier plugin_property_identifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8* plugin_property_identifier_names[NUM_PROPERTY_IDENTIFIERS] = {
  "property",
  "testObject",
};

// Methods ---------------------------------------------------------------------

enum {
  ID_TEST_GET_PROPERTY = 0,
  NUM_METHOD_IDENTIFIERS
};

static NPIdentifier plugin_method_identifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8* plugin_method_identifier_names[NUM_METHOD_IDENTIFIERS] = {
  "testGetProperty",
};

void EnsureIdentifiersInitialized() {
  static bool identifiers_initialized = false;
  if (identifiers_initialized)
    return;

  browser->getstringidentifiers(plugin_property_identifier_names,
                                NUM_PROPERTY_IDENTIFIERS,
                                plugin_property_identifiers);
  browser->getstringidentifiers(plugin_method_identifier_names,
                                NUM_METHOD_IDENTIFIERS,
                                plugin_method_identifiers);
  identifiers_initialized = true;
}

// Helper functions ------------------------------------------------------------

std::string CreateStringFromNPVariant(const NPVariant& variant) {
  return std::string(NPVARIANT_TO_STRING(variant).UTF8Characters,
                     NPVARIANT_TO_STRING(variant).UTF8Length);
}

bool TestGetProperty(PluginObject* obj,
                     const NPVariant* args, uint32 arg_count,
                     NPVariant* result) {
  if (arg_count == 0)
    return false;

  NPObject *object;
  browser->getvalue(obj->npp(), NPNVWindowNPObject, &object);

  for (uint32 i = 0; i < arg_count; i++) {
    CHECK(NPVARIANT_IS_STRING(args[i]));
    std::string property_string = CreateStringFromNPVariant(args[i]);
    NPIdentifier property_identifier =
        browser->getstringidentifier(property_string.c_str());

    NPVariant variant;
    bool retval = browser->getproperty(obj->npp(), object, property_identifier,
                                       &variant);
    browser->releaseobject(object);

    if (!retval)
      break;

    if (i + 1 < arg_count) {
      CHECK(NPVARIANT_IS_OBJECT(variant));
      object = NPVARIANT_TO_OBJECT(variant);
    } else {
      *result = variant;
      return true;
    }
  }

  VOID_TO_NPVARIANT(*result);
  return false;
}

// Plugin class functions ------------------------------------------------------

NPObject* PluginAllocate(NPP npp, NPClass* the_class) {
  EnsureIdentifiersInitialized();
  PluginObject* newInstance = new PluginObject(npp);
  return reinterpret_cast<NPObject*>(newInstance);
}

void PluginDeallocate(NPObject* header) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
  delete plugin;
}

void PluginInvalidate(NPObject* obj) {
}

bool PluginHasMethod(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
    if (name == plugin_method_identifiers[i])
      return true;
  }
  return false;
}

bool PluginInvoke(NPObject* header,
                  NPIdentifier name,
                  const NPVariant* args, uint32 arg_count,
                  NPVariant* result) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(header);
  if (name == plugin_method_identifiers[ID_TEST_GET_PROPERTY])
    return TestGetProperty(plugin, args, arg_count, result);

  return false;
}

bool PluginInvokeDefault(NPObject* obj,
                         const NPVariant* args, uint32 arg_count,
                         NPVariant* result) {
  INT32_TO_NPVARIANT(1, *result);
  return true;
}

bool PluginHasProperty(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++) {
    if (name == plugin_property_identifiers[i])
      return true;
  }
  return false;
}

bool PluginGetProperty(NPObject* obj,
                       NPIdentifier name,
                       NPVariant* result) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
  return false;
}

bool PluginSetProperty(NPObject* obj,
                       NPIdentifier name,
                       const NPVariant* variant) {
  PluginObject* plugin = reinterpret_cast<PluginObject*>(obj);
  return false;
}

NPClass plugin_class = {
  NP_CLASS_STRUCT_VERSION,
  PluginAllocate,
  PluginDeallocate,
  PluginInvalidate,
  PluginHasMethod,
  PluginInvoke,
  PluginInvokeDefault,
  PluginHasProperty,
  PluginGetProperty,
  PluginSetProperty,
};

}  // namespace

// PluginObject ----------------------------------------------------------------

PluginObject::PluginObject(NPP npp)
    : npp_(npp),
      test_object_(browser->createobject(npp, GetTestClass())) {
}

PluginObject::~PluginObject() {
  browser->releaseobject(test_object_);
}

// static
NPClass* PluginObject::GetPluginClass() {
  return &plugin_class;
}
