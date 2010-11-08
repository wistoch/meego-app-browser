// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

namespace pepper {

class PluginInstance;
class PluginModule;
class Resource;

// This class maintains a global list of all live pepper resources. It allows
// us to check resource ID validity and to map them to a specific module.
//
// This object is threadsafe.
class ResourceTracker {
 public:
  // Returns the pointer to the singleton object.
  static ResourceTracker* Get() {
    return Singleton<ResourceTracker>::get();
  }

  // PP_Resources --------------------------------------------------------------

  // The returned pointer will be NULL if there is no resource. Note that this
  // return value is a scoped_refptr so that we ensure the resource is valid
  // from the point of the lookup to the point that the calling code needs it.
  // Otherwise, the plugin could Release() the resource on another thread and
  // the object will get deleted out from under us.
  scoped_refptr<Resource> GetResource(PP_Resource res) const;

  // Increment resource's plugin refcount. See ResourceAndRefCount comments
  // below.
  bool AddRefResource(PP_Resource res);
  bool UnrefResource(PP_Resource res);

  // Returns the number of resources associated with this module.
  //
  // This is slow, use only for testing.
  uint32 GetLiveObjectsForModule(PluginModule* module) const;

  // PP_Modules ----------------------------------------------------------------

  // Adds a new plugin module to the list of tracked module, and returns a new
  // module handle to identify it.
  PP_Module AddModule(PluginModule* module);

  // Called when a plugin modulde was deleted and should no longer be tracked.
  // The given handle should be one generated by AddModule.
  void ModuleDeleted(PP_Module module);

  // Returns a pointer to the plugin modulde object associated with the given
  // modulde handle. The return value will be NULL if the handle is invalid.
  PluginModule* GetModule(PP_Module module);

  // PP_Instances --------------------------------------------------------------

  // Adds a new plugin instance to the list of tracked instances, and returns a
  // new instance handle to identify it.
  PP_Instance AddInstance(PluginInstance* instance);

  // Called when a plugin instance was deleted and should no longer be tracked.
  // The given handle should be one generated by AddInstance.
  void InstanceDeleted(PP_Instance instance);

  // Returns a pointer to the plugin instance object associated with the given
  // instance handle. The return value will be NULL if the handle is invalid.
  PluginInstance* GetInstance(PP_Instance instance);

 private:
  friend struct DefaultSingletonTraits<ResourceTracker>;
  friend class Resource;

  // Prohibit creation other then by the Singleton class.
  ResourceTracker();
  ~ResourceTracker();

  // Adds the given resource to the tracker and assigns it a resource ID and
  // refcount of 1. The assigned resource ID will be returned. Used only by the
  // Resource class.
  PP_Resource AddResource(Resource* resource);

  // Last assigned resource ID.
  PP_Resource last_id_;

  // For each PP_Resource, keep the Resource* (as refptr) and plugin use count.
  // This use count is different then Resource's RefCount, and is manipulated
  // using this RefResource/UnrefResource. When it drops to zero, we just remove
  // the resource from this resource tracker, but the resource object will be
  // alive so long as some scoped_refptr still holds it's reference. This
  // prevents plugins from forcing destruction of Resource objects.
  typedef std::pair<scoped_refptr<Resource>, size_t> ResourceAndRefCount;
  typedef base::hash_map<PP_Resource, ResourceAndRefCount> ResourceMap;
  ResourceMap live_resources_;

  // Tracks all live instances. The pointers are non-owning, the PluginInstance
  // destructor will notify us when the instance is deleted.
  typedef std::map<PP_Instance, PluginInstance*> InstanceMap;
  InstanceMap instance_map_;

  // Tracks all live modules. The pointers are non-owning, the PluginModule
  // destructor will notify us when the module is deleted.
  typedef std::map<PP_Module, PluginModule*> ModuleMap;
  ModuleMap module_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourceTracker);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_
