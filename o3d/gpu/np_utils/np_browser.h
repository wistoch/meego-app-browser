// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_NP_UTILS_NP_BROWSER_H_
#define GPU_NP_UTILS_NP_BROWSER_H_

#include "base/basictypes.h"
#include "gpu/np_utils/np_headers.h"

typedef struct _NPNetscapeFuncs NPNetscapeFuncs;

namespace np_utils {

// This class exposes the functions provided by the browser to a plugin (the
// ones prefixed NPN_).
class NPBrowser {
 public:
  explicit NPBrowser(NPNetscapeFuncs* funcs);
  virtual ~NPBrowser();

  static NPBrowser* get() {
    return browser_;
  }

  // Standard functions from NPNetscapeFuncs.

  virtual NPIdentifier GetStringIdentifier(const NPUTF8* name);

  virtual void* MemAlloc(size_t size);

  virtual void MemFree(void* p);

  virtual NPObject* CreateObject(NPP npp, const NPClass* cl);

  virtual NPObject* RetainObject(NPObject* object);

  virtual void ReleaseObject(NPObject* object);

  virtual void ReleaseVariantValue(NPVariant* variant);

  virtual bool HasProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name);

  virtual bool GetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           NPVariant* result);

  virtual bool SetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           const NPVariant* result);

  virtual bool RemoveProperty(NPP npp,
                              NPObject* object,
                              NPIdentifier name);

  virtual bool HasMethod(NPP npp,
                           NPObject* object,
                           NPIdentifier name);

  virtual bool Invoke(NPP npp,
                      NPObject* object,
                      NPIdentifier name,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result);

  virtual NPObject* GetWindowNPObject(NPP npp);

  typedef void (*PluginThreadAsyncCallProc)(void* data);
  virtual void PluginThreadAsyncCall(NPP npp,
                                     PluginThreadAsyncCallProc callback,
                                     void* data);

  typedef void (*TimerProc)(NPP npp, uint32 timer_id);
  virtual uint32 ScheduleTimer(NPP npp,
                               uint32 interval,
                               bool repeat,
                               TimerProc callback);

  virtual void UnscheduleTimer(NPP npp, uint32 timer_id);

 private:
  static NPBrowser* browser_;
  NPBrowser* previous_browser_;
  NPNetscapeFuncs* netscape_funcs_;
  DISALLOW_COPY_AND_ASSIGN(NPBrowser);
};

}  // namespace np_utils

#endif  // GPU_NP_UTILS_NP_BROWSER_H_
