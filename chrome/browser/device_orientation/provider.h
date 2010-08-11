// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_

#include "base/logging.h"
#include "base/ref_counted.h"

namespace device_orientation {

class Orientation;

class Provider : public base::RefCounted<Provider> {
 public:
  class Observer {
   public:
    // Called when the orientation changes.
    // An Observer must not synchronously call Provider::RemoveObserver
    // or Provider::AddObserver when this is called.
    virtual void OnOrientationUpdate(const Orientation& orientation) = 0;
  };

  // Returns a pointer to the singleton instance of this class.
  // The caller should store the returned pointer in a scoped_refptr.
  // The Provider instance is lazily constructed when GetInstance() is called,
  // and destructed when the last scoped_refptr referring to it is destructed.
  static Provider* GetInstance();

  // Inject a mock Provider for testing. Only a weak pointer to the injected
  // object will be held by Provider, i.e. it does not itself contribute to the
  // injected object's reference count.
  static void SetInstanceForTests(Provider* provider);

  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}

 protected:
  Provider() {}
  virtual ~Provider() {
    DCHECK(instance_ == this);
    instance_ = NULL;
  }

 private:
  friend class base::RefCounted<Provider>;
  static Provider* instance_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace device_orientation

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_
