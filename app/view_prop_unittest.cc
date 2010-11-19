// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "app/view_prop.h"
#include "base/scoped_ptr.h"

typedef testing::Test ViewPropTest;

static const char* kKey1 = "key_1";
static const char* kKey2 = "key_2";

using app::ViewProp;

// Test a handful of viewprop assertions.
TEST_F(ViewPropTest, Basic) {
  gfx::NativeView nv1 = reinterpret_cast<gfx::NativeView>(1);
  gfx::NativeView nv2 = reinterpret_cast<gfx::NativeView>(2);

  void* data1 = reinterpret_cast<void*>(11);
  void* data2 = reinterpret_cast<void*>(12);

  // Initial value for a new view/key pair should be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    ViewProp prop(nv1, kKey1, data1);
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));
  }

  // The property fell out of scope, so the value should now be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    scoped_ptr<ViewProp> v1(new ViewProp(nv1, kKey1, data1));
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));

    // Register a value for the same view/key pair.
    scoped_ptr<ViewProp> v2(new ViewProp(nv1, kKey1, data2));
    // The new value should take over.
    EXPECT_EQ(data2, ViewProp::GetValue(nv1, kKey1));

    // Null out the first ViewProp, which should NULL out the value.
    v1.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
  }

  // The property fell out of scope, so the value should now be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    scoped_ptr<ViewProp> v1(new ViewProp(nv1, kKey1, data1));
    scoped_ptr<ViewProp> v2(new ViewProp(nv2, kKey2, data2));
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(data2, ViewProp::GetValue(nv2, kKey2));

    v1.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(data2, ViewProp::GetValue(nv2, kKey2));

    v2.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(NULL, ViewProp::GetValue(nv2, kKey2));
  }
}
