// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_WEBKIT_BROWSER_H_
#define O3D_GPU_PLUGIN_NP_UTILS_WEBKIT_BROWSER_H_

// TODO(apatrick): This does not belong in np_utils. np_utils should not be
//    dependent on WebKit (and it isn't - that's why the member functions are
//    inline).

#include <stdlib.h>

#include "o3d/gpu_plugin/np_utils/np_browser.h"
#include "WebKit/api/public/WebBindings.h"

typedef struct _NPNetscapeFuncs NPNetscapeFuncs;
typedef struct _NPChromiumFuncs NPChromiumFuncs;

namespace o3d {
namespace gpu_plugin {

// This class implements NPBrowser for the WebKit WebBindings.
class WebKitBrowser : public NPBrowser {
 public:
  WebKitBrowser(): NPBrowser(NULL) {
  }

  // Standard functions from NPNetscapeFuncs.

  virtual NPIdentifier GetStringIdentifier(const NPUTF8* name) {
    return WebKit::WebBindings::getStringIdentifier(name);
  }

  virtual void* MemAlloc(size_t size) {
    return malloc(size);
  }

  virtual void MemFree(void* p) {
    free(p);
  }

  virtual NPObject* CreateObject(NPP npp, const NPClass* cl) {
    return WebKit::WebBindings::createObject(npp, const_cast<NPClass*>(cl));
  }

  virtual NPObject* RetainObject(NPObject* object) {
    return WebKit::WebBindings::retainObject(object);
  }

  virtual void ReleaseObject(NPObject* object) {
    WebKit::WebBindings::releaseObject(object);
  }

  virtual void ReleaseVariantValue(NPVariant* variant) {
    WebKit::WebBindings::releaseVariantValue(variant);
  }

  virtual bool HasProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name) {
    return WebKit::WebBindings::hasProperty(npp, object, name);
  }

  virtual bool GetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           NPVariant* result) {
    return WebKit::WebBindings::getProperty(npp, object, name, result);
  }

  virtual bool SetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           const NPVariant* result) {
    return WebKit::WebBindings::setProperty(npp, object, name, result);
  }

  virtual bool RemoveProperty(NPP npp,
                              NPObject* object,
                              NPIdentifier name) {
    return WebKit::WebBindings::removeProperty(npp, object, name);
  }

  virtual bool HasMethod(NPP npp,
                           NPObject* object,
                           NPIdentifier name) {
    return WebKit::WebBindings::hasMethod(npp, object, name);
  }

  virtual bool Invoke(NPP npp,
                      NPObject* object,
                      NPIdentifier name,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result) {
    return WebKit::WebBindings::invoke(npp, object, name, args, num_args,
                                       result);
  }

  virtual NPObject* GetWindowNPObject(NPP npp) {
    NPObject* window;
    if (NPERR_NO_ERROR == NPN_GetValue(npp,
                                       NPNVWindowNPObject,
                                       &window)) {
      return window;
    } else {
      return NULL;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebKitBrowser);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_WEBKIT_BROWSER_H_
