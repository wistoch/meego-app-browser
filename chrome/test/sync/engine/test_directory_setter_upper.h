// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A handy class that takes care of setting up and destroying a
// syncable::Directory instance for unit tests that require one.
//
// The expected usage is to make this a component of your test fixture:
//
//   class AwesomenessTest : public testing::Test {
//    public:
//     virtual void SetUp() {
//       metadb_.SetUp();
//     }
//     virtual void TearDown() {
//       metadb_.TearDown();
//     }
//    protected:
//     TestDirectorySetterUpper metadb_;
//   };
//
// Then, in your tests, get at the directory like so:
//
//   TEST_F(AwesomenessTest, IsMaximal) {
//     ScopedDirLookup dir(metadb_.manager(), metadb_.name());
//     ... now use |dir| to get at syncable::Entry objects ...
//   }
//

#ifndef CHROME_TEST_SYNC_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
#define CHROME_TEST_SYNC_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace syncable {
class DirectoryManager;
class ScopedDirLookup;
}  // namespace syncable

namespace browser_sync {

class TestDirectorySetterUpper {
 public:
  TestDirectorySetterUpper();
  virtual ~TestDirectorySetterUpper();

  // Create a DirectoryManager instance and use it to open the directory.
  // Clears any existing database backing files that might exist on disk.
  virtual void SetUp();

  // Undo everything done by SetUp(): close the directory and delete the
  // backing files. Before closing the directory, this will run the directory
  // invariant checks and perform the SaveChanges action on the directory.
  virtual void TearDown();

  syncable::DirectoryManager* manager() const { return manager_.get(); }
  const PathString& name() const { return name_; }

 protected:
  virtual void Init();

 private:
  void RunInvariantCheck(const syncable::ScopedDirLookup& dir);

  scoped_ptr<syncable::DirectoryManager> manager_;
  const PathString name_;
  PathString file_path_;
};

// A variant of the above where SetUp does not actually open the directory.
// You must manually invoke Open().  This is useful if you are writing a test
// that depends on the DirectoryManager::OPENED event.
class ManuallyOpenedTestDirectorySetterUpper : public TestDirectorySetterUpper {
 public:
  ManuallyOpenedTestDirectorySetterUpper() : was_opened_(false) {}
  virtual void SetUp();
  virtual void TearDown();
  void Open();
 private:
  bool was_opened_;
};

}  // namespace browser_sync

#endif  // CHROME_TEST_SYNC_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
