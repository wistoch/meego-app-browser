// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_MOCK_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_MOCK_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace o3d {
namespace gpu_plugin {

class MockNPObject : public NPObject {
 public:
  explicit MockNPObject(NPP npp) {
  }

  MOCK_METHOD0(Invalidate, void());
  MOCK_METHOD1(HasMethod, bool(NPIdentifier));
  MOCK_METHOD4(Invoke,
               bool(NPIdentifier, const NPVariant*, uint32_t, NPVariant*));
  MOCK_METHOD3(InvokeDefault, bool(const NPVariant*, uint32_t, NPVariant*));
  MOCK_METHOD1(HasProperty, bool(NPIdentifier));
  MOCK_METHOD2(GetProperty, bool(NPIdentifier, NPVariant*));
  MOCK_METHOD2(SetProperty, bool(NPIdentifier, const NPVariant*));
  MOCK_METHOD1(RemoveProperty, bool(NPIdentifier));
  MOCK_METHOD2(Enumerate, bool(NPIdentifier**, uint32_t*));
  MOCK_METHOD3(Construct, bool(const NPVariant*, uint32_t, NPVariant*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNPObject);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_MOCK_H_
