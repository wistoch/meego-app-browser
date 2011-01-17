// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/var.h"

#include <limits>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "v8/include/v8.h"

using WebKit::WebBindings;

namespace webkit {
namespace ppapi {

namespace {

const char kInvalidObjectException[] = "Error: Invalid object";
const char kInvalidPropertyException[] = "Error: Invalid property";
const char kInvalidValueException[] = "Error: Invalid value";
const char kUnableToGetPropertyException[] = "Error: Unable to get property";
const char kUnableToSetPropertyException[] = "Error: Unable to set property";
const char kUnableToRemovePropertyException[] =
    "Error: Unable to remove property";
const char kUnableToGetAllPropertiesException[] =
    "Error: Unable to get all properties";
const char kUnableToCallMethodException[] = "Error: Unable to call method";
const char kUnableToConstructException[] = "Error: Unable to construct";

// ---------------------------------------------------------------------------
// Utilities

// Converts the given PP_Var to an NPVariant, returning true on success.
// False means that the given variant is invalid. In this case, the result
// NPVariant will be set to a void one.
//
// The contents of the PP_Var will NOT be copied, so you need to ensure that
// the PP_Var remains valid while the resultant NPVariant is in use.
bool PPVarToNPVariantNoCopy(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      STRINGN_TO_NPVARIANT(value.c_str(), value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      scoped_refptr<ObjectVar> object(ObjectVar::FromPPVar(var));
      if (!object) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      OBJECT_TO_NPVARIANT(object->np_object(), *result);
      break;
    }
    default:
      VOID_TO_NPVARIANT(*result);
      return false;
  }
  return true;
}

// ObjectAccessorTryCatch ------------------------------------------------------

// Automatically sets up a TryCatch for accessing the object identified by the
// given PP_Var. The module from the object will be used for the exception
// strings generated by the TryCatch.
//
// This will automatically retrieve the ObjectVar from the object and throw
// an exception if it's invalid. At the end of construction, if there is no
// exception, you know that there is no previously set exception, that the
// object passed in is valid and ready to use (via the object() getter), and
// that the TryCatch's module() getter is also set up properly and ready to
// use.
class ObjectAccessorTryCatch : public TryCatch {
 public:
  ObjectAccessorTryCatch(PP_Var object, PP_Var* exception)
      : TryCatch(NULL, exception),
        object_(ObjectVar::FromPPVar(object)) {
    if (!object_) {
      // No object or an invalid object was given. This means we have no module
      // to associated with the exception text, so use the magic invalid object
      // exception.
      SetInvalidObjectException();
    } else {
      // When the object is valid, we have a valid module to associate
      set_module(object_->module());
    }
  }

  ObjectVar* object() { return object_.get(); }

 protected:
  scoped_refptr<ObjectVar> object_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorTryCatch);
};

// ObjectAccessiorWithIdentifierTryCatch ---------------------------------------

// Automatically sets up a TryCatch for accessing the identifier on the given
// object. This just extends ObjectAccessorTryCatch to additionally convert
// the given identifier to an NPIdentifier and validate it, throwing an
// exception if it's invalid.
//
// At the end of construction, if there is no exception, you know that there is
// no previously set exception, that the object passed in is valid and ready to
// use (via the object() getter), that the identifier is valid and ready to
// use (via the identifier() getter), and that the TryCatch's module() getter
// is also set up properly and ready to use.
class ObjectAccessorWithIdentifierTryCatch : public ObjectAccessorTryCatch {
 public:
  ObjectAccessorWithIdentifierTryCatch(PP_Var object,
                                       PP_Var identifier,
                                       PP_Var* exception)
      : ObjectAccessorTryCatch(object, exception),
        identifier_(0) {
    if (!has_exception()) {
      identifier_ = Var::PPVarToNPIdentifier(identifier);
      if (!identifier_)
        SetException(kInvalidPropertyException);
    }
  }

  NPIdentifier identifier() const { return identifier_; }

 private:
  NPIdentifier identifier_;

  DISALLOW_COPY_AND_ASSIGN(ObjectAccessorWithIdentifierTryCatch);
};

PP_Var RunJSFunction(PP_Var scope_var,
                     const char* function_script,
                     PP_Var* argv,
                     unsigned argc,
                     PP_Var* exception) {
  TryCatch try_catch(NULL, exception);
  if (try_catch.has_exception())
    return PP_MakeUndefined();

  scoped_refptr<ObjectVar> obj = ObjectVar::FromPPVar(scope_var);
  if (!obj) {
    try_catch.SetInvalidObjectException();
    return PP_MakeUndefined();
  }

  try_catch.set_module(obj->module());

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        try_catch.SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  NPVariant function_var;
  VOID_TO_NPVARIANT(function_var);
  NPString function_string = { function_script, strlen(function_script) };
  if (!WebBindings::evaluate(NULL, obj->np_object(), &function_string,
                             &function_var)) {
    try_catch.SetException(kInvalidValueException);
    return PP_MakeUndefined();
  }
  DCHECK(NPVARIANT_IS_OBJECT(function_var));
  DCHECK(!try_catch.has_exception());

  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  PP_Var result;

  if (WebBindings::invokeDefault(NULL, NPVARIANT_TO_OBJECT(function_var),
                                 args.get(), argc, &result_var)) {
    result = Var::NPVariantToPPVar(obj->instance(), &result_var);
  } else {
    DCHECK(try_catch.has_exception());
    result = PP_MakeUndefined();
  }

  WebBindings::releaseVariantValue(&function_var);
  WebBindings::releaseVariantValue(&result_var);
  return result;
}

// PPB_Var methods -------------------------------------------------------------

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  PluginModule* module = ResourceTracker::Get()->GetModule(module_id);
  if (!module)
    return PP_MakeNull();
  return StringVar::StringToPPVar(module, data, len);
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  scoped_refptr<StringVar> str(StringVar::FromPPVar(var));
  if (!str) {
    *len = 0;
    return NULL;
  }
  *len = static_cast<uint32_t>(str->value().size());
  if (str->value().empty())
    return "";  // Don't return NULL on success.
  return str->value().data();
}

PP_Var ConvertType(PP_Instance instance,
                   struct PP_Var var,
                   PP_VarType new_type,
                   PP_Var* exception) {
  TryCatch try_catch(NULL, exception);
  if (try_catch.has_exception())
    return PP_MakeUndefined();

  if (var.type == new_type)
    return var;

  PluginInstance* plugin_instance =
      ResourceTracker::Get()->GetInstance(instance);
  if (!plugin_instance) {
    try_catch.SetInvalidObjectException();
    return PP_MakeUndefined();
  }

  try_catch.set_module(plugin_instance->module());
  PP_Var object = plugin_instance->GetWindowObject();

  PP_Var params[] = {
    var,
    PP_MakeInt32(new_type),
    PP_MakeInt32(PP_VARTYPE_NULL),
    PP_MakeInt32(PP_VARTYPE_BOOL),
    PP_MakeInt32(PP_VARTYPE_INT32),
    PP_MakeInt32(PP_VARTYPE_DOUBLE),
    PP_MakeInt32(PP_VARTYPE_STRING),
    PP_MakeInt32(PP_VARTYPE_OBJECT)
  };
  PP_Var result = RunJSFunction(object,
      "(function(v, new_type, type_null, type_bool, type_int32, type_double,"
      "          type_string, type_object) {"
      "  switch(new_type) {"
      "    case type_null: return null;"
      "    case type_bool: return Boolean(v);"
      "    case type_int32: case type_double: return Number(v);"
      "    case type_string: return String(v);"
      "    case type_object: return Object(v);"
      "    default: return undefined;"
      "  }})",
      params, sizeof(params) / sizeof(PP_Var), exception);

  // Massage Number into the correct type.
  if (new_type == PP_VARTYPE_INT32 && result.type == PP_VARTYPE_DOUBLE) {
    double value = result.value.as_double;
    // Exclusive test wouldn't deal with NaNs correctly.
    if (value >= std::numeric_limits<int32_t>::max()
        && value <= std::numeric_limits<int32_t>::min())
      result = PP_MakeInt32(static_cast<int32_t>(value));
    else
      result = PP_MakeInt32(0);
  } else if (new_type == PP_VARTYPE_DOUBLE && result.type == PP_VARTYPE_INT32) {
    result = PP_MakeDouble(result.value.as_int);
  }

  Var::PluginReleasePPVar(object);
  return result;
}

PP_Var BoolToPPVar(bool value) {
  return PP_MakeBool(BoolToPPBool(value));
}

void DefineProperty(struct PP_Var object,
                    struct PP_ObjectProperty property,
                    PP_Var* exception) {
  PP_Var params[] = {
    object, property.name,
    BoolToPPVar(!!(property.modifiers & PP_OBJECTPROPERTY_MODIFIER_HASVALUE)),
    property.value,
    BoolToPPVar(property.getter.type == PP_VARTYPE_OBJECT),
    property.getter,
    BoolToPPVar(property.setter.type == PP_VARTYPE_OBJECT),
    property.setter,
    BoolToPPVar(!!(property.modifiers & PP_OBJECTPROPERTY_MODIFIER_READONLY)),
    BoolToPPVar(!!(property.modifiers & PP_OBJECTPROPERTY_MODIFIER_DONTDELETE)),
    BoolToPPVar(!!(property.modifiers & PP_OBJECTPROPERTY_MODIFIER_DONTENUM))
  };

  RunJSFunction(object,
      "(function(o, name,"
      "          has_value,  value,"
      "          has_getter, getter,"
      "          has_setter, setter,"
      "          modifier_readonly, modifier_dontdelete, modifier_dontenum) {"
      "  prop = { 'enumerable':   !modifier_dontenum,"
      "           'configurable': !modifier_dontdelete };"
      "  if (has_value && !modifier_readonly) prop.writable = true;"
      "  if (has_value)                       prop.value    = value;"
      "  if (has_getter)                      prop.get      = getter;"
      "  if (has_setter)                      prop.set      = setter;"
      "  return Object.defineProperty(o, name, prop); })",
      params, sizeof(params) / sizeof(PP_Var), exception);
}

PP_Bool HasProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_FALSE;
  return BoolToPPBool(WebBindings::hasProperty(NULL,
                                               accessor.object()->np_object(),
                                               accessor.identifier()));
}

bool HasPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var* exception) {
  return PPBoolToBool(HasProperty(var, name, exception));
}

bool HasMethodDeprecated(PP_Var var,
                         PP_Var name,
                         PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return false;
  return WebBindings::hasMethod(NULL, accessor.object()->np_object(),
                                accessor.identifier());
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  NPVariant result;
  if (!WebBindings::getProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToGetPropertyException);
    return PP_MakeUndefined();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.object()->instance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  *properties = NULL;
  *property_count = 0;

  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return;

  NPIdentifier* identifiers = NULL;
  uint32_t count = 0;
  if (!WebBindings::enumerate(NULL, accessor.object()->np_object(),
                              &identifiers, &count)) {
    accessor.SetException(kUnableToGetAllPropertiesException);
    return;
  }

  if (count == 0)
    return;

  *property_count = count;
  *properties = static_cast<PP_Var*>(malloc(sizeof(PP_Var) * count));
  for (uint32_t i = 0; i < count; ++i) {
    (*properties)[i] = Var::NPIdentifierToPPVar(
        accessor.object()->instance()->module(),
        identifiers[i]);
  }
  free(identifiers);
}

void SetPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var value,
                           PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  NPVariant variant;
  if (!PPVarToNPVariantNoCopy(value, &variant)) {
    accessor.SetException(kInvalidValueException);
    return;
  }
  if (!WebBindings::setProperty(NULL, accessor.object()->np_object(),
                                accessor.identifier(), &variant))
    accessor.SetException(kUnableToSetPropertyException);
}

PP_Bool DeleteProperty(PP_Var var,
                       PP_Var name,
                       PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return PP_FALSE;

  return BoolToPPBool(
      WebBindings::removeProperty(NULL,
                                  accessor.object()->np_object(),
                                  accessor.identifier()));
}

void DeletePropertyDeprecated(PP_Var var,
                              PP_Var name,
                              PP_Var* exception) {
  ObjectAccessorWithIdentifierTryCatch accessor(var, name, exception);
  if (accessor.has_exception())
    return;

  if (!WebBindings::removeProperty(NULL, accessor.object()->np_object(),
                                   accessor.identifier()))
    accessor.SetException(kUnableToRemovePropertyException);
}

PP_Bool IsCallable(struct PP_Var object) {
  PP_Var result = RunJSFunction(object,
      "(function() { return typeof(this) == 'function' })", NULL, 0, NULL);
  if (result.type == PP_VARTYPE_BOOL)
    return result.value.as_bool;
  return PP_FALSE;
}

struct PP_Var Call(struct PP_Var object,
                   struct PP_Var this_object,
                   uint32_t argc,
                   struct PP_Var* argv,
                   struct PP_Var* exception) {
  ObjectAccessorTryCatch accessor(object, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  NPVariant result;
  if (!WebBindings::invokeDefault(NULL, accessor.object()->np_object(),
                                  args.get(), argc, &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToCallMethodException);
    return PP_MakeUndefined();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.object()->instance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var CallDeprecated(PP_Var var,
                      PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  NPIdentifier identifier;
  if (method_name.type == PP_VARTYPE_UNDEFINED) {
    identifier = NULL;
  } else if (method_name.type == PP_VARTYPE_STRING) {
    // Specifically allow only string functions to be called.
    identifier = Var::PPVarToNPIdentifier(method_name);
    if (!identifier) {
      accessor.SetException(kInvalidPropertyException);
      return PP_MakeUndefined();
    }
  } else {
    accessor.SetException(kInvalidPropertyException);
    return PP_MakeUndefined();
  }

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  bool ok;

  NPVariant result;
  if (identifier) {
    ok = WebBindings::invoke(NULL, accessor.object()->np_object(),
                             identifier, args.get(), argc, &result);
  } else {
    ok = WebBindings::invokeDefault(NULL, accessor.object()->np_object(),
                                    args.get(), argc, &result);
  }

  if (!ok) {
    // An exception may have been raised.
    accessor.SetException(kUnableToCallMethodException);
    return PP_MakeUndefined();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.object()->instance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var Construct(PP_Var var,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjectAccessorTryCatch accessor(var, exception);
  if (accessor.has_exception())
    return PP_MakeUndefined();

  scoped_array<NPVariant> args;
  if (argc) {
    args.reset(new NPVariant[argc]);
    for (uint32_t i = 0; i < argc; ++i) {
      if (!PPVarToNPVariantNoCopy(argv[i], &args[i])) {
        // This argument was invalid, throw an exception & give up.
        accessor.SetException(kInvalidValueException);
        return PP_MakeUndefined();
      }
    }
  }

  NPVariant result;
  if (!WebBindings::construct(NULL, accessor.object()->np_object(),
                              args.get(), argc, &result)) {
    // An exception may have been raised.
    accessor.SetException(kUnableToConstructException);
    return PP_MakeUndefined();
  }

  PP_Var ret = Var::NPVariantToPPVar(accessor.object()->instance(), &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

bool IsInstanceOfDeprecated(PP_Var var,
                            const PPP_Class_Deprecated* ppp_class,
                            void** ppp_class_data) {
  scoped_refptr<ObjectVar> object(ObjectVar::FromPPVar(var));
  if (!object)
    return false;  // Not an object at all.

  return PluginObject::IsInstanceOf(object->np_object(),
                                    ppp_class, ppp_class_data);
}

PP_Var CreateObjectDeprecated(PP_Instance instance_id,
                              const PPP_Class_Deprecated* ppp_class,
                              void* ppp_class_data) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance) {
    DLOG(ERROR) << "Create object passed an invalid instance.";
    return PP_MakeNull();
  }
  return PluginObject::Create(instance, ppp_class, ppp_class_data);
}

PP_Var CreateObjectWithModuleDeprecated(PP_Module module_id,
                                        const PPP_Class_Deprecated* ppp_class,
                                        void* ppp_class_data) {
  PluginModule* module = ResourceTracker::Get()->GetModule(module_id);
  if (!module)
    return PP_MakeNull();
  return PluginObject::Create(module->GetSomeInstance(),
                              ppp_class, ppp_class_data);
}

const PPB_Var_Deprecated var_deprecated_interface = {
  &Var::PluginAddRefPPVar,
  &Var::PluginReleasePPVar,
  &VarFromUtf8,
  &VarToUtf8,
  &HasPropertyDeprecated,
  &HasMethodDeprecated,
  &GetProperty,
  &EnumerateProperties,
  &SetPropertyDeprecated,
  &DeletePropertyDeprecated,
  &CallDeprecated,
  &Construct,
  &IsInstanceOfDeprecated,
  &CreateObjectDeprecated,
  &CreateObjectWithModuleDeprecated,
};

const PPB_Var var_interface = {
  &Var::PluginAddRefPPVar,
  &Var::PluginReleasePPVar,
  &VarFromUtf8,
  &VarToUtf8,
  &ConvertType,
  &DefineProperty,
  &HasProperty,
  &GetProperty,
  &DeleteProperty,
  &EnumerateProperties,
  &IsCallable,
  &Call,
  &Construct,
};


}  // namespace

// Var -------------------------------------------------------------------------

Var::Var(PluginModule* module) : module_(module), var_id_(0) {
}

Var::~Var() {
}

// static
PP_Var Var::NPVariantToPPVar(PluginInstance* instance,
                             const NPVariant* variant) {
  switch (variant->type) {
    case NPVariantType_Void:
      return PP_MakeUndefined();
    case NPVariantType_Null:
      return PP_MakeNull();
    case NPVariantType_Bool:
      return BoolToPPVar(NPVARIANT_TO_BOOLEAN(*variant));
    case NPVariantType_Int32:
      return PP_MakeInt32(NPVARIANT_TO_INT32(*variant));
    case NPVariantType_Double:
      return PP_MakeDouble(NPVARIANT_TO_DOUBLE(*variant));
    case NPVariantType_String:
      return StringVar::StringToPPVar(
          instance->module(),
          NPVARIANT_TO_STRING(*variant).UTF8Characters,
          NPVARIANT_TO_STRING(*variant).UTF8Length);
    case NPVariantType_Object:
      return ObjectVar::NPObjectToPPVar(instance,
                                        NPVARIANT_TO_OBJECT(*variant));
  }
  NOTREACHED();
  return PP_MakeUndefined();
}

// static
NPIdentifier Var::PPVarToNPIdentifier(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string)
        return NULL;
      return WebBindings::getStringIdentifier(string->value().c_str());
    }
    case PP_VARTYPE_INT32:
      return WebBindings::getIntIdentifier(var.value.as_int);
    default:
      return NULL;
  }
}

// static
PP_Var Var::NPIdentifierToPPVar(PluginModule* module, NPIdentifier id) {
  const NPUTF8* string_value = NULL;
  int32_t int_value = 0;
  bool is_string = false;
  WebBindings::extractIdentifierData(id, string_value, int_value, is_string);
  if (is_string)
    return StringVar::StringToPPVar(module, string_value);

  return PP_MakeInt32(int_value);
}

// static
void Var::PluginAddRefPPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    if (!ResourceTracker::Get()->AddRefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "AddRefVar()ing a nonexistent string/object var.";
  }
}

// static
void Var::PluginReleasePPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    if (!ResourceTracker::Get()->UnrefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "ReleaseVar()ing a nonexistent string/object var.";
  }
}

// static
const PPB_Var_Deprecated* Var::GetDeprecatedInterface() {
  return &var_deprecated_interface;
}

const PPB_Var* Var::GetInterface() {
  return &var_interface;
}

StringVar* Var::AsStringVar() {
  return NULL;
}

ObjectVar* Var::AsObjectVar() {
  return NULL;
}

int32 Var::GetID() {
  // This should only be called for objects and strings. POD vars like integers
  // have no identifiers.
  DCHECK(AsStringVar() || AsObjectVar());

  ResourceTracker *tracker = ResourceTracker::Get();
  if (var_id_)
    tracker->AddRefVar(var_id_);
  else
    var_id_ = tracker->AddVar(this);
  return var_id_;
}

// StringVar -------------------------------------------------------------------

StringVar::StringVar(PluginModule* module, const char* str, uint32 len)
    : Var(module),
      value_(str, len) {
}

StringVar::~StringVar() {
}

StringVar* StringVar::AsStringVar() {
  return this;
}

// static
PP_Var StringVar::StringToPPVar(PluginModule* module, const std::string& var) {
  return StringToPPVar(module, var.c_str(), var.size());
}

// static
PP_Var StringVar::StringToPPVar(PluginModule* module,
                                const char* data, uint32 len) {
  scoped_refptr<StringVar> str(new StringVar(module, data, len));
  if (!str || !IsStringUTF8(str->value()))
    return PP_MakeNull();

  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;

  // The caller takes ownership now.
  ret.value.as_id = str->GetID();
  return ret;
}

// static
scoped_refptr<StringVar> StringVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_STRING)
    return scoped_refptr<StringVar>();
  scoped_refptr<Var> var_object(
      ResourceTracker::Get()->GetVar(static_cast<int32>(var.value.as_id)));
  if (!var_object)
    return scoped_refptr<StringVar>();
  return scoped_refptr<StringVar>(var_object->AsStringVar());
}

// ObjectVar -------------------------------------------------------------

ObjectVar::ObjectVar(PluginInstance* instance, NPObject* np_object)
    : Var(instance->module()),
      instance_(instance),
      np_object_(np_object) {
  WebBindings::retainObject(np_object_);
  instance->AddNPObjectVar(this);
}

ObjectVar::~ObjectVar() {
  instance_->RemoveNPObjectVar(this);
  WebBindings::releaseObject(np_object_);
}

ObjectVar* ObjectVar::AsObjectVar() {
  return this;
}

// static
PP_Var ObjectVar::NPObjectToPPVar(PluginInstance* instance, NPObject* object) {
  scoped_refptr<ObjectVar> object_var(instance->ObjectVarForNPObject(object));
  if (!object_var)  // No object for this module yet, make a new one.
    object_var = new ObjectVar(instance, object);

  if (!object_var)
    return PP_MakeUndefined();

  // Convert to a PP_Var, GetReference will AddRef for us.
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = object_var->GetID();
  return result;
}

// static
scoped_refptr<ObjectVar> ObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<ObjectVar>(NULL);
  scoped_refptr<Var> var_object(
      ResourceTracker::Get()->GetVar(static_cast<int32>(var.value.as_id)));
  if (!var_object)
    return scoped_refptr<ObjectVar>();
  return scoped_refptr<ObjectVar>(var_object->AsObjectVar());
}

// TryCatch --------------------------------------------------------------------

TryCatch::TryCatch(PluginModule* module, PP_Var* exception)
    : module_(module),
      has_exception_(exception && exception->type != PP_VARTYPE_UNDEFINED),
      exception_(exception) {
  WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
}

TryCatch::~TryCatch() {
  WebBindings::popExceptionHandler();
}

void TryCatch::SetException(const char* message) {
  if (!module_) {
    // Don't have a module to make the string.
    SetInvalidObjectException();
    return;
  }

  if (!has_exception()) {
    has_exception_ = true;
    if (exception_)
      *exception_ = StringVar::StringToPPVar(module_, message, strlen(message));
  }
}

void TryCatch::SetInvalidObjectException() {
  if (!has_exception()) {
    has_exception_ = true;
    // TODO(brettw) bug 54504: Have a global singleton string that can hold
    // a generic error message.
    if (exception_)
      *exception_ = PP_MakeInt32(1);
  }
}

// static
void TryCatch::Catch(void* self, const char* message) {
  static_cast<TryCatch*>(self)->SetException(message);
}

}  // namespace ppapi
}  // namespace webkit

