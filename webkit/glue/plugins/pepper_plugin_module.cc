// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_module.h"

#include <set>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/ppapi/c/ppb_buffer.h"
#include "third_party/ppapi/c/ppb_core.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/ppapi/c/ppb_image_data.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppb_testing.h"
#include "third_party/ppapi/c/ppb_url_loader.h"
#include "third_party/ppapi/c/ppb_url_request_info.h"
#include "third_party/ppapi/c/ppb_url_response_info.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "third_party/ppapi/c/ppp.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"
#include "webkit/glue/plugins/pepper_var.h"

typedef bool (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
typedef void (*PPP_ShutdownModuleFunc)();

namespace pepper {

namespace {

// Maintains all currently loaded plugin libs for validating PP_Module
// identifiers.
typedef std::set<PluginModule*> PluginModuleSet;

PluginModuleSet* GetLivePluginSet() {
  static PluginModuleSet live_plugin_libs;
  return &live_plugin_libs;
}

base::MessageLoopProxy* GetMainThreadMessageLoop() {
  static scoped_refptr<base::MessageLoopProxy> proxy(
      base::MessageLoopProxy::CreateForCurrentThread());
  return proxy.get();
}

// PPB_Core --------------------------------------------------------------------

void AddRefResource(PP_Resource resource) {
  Resource* res = ResourceTracker::Get()->GetResource(resource);
  if (!res) {
    DLOG(WARNING) << "AddRef()ing a nonexistent resource";
    return;
  }
  res->AddRef();
}

void ReleaseResource(PP_Resource resource) {
  Resource* res = ResourceTracker::Get()->GetResource(resource);
  if (!res) {
    DLOG(WARNING) << "Release()ing a nonexistent resource";
    return;
  }
  res->Release();
}

void* MemAlloc(size_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  free(ptr);
}

double GetTime() {
  return base::Time::Now().ToDoubleT();
}

void CallOnMainThread(int delay_in_msec, void (*func)(void*), void* context) {
  GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(func, context),
      delay_in_msec);
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &MemAlloc,
  &MemFree,
  &GetTime,
  &CallOnMainThread
};

// PPB_Testing -----------------------------------------------------------------

bool ReadImageData(PP_Resource device_context_2d,
                   PP_Resource image,
                   int32_t x, int32_t y) {
  scoped_refptr<DeviceContext2D> context(
      ResourceTracker::Get()->GetAsDeviceContext2D(device_context_2d));
  if (!context.get())
    return false;
  return context->ReadImageData(image, x, y);
}

void RunMessageLoop() {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

const PPB_Testing testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
};

// GetInterface ----------------------------------------------------------------

const void* GetInterface(const char* name) {
  if (strcmp(name, PPB_CORE_INTERFACE) == 0)
    return &core_interface;
  if (strcmp(name, PPB_VAR_INTERFACE) == 0)
    return GetVarInterface();
  if (strcmp(name, PPB_INSTANCE_INTERFACE) == 0)
    return PluginInstance::GetInterface();
  if (strcmp(name, PPB_IMAGEDATA_INTERFACE) == 0)
    return ImageData::GetInterface();
  if (strcmp(name, PPB_DEVICECONTEXT2D_INTERFACE) == 0)
    return DeviceContext2D::GetInterface();
  if (strcmp(name, PPB_URLLOADER_INTERFACE) == 0)
    return URLLoader::GetInterface();
  if (strcmp(name, PPB_URLREQUESTINFO_INTERFACE) == 0)
    return URLRequestInfo::GetInterface();
  if (strcmp(name, PPB_URLRESPONSEINFO_INTERFACE) == 0)
    return URLResponseInfo::GetInterface();
  if (strcmp(name, PPB_BUFFER_INTERFACE) == 0)
    return Buffer::GetInterface();

  // Only support the testing interface when the command line switch is
  // specified. This allows us to prevent people from (ab)using this interface
  // in production code.
  if (strcmp(name, PPB_TESTING_INTERFACE) == 0) {
    if (CommandLine::ForCurrentProcess()->HasSwitch("enable-pepper-testing"))
      return &testing_interface;
  }
  return NULL;
}

}  // namespace

PluginModule::PluginModule(const FilePath& filename)
    : filename_(filename),
      initialized_(false),
      library_(0),
      ppp_get_interface_(NULL) {
  GetMainThreadMessageLoop();  // Initialize the main thread message loop.
  GetLivePluginSet()->insert(this);
}

PluginModule::~PluginModule() {
  // When the module is being deleted, there should be no more instances still
  // holding a reference to us.
  DCHECK(instances_.empty());

  GetLivePluginSet()->erase(this);

  if (library_) {
    PPP_ShutdownModuleFunc shutdown_module =
        reinterpret_cast<PPP_ShutdownModuleFunc>(
            base::GetFunctionPointerFromNativeLibrary(library_,
                                                      "PPP_ShutdownModule"));
    if (shutdown_module)
      shutdown_module();
    base::UnloadNativeLibrary(library_);
  }
}

// static
scoped_refptr<PluginModule> PluginModule::CreateModule(
    const FilePath& filename) {
  // FIXME(brettw) do uniquifying of the plugin here like the NPAPI one.

  scoped_refptr<PluginModule> lib(new PluginModule(filename));
  if (!lib->Load())
    lib = NULL;
  return lib;
}

// static
PluginModule* PluginModule::FromPPModule(PP_Module module) {
  PluginModule* lib = reinterpret_cast<PluginModule*>(module);
  if (GetLivePluginSet()->find(lib) == GetLivePluginSet()->end())
    return NULL;  // Invalid plugin.
  return lib;
}

bool PluginModule::Load() {
  if (initialized_)
    return true;
  initialized_ = true;

  library_ = base::LoadNativeLibrary(filename_);
  if (!library_)
    return false;

  // Save the GetInterface function pointer for later.
  ppp_get_interface_ =
      reinterpret_cast<PPP_GetInterfaceFunc>(
          base::GetFunctionPointerFromNativeLibrary(library_,
                                                    "PPP_GetInterface"));
  if (!ppp_get_interface_) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  // Call the plugin initialize function.
  PPP_InitializeModuleFunc initialize_module =
      reinterpret_cast<PPP_InitializeModuleFunc>(
          base::GetFunctionPointerFromNativeLibrary(library_,
                                                    "PPP_InitializeModule"));
  if (!initialize_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }
  int retval = initialize_module(GetPPModule(), &GetInterface);
  if (retval != 0) {
    LOG(WARNING) << "PPP_InitializeModule returned failure " << retval;
    return false;
  }

  return true;
}

PP_Module PluginModule::GetPPModule() const {
  return reinterpret_cast<intptr_t>(this);
}

PluginInstance* PluginModule::CreateInstance(PluginDelegate* delegate) {
  const PPP_Instance* plugin_instance_interface =
      reinterpret_cast<const PPP_Instance*>(GetPluginInterface(
          PPP_INSTANCE_INTERFACE));
  if (!plugin_instance_interface) {
    LOG(WARNING) << "Plugin doesn't support instance interface, failing.";
    return NULL;
  }
  return new PluginInstance(delegate, this, plugin_instance_interface);
}

PluginInstance* PluginModule::GetSomeInstance() const {
  // This will generally crash later if there is not actually any instance to
  // return, so we force a crash now to make bugs easier to track down.
  CHECK(!instances_.empty());
  return *instances_.begin();
}

const void* PluginModule::GetPluginInterface(const char* name) const {
  if (!ppp_get_interface_)
    return NULL;
  return ppp_get_interface_(name);
}

void PluginModule::InstanceCreated(PluginInstance* instance) {
  instances_.insert(instance);
}

void PluginModule::InstanceDeleted(PluginInstance* instance) {
  instances_.erase(instance);
}

}  // namespace pepper
