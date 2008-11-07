// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/perftimer.h"
#include "base/platform_test.h"
#if defined(OS_WIN)
#include "base/scoped_handle.h"
#endif
#include "base/string_util.h"
#include "base/timer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

extern int g_cache_tests_max_id;
extern volatile int g_cache_tests_received;
extern volatile bool g_cache_tests_error;

typedef PlatformTest DiskCacheTest;

namespace {

bool EvictFileFromSystemCache(const wchar_t* name) {
#if defined(OS_WIN)
  // Overwrite it with no buffering.
  ScopedHandle file(CreateFile(name, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL));
  if (!file.IsValid())
    return false;

  // Execute in chunks. It could be optimized. We want to do few of these since
  // these opterations will be slow without the cache.
  char buffer[128 * 1024];
  int total_bytes = 0;
  DWORD bytes_read;
  for (;;) {
    if (!ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL))
      return false;
    if (bytes_read == 0)
      break;

    bool final = false;
    if (bytes_read < sizeof(buffer))
      final = true;

    DWORD to_write = final ? sizeof(buffer) : bytes_read;

    DWORD actual;
    SetFilePointer(file, total_bytes, 0, FILE_BEGIN);
    if (!WriteFile(file, buffer, to_write, &actual, NULL))
      return false;
    total_bytes += bytes_read;

    if (final) {
      SetFilePointer(file, total_bytes, 0, FILE_BEGIN);
      SetEndOfFile(file);
      break;
    }
  }
  return true;
#elif defined(OS_LINUX)
  int fd = open(WideToUTF8(std::wstring(name)).c_str(), O_RDONLY);
  if (fd < 0)
    return false;
  if (fdatasync(fd) != 0)
    return false;
  if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) != 0)
    return false;
  close(fd);
  return true;
#else
  // TODO(port): Mac has its own way to do this.
  NOTIMPLEMENTED();
  return false;
#endif
}

struct TestEntry {
  std::string key;
  int data_len;
};
typedef std::vector<TestEntry> TestEntries;

const int kMaxSize = 16 * 1024 - 1;

// Creates num_entries on the cache, and writes 200 bytes of metadata and up
// to kMaxSize of data to each entry.
int TimeWrite(int num_entries, disk_cache::Backend* cache,
              TestEntries* entries) {
  char buffer1[200];
  char buffer2[kMaxSize];

  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  CacheTestFillBuffer(buffer2, sizeof(buffer2), false);

  CallbackTest callback(1);
  g_cache_tests_error = false;
  g_cache_tests_max_id = 1;
  g_cache_tests_received = 0;
  int expected = 0;

  MessageLoopHelper helper;

  PerfTimeLogger timer("Write disk cache entries");

  for (int i = 0; i < num_entries; i++) {
    TestEntry entry;
    entry.key = GenerateKey(true);
    entry.data_len = rand() % sizeof(buffer2);
    entries->push_back(entry);

    disk_cache::Entry* cache_entry;
    if (!cache->CreateEntry(entry.key, &cache_entry))
      break;
    int ret = cache_entry->WriteData(0, 0, buffer1, sizeof(buffer1), &callback,
                                     false);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (sizeof(buffer1) != ret)
      break;

    ret = cache_entry->WriteData(1, 0, buffer2, entry.data_len, &callback,
                                 false);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (entry.data_len != ret)
      break;
    cache_entry->Close();
  }

  helper.WaitUntilCacheIoFinished(expected);
  timer.Done();

  return expected;
}

// Reads the data and metadata from each entry listed on |entries|.
int TimeRead(int num_entries, disk_cache::Backend* cache,
             const TestEntries& entries, bool cold) {
  char buffer1[200];
  char buffer2[kMaxSize];

  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  CacheTestFillBuffer(buffer2, sizeof(buffer2), false);

  CallbackTest callback(1);
  g_cache_tests_error = false;
  g_cache_tests_max_id = 1;
  g_cache_tests_received = 0;
  int expected = 0;

  MessageLoopHelper helper;

  const char* message = cold ? "Read disk cache entries (cold)" :
                        "Read disk cache entries (warm)";
  PerfTimeLogger timer(message);

  for (int i = 0; i < num_entries; i++) {
    disk_cache::Entry* cache_entry;
    if (!cache->OpenEntry(entries[i].key, &cache_entry))
      break;
    int ret = cache_entry->ReadData(0, 0, buffer1, sizeof(buffer1), &callback);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (sizeof(buffer1) != ret)
      break;

    ret = cache_entry->ReadData(1, 0, buffer2, entries[i].data_len, &callback);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (entries[i].data_len != ret)
      break;
    cache_entry->Close();
  }

  helper.WaitUntilCacheIoFinished(expected);
  timer.Done();

  return expected;
}

int BlockSize() {
  // We can use form 1 to 4 blocks.
  return (rand() & 0x3) + 1;
}

}  // namespace

TEST_F(DiskCacheTest, Hash) {
  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  PerfTimeLogger timer("Hash disk cache keys");
  for (int i = 0; i < 300000; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Hash(key);
  }
  timer.Done();
}

TEST_F(DiskCacheTest, CacheBackendPerformance) {
  MessageLoopForIO message_loop;

  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));
  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0);
  ASSERT_TRUE(NULL != cache);

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  TestEntries entries;
  int num_entries = 1000;

  int ret = TimeWrite(num_entries, cache, &entries);
  EXPECT_EQ(ret, g_cache_tests_received);

  MessageLoop::current()->RunAllPending();
  delete cache;

  std::wstring filename(path);
  file_util::AppendToPath(&filename, L"index");
  ASSERT_TRUE(EvictFileFromSystemCache(filename.c_str()));

  filename = path;
  file_util::AppendToPath(&filename, L"data_0");
  ASSERT_TRUE(EvictFileFromSystemCache(filename.c_str()));

  filename = path;
  file_util::AppendToPath(&filename, L"data_1");
  ASSERT_TRUE(EvictFileFromSystemCache(filename.c_str()));

  filename = path;
  file_util::AppendToPath(&filename, L"data_2");
  ASSERT_TRUE(EvictFileFromSystemCache(filename.c_str()));

  filename = path;
  file_util::AppendToPath(&filename, L"data_3");
  ASSERT_TRUE(EvictFileFromSystemCache(filename.c_str()));

  cache = disk_cache::CreateCacheBackend(path, false, 0);
  ASSERT_TRUE(NULL != cache);

  ret = TimeRead(num_entries, cache, entries, true);
  EXPECT_EQ(ret, g_cache_tests_received);

  ret = TimeRead(num_entries, cache, entries, false);
  EXPECT_EQ(ret, g_cache_tests_received);

  MessageLoop::current()->RunAllPending();
  delete cache;
}

// Creating and deleting "entries" on a block-file is something quite frequent
// (after all, almost everything is stored on block files). The operation is
// almost free when the file is empty, but can be expensive if the file gets
// fragmented, or if we have multiple files. This test measures that scenario,
// by using multiple, highly fragmented files.
TEST_F(DiskCacheTest, BlockFilesPerformance) {
  MessageLoopForIO message_loop;
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 60000;
  int32 buffer[kNumEntries];
  memset(buffer, 0, sizeof(buffer));
  disk_cache::Addr* address = reinterpret_cast<disk_cache::Addr*>(buffer);
  ASSERT_EQ(sizeof(*address), sizeof(*buffer));
  
  PerfTimeLogger timer1("Fill three block-files");

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kNumEntries; i++) {
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, BlockSize(),
                                  &address[i]));
  }

  timer1.Done();
  PerfTimeLogger timer2("Create and delete blocks");

  for (int i = 0; i < 200000; i++) {
    int entry = rand() * (kNumEntries / RAND_MAX + 1);
    if (entry >= kNumEntries)
      entry = 0;

    files.DeleteBlock(address[entry], false);
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, BlockSize(),
                                  &address[entry]));
  }

  timer2.Done();
  MessageLoop::current()->RunAllPending();
}
