// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_

typedef struct _pp_Var PP_Var;
typedef struct _ppb_Var PPB_Var;
typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace pepper {

// There's no class implementing Var since it could represent a number of
// objects. Instead, we just expose a getter for the interface implemented in
// the .cc file here.
const PPB_Var* GetVarInterface();

// Returns a PP_Var of type object that wraps the given NPObject.  Calling this
// function multiple times given the same NPObject results in the same PP_Var.
PP_Var NPObjectToPPVar(NPObject* object);

// Returns a PP_Var that corresponds to the given NPVariant.  The contents of
// the NPVariant will be copied unless the NPVariant corresponds to an object.
PP_Var NPVariantToPPVar(NPVariant* variant);

// Returns a NPVariant that corresponds to the given PP_Var.  The contents of
// the PP_Var will be copied unless the PP_Var corresponds to an object.
NPVariant PPVarToNPVariant(PP_Var var);

// Returns a NPVariant that corresponds to the given PP_Var.  The contents of
// the PP_Var will NOT be copied, so you need to ensure that the PP_Var remains
// valid while the resultant NPVariant is in use.
NPVariant PPVarToNPVariantNoCopy(PP_Var var);

// Returns a NPIdentifier that corresponds to the given PP_Var.  The contents
// of the PP_Var will be copied.  Returns NULL if the given PP_Var is not a a
// string or integer type.
NPIdentifier PPVarToNPIdentifier(PP_Var var);

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
