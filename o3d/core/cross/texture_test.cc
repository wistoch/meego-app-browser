/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class Texture.

#include "tests/common/win/testing_common.h"
#include "core/cross/texture_base.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"

namespace o3d {

class Texture2DTest : public testing::Test {
 protected:
  Texture2DTest()
      : object_manager_(g_service_locator) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

TEST_F(Texture2DTest, Basic) {
  Texture2D* texture = pack()->CreateTexture2D(8, 8, Texture::ARGB8, 1, false);
  ASSERT_TRUE(texture != NULL);
  EXPECT_TRUE(texture->IsA(Texture2D::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(Texture::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(ParamObject::GetApparentClass()));
  EXPECT_EQ(texture->format(), Texture::ARGB8);
  EXPECT_EQ(texture->levels(), 1);
  EXPECT_FALSE(texture->render_surfaces_enabled());
}

class TextureCUBETest : public testing::Test {
 protected:
  TextureCUBETest()
      : object_manager_(g_service_locator) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

TEST_F(TextureCUBETest, Basic) {
  TextureCUBE* texture =
      pack()->CreateTextureCUBE(8, Texture::ARGB8, 1, false);
  ASSERT_TRUE(texture != NULL);
  EXPECT_TRUE(texture->IsA(TextureCUBE::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(Texture::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(ParamObject::GetApparentClass()));
  EXPECT_EQ(texture->format(), Texture::ARGB8);
  EXPECT_EQ(texture->levels(), 1);
  EXPECT_FALSE(texture->render_surfaces_enabled());
}

}  // namespace o3d

