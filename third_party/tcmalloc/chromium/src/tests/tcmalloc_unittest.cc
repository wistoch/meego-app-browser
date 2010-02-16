// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Unittest for the TCMalloc implementation.
//
// * The test consists of a set of threads.
// * Each thread maintains a set of allocated objects, with
//   a bound on the total amount of data in the set.
// * Each allocated object's contents are generated by
//   hashing the object pointer, and a generation count
//   in the object.  This allows us to easily check for
//   data corruption.
// * At any given step, the thread can do any of the following:
//     a. Allocate an object
//     b. Increment an object's generation count and update
//        its contents.
//     c. Pass the object to another thread
//     d. Free an object
//   Also, at the end of every step, object(s) are freed to maintain
//   the memory upper-bound.
//
// If this test is compiled with -DDEBUGALLOCATION, then we don't
// run some tests that test the inner workings of tcmalloc and
// break on debugallocation: that certain allocations are aligned
// in a certain way (even though no standard requires it), and that
// realloc() tries to minimize copying (which debug allocators don't
// care about).

#include "config_for_unittests.h"
// Complicated ordering requirements.  tcmalloc.h defines (indirectly)
// _POSIX_C_SOURCE, which it needs so stdlib.h defines posix_memalign.
// unistd.h, on the other hand, requires _POSIX_C_SOURCE to be unset,
// at least on FreeBSD, in order to define sbrk.  The solution
// is to #include unistd.h first.  This is safe because unistd.h
// doesn't sub-include stdlib.h, so we'll still get posix_memalign
// when we #include stdlib.h.  Blah.
#ifdef HAVE_UNISTD_H
#include <unistd.h>        // for testing sbrk hooks
#endif
#include "tcmalloc.h"      // must come early, to pick up posix_memalign
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined HAVE_STDINT_H
#include <stdint.h>        // for intptr_t
#endif
#include <sys/types.h>     // for size_t
#ifdef HAVE_FCNTL_H
#include <fcntl.h>         // for open; used with mmap-hook test
#endif
#ifdef HAVE_MMAP
#include <sys/mman.h>      // for testing mmap hooks
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>        // defines pvalloc/etc on cygwin
#endif
#include <assert.h>
#include <vector>
#include <algorithm>
#include <string>
#include <new>
#include "base/logging.h"
#include "base/simple_mutex.h"
#include "google/malloc_hook.h"
#include "google/malloc_extension.h"
#include "google/tcmalloc.h"
#include "thread_cache.h"
#include "tests/testutil.h"

// Windows doesn't define pvalloc and a few other obsolete unix
// functions; nor does it define posix_memalign (which is not obsolete).
#if defined(_MSC_VER) || defined(__MINGW32__)
# define cfree free         // don't bother to try to test these obsolete fns
# define valloc malloc
# define pvalloc malloc
# ifdef PERFTOOLS_NO_ALIGNED_MALLOC
#   define _aligned_malloc(size, alignment) malloc(size)
# else
#   include <malloc.h>      // for _aligned_malloc
# endif
# define memalign(alignment, size) _aligned_malloc(size, alignment)
// Assume if we fail, it's because of out-of-memory.
// Note, this isn't a perfect analogue: we don't enforce constraints on "align"
# include <errno.h>
# define posix_memalign(pptr, align, size) \
      ((*(pptr)=_aligned_malloc(size, align)) ? 0 : ENOMEM)
#endif

// On systems (like freebsd) that don't define MAP_ANONYMOUS, use the old
// form of the name instead.
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

#define LOGSTREAM   stdout

using std::vector;
using std::string;

namespace testing {

static const int FLAGS_numtests = 50000;
static const int FLAGS_log_every_n_tests = 50000; // log exactly once

// Testing parameters
static const int FLAGS_lgmaxsize = 16;   // lg() of the max size object to alloc
static const int FLAGS_numthreads = 10;  // Number of threads
static const int FLAGS_threadmb = 4;     // Max memory size allocated by thread
static const int FLAGS_lg_max_memalign = 18; // lg of max alignment for memalign

static const double FLAGS_memalign_min_fraction = 0;    // min expected%
static const double FLAGS_memalign_max_fraction = 0.4;  // max expected%
static const double FLAGS_memalign_max_alignment_ratio = 6;  // alignment/size

// Weights of different operations
static const int FLAGS_allocweight = 50;    // Weight for picking allocation
static const int FLAGS_freeweight = 50;     // Weight for picking free
static const int FLAGS_updateweight = 10;   // Weight for picking update
static const int FLAGS_passweight = 1;      // Weight for passing object

static const int kSizeBits = 8 * sizeof(size_t);
static const size_t kMaxSize = ~static_cast<size_t>(0);
static const size_t kMaxSignedSize = ((size_t(1) << (kSizeBits-1)) - 1);

static const size_t kNotTooBig = 100000;
static const size_t kTooBig = kMaxSize;

static int news_handled = 0;

// Global array of threads
class TesterThread;
static TesterThread** threads;

// To help with generating random numbers
class TestHarness {
 private:
  // Information kept per type
  struct Type {
    string      name;
    int         type;
    int         weight;
  };

 public:
  TestHarness(int seed)
      : types_(new vector<Type>), total_weight_(0), num_tests_(0) {
    srandom(seed);
  }
  ~TestHarness() {
    delete types_;
  }

  // Add operation type with specified weight.  When starting a new
  // iteration, an operation type is picked with probability
  // proportional to its weight.
  //
  // "type" must be non-negative.
  // "weight" must be non-negative.
  void AddType(int type, int weight, const char* name);

  // Call this to get the type of operation for the next iteration.
  // It returns a random operation type from the set of registered
  // operations.  Returns -1 if tests should finish.
  int PickType();

  // If n == 0, returns the next pseudo-random number in the range [0 .. 0]
  // If n != 0, returns the next pseudo-random number in the range [0 .. n)
  int Uniform(int n) {
    if (n == 0) {
      return random() * 0;
    } else {
      return random() % n;
    }
  }
  // Pick "base" uniformly from range [0,max_log] and then return
  // "base" random bits.  The effect is to pick a number in the range
  // [0,2^max_log-1] with bias towards smaller numbers.
  int Skewed(int max_log) {
    const int base = random() % (max_log+1);
    return random() % (1 << base);
  }

 private:
  vector<Type>*         types_;         // Registered types
  int                   total_weight_;  // Total weight of all types
  int                   num_tests_;     // Num tests run so far
};

void TestHarness::AddType(int type, int weight, const char* name) {
  Type t;
  t.name = name;
  t.type = type;
  t.weight = weight;
  types_->push_back(t);
  total_weight_ += weight;
}

int TestHarness::PickType() {
  if (num_tests_ >= FLAGS_numtests) return -1;
  num_tests_++;

  assert(total_weight_ > 0);
  // This is a little skewed if total_weight_ doesn't divide 2^31, but it's close
  int v = Uniform(total_weight_);
  int i;
  for (i = 0; i < types_->size(); i++) {
    v -= (*types_)[i].weight;
    if (v < 0) {
      break;
    }
  }

  assert(i < types_->size());
  if ((num_tests_ % FLAGS_log_every_n_tests) == 0) {
    fprintf(LOGSTREAM, "  Test %d out of %d: %s\n",
            num_tests_, FLAGS_numtests, (*types_)[i].name.c_str());
  }
  return (*types_)[i].type;
}

class AllocatorState : public TestHarness {
 public:
  explicit AllocatorState(int seed) : TestHarness(seed) {
    CHECK_GE(FLAGS_memalign_max_fraction, 0);
    CHECK_LE(FLAGS_memalign_max_fraction, 1);
    CHECK_GE(FLAGS_memalign_min_fraction, 0);
    CHECK_LE(FLAGS_memalign_min_fraction, 1);
    double delta = FLAGS_memalign_max_fraction - FLAGS_memalign_min_fraction;
    CHECK_GE(delta, 0);
    memalign_fraction_ = (Uniform(10000)/10000.0 * delta +
                          FLAGS_memalign_min_fraction);
    //fprintf(LOGSTREAM, "memalign fraction: %f\n", memalign_fraction_);
  }
  virtual ~AllocatorState() {}

  // Allocate memory.  Randomly choose between malloc() or posix_memalign().
  void* alloc(size_t size) {
    if (Uniform(100) < memalign_fraction_ * 100) {
      // Try a few times to find a reasonable alignment, or fall back on malloc.
      for (int i = 0; i < 5; i++) {
        size_t alignment = 1 << Uniform(FLAGS_lg_max_memalign);
        if (alignment >= sizeof(intptr_t) &&
            (size < sizeof(intptr_t) ||
             alignment < FLAGS_memalign_max_alignment_ratio * size)) {
          void *result = reinterpret_cast<void*>(static_cast<intptr_t>(0x1234));
          int err = posix_memalign(&result, alignment, size);
          if (err != 0) {
            CHECK_EQ(err, ENOMEM);
          }
          return err == 0 ? result : NULL;
        }
      }
    }
    return malloc(size);
  }

 private:
  double memalign_fraction_;
};


// Info kept per thread
class TesterThread {
 private:
  // Info kept per allocated object
  struct Object {
    char*       ptr;                    // Allocated pointer
    int         size;                   // Allocated size
    int         generation;             // Generation counter of object contents
  };

  Mutex                 lock_;          // For passing in another thread's obj
  int                   id_;            // My thread id
  AllocatorState        rnd_;           // For generating random numbers
  vector<Object>        heap_;          // This thread's heap
  vector<Object>        passed_;        // Pending objects passed from others
  size_t                heap_size_;     // Current heap size
  int                   locks_ok_;      // Number of OK TryLock() ops
  int                   locks_failed_;  // Number of failed TryLock() ops

  // Type of operations
  enum Type { ALLOC, FREE, UPDATE, PASS };

  // ACM minimal standard random number generator.  (re-entrant.)
  class ACMRandom {
    int32 seed_;
   public:
    explicit ACMRandom(int32 seed) { seed_ = seed; }
    int32 Next() {
      const int32 M = 2147483647L;   // 2^31-1
      const int32 A = 16807;
      // In effect, we are computing seed_ = (seed_ * A) % M, where M = 2^31-1
      uint32 lo = A * (int32)(seed_ & 0xFFFF);
      uint32 hi = A * (int32)((uint32)seed_ >> 16);
      lo += (hi & 0x7FFF) << 16;
      if (lo > M) {
        lo &= M;
        ++lo;
      }
      lo += hi >> 15;
      if (lo > M) {
        lo &= M;
        ++lo;
      }
      return (seed_ = (int32) lo);
    }
  };

 public:
  TesterThread(int id)
    : id_(id),
      rnd_(id+1),
      heap_size_(0),
      locks_ok_(0),
      locks_failed_(0) {
  }

  virtual ~TesterThread() {
    if (FLAGS_verbose)
      fprintf(LOGSTREAM, "Thread %2d: locks %6d ok; %6d trylocks failed\n",
              id_, locks_ok_, locks_failed_);
    if (locks_ok_ + locks_failed_ >= 1000) {
      CHECK_LE(locks_failed_, locks_ok_ / 2);
    }
  }

  virtual void Run() {
    rnd_.AddType(ALLOC,  FLAGS_allocweight,   "allocate");
    rnd_.AddType(FREE,   FLAGS_freeweight,    "free");
    rnd_.AddType(UPDATE, FLAGS_updateweight,  "update");
    rnd_.AddType(PASS,   FLAGS_passweight,    "pass");

    while (true) {
      AcquirePassedObjects();

      switch (rnd_.PickType()) {
        case ALLOC:   AllocateObject(); break;
        case FREE:    FreeObject();     break;
        case UPDATE:  UpdateObject();   break;
        case PASS:    PassObject();     break;
        case -1:      goto done;
        default:      assert(NULL == "Unknown type");
      }

      ShrinkHeap();
    }

 done:
    DeleteHeap();
  }

  // Allocate a new object
  void AllocateObject() {
    Object object;
    object.size = rnd_.Skewed(FLAGS_lgmaxsize);
    object.ptr = static_cast<char*>(rnd_.alloc(object.size));
    CHECK(object.ptr);
    object.generation = 0;
    FillContents(&object);
    heap_.push_back(object);
    heap_size_ += object.size;
  }

  // Mutate a random object
  void UpdateObject() {
    if (heap_.empty()) return;
    const int index = rnd_.Uniform(heap_.size());
    CheckContents(heap_[index]);
    heap_[index].generation++;
    FillContents(&heap_[index]);
  }

  // Free a random object
  void FreeObject() {
    if (heap_.empty()) return;
    const int index = rnd_.Uniform(heap_.size());
    Object object = heap_[index];
    CheckContents(object);
    free(object.ptr);
    heap_size_ -= object.size;
    heap_[index] = heap_[heap_.size()-1];
    heap_.pop_back();
  }

  // Delete all objects in the heap
  void DeleteHeap() {
    while (!heap_.empty()) {
      FreeObject();
    }
  }

  // Free objects until our heap is small enough
  void ShrinkHeap() {
    while (heap_size_ > FLAGS_threadmb << 20) {
      assert(!heap_.empty());
      FreeObject();
    }
  }

  // Pass a random object to another thread
  void PassObject() {
    // Pick object to pass
    if (heap_.empty()) return;
    const int index = rnd_.Uniform(heap_.size());
    Object object = heap_[index];
    CheckContents(object);

    // Pick thread to pass
    const int tid = rnd_.Uniform(FLAGS_numthreads);
    TesterThread* thread = threads[tid];

    if (thread->lock_.TryLock()) {
      // Pass the object
      locks_ok_++;
      thread->passed_.push_back(object);
      thread->lock_.Unlock();
      heap_size_ -= object.size;
      heap_[index] = heap_[heap_.size()-1];
      heap_.pop_back();
    } else {
      locks_failed_++;
    }
  }

  // Grab any objects passed to this thread by another thread
  void AcquirePassedObjects() {
    // We do not create unnecessary contention by always using
    // TryLock().  Plus we unlock immediately after swapping passed
    // objects into a local vector.
    vector<Object> copy;
    { // Locking scope
      if (!lock_.TryLock()) {
        locks_failed_++;
        return;
      }
      locks_ok_++;
      swap(copy, passed_);
      lock_.Unlock();
    }

    for (int i = 0; i < copy.size(); ++i) {
      const Object& object = copy[i];
      CheckContents(object);
      heap_.push_back(object);
      heap_size_ += object.size;
    }
  }

  // Fill object contents according to ptr/generation
  void FillContents(Object* object) {
    ACMRandom r(reinterpret_cast<intptr_t>(object->ptr) & 0x7fffffff);
    for (int i = 0; i < object->generation; ++i) {
      r.Next();
    }
    const char c = static_cast<char>(r.Next());
    memset(object->ptr, c, object->size);
  }

  // Check object contents
  void CheckContents(const Object& object) {
    ACMRandom r(reinterpret_cast<intptr_t>(object.ptr) & 0x7fffffff);
    for (int i = 0; i < object.generation; ++i) {
      r.Next();
    }

    // For large objects, we just check a prefix/suffix
    const char expected = static_cast<char>(r.Next());
    const int limit1 = object.size < 32 ? object.size : 32;
    const int start2 = limit1 > object.size - 32 ? limit1 : object.size - 32;
    for (int i = 0; i < limit1; ++i) {
      CHECK_EQ(object.ptr[i], expected);
    }
    for (int i = start2; i < object.size; ++i) {
      CHECK_EQ(object.ptr[i], expected);
    }
  }
};

static void RunThread(int thread_id) {
  threads[thread_id]->Run();
}

static void TryHugeAllocation(size_t s, AllocatorState* rnd) {
  void* p = rnd->alloc(s);
  CHECK(p == NULL);   // huge allocation s should fail!
}

static void TestHugeAllocations(AllocatorState* rnd) {
  // Check that asking for stuff tiny bit smaller than largest possible
  // size returns NULL.
  for (size_t i = 0; i < 70000; i += rnd->Uniform(20)) {
    TryHugeAllocation(kMaxSize - i, rnd);
  }
  // Asking for memory sizes near signed/unsigned boundary (kMaxSignedSize)
  // might work or not, depending on the amount of virtual memory.
#ifndef DEBUGALLOCATION    // debug allocation takes forever for huge allocs
  for (size_t i = 0; i < 100; i++) {
    void* p = NULL;
    p = rnd->alloc(kMaxSignedSize + i);
    if (p) free(p);    // if: free(NULL) is not necessarily defined
    p = rnd->alloc(kMaxSignedSize - i);
    if (p) free(p);
  }
#endif

  // Check that ReleaseFreeMemory has no visible effect (aka, does not
  // crash the test):
  MallocExtension* inst = MallocExtension::instance();
  CHECK(inst);
  inst->ReleaseFreeMemory();
}

static void TestCalloc(size_t n, size_t s, bool ok) {
  char* p = reinterpret_cast<char*>(calloc(n, s));
  if (FLAGS_verbose)
    fprintf(LOGSTREAM, "calloc(%"PRIxS", %"PRIxS"): %p\n", n, s, p);
  if (!ok) {
    CHECK(p == NULL);  // calloc(n, s) should not succeed
  } else {
    CHECK(p != NULL);  // calloc(n, s) should succeed
    for (int i = 0; i < n*s; i++) {
      CHECK(p[i] == '\0');
    }
    free(p);
  }
}

// This makes sure that reallocing a small number of bytes in either
// direction doesn't cause us to allocate new memory.
static void TestRealloc() {
#ifndef DEBUGALLOCATION  // debug alloc doesn't try to minimize reallocs
  int start_sizes[] = { 100, 1000, 10000, 100000 };
  int deltas[] = { 1, -2, 4, -8, 16, -32, 64, -128 };

  for (int s = 0; s < sizeof(start_sizes)/sizeof(*start_sizes); ++s) {
    void* p = malloc(start_sizes[s]);
    CHECK(p);
    // The larger the start-size, the larger the non-reallocing delta.
    for (int d = 0; d < s*2; ++d) {
      void* new_p = realloc(p, start_sizes[s] + deltas[d]);
      CHECK(p == new_p);  // realloc should not allocate new memory
    }
    // Test again, but this time reallocing smaller first.
    for (int d = 0; d < s*2; ++d) {
      void* new_p = realloc(p, start_sizes[s] - deltas[d]);
      CHECK(p == new_p);  // realloc should not allocate new memory
    }
    free(p);
  }
#endif
}

static void TestNewHandler() throw (std::bad_alloc) {
  ++news_handled;
  throw std::bad_alloc();
}

static void TestOneNew(void* (*func)(size_t)) {
  // success test
  try {
    void* ptr = (*func)(kNotTooBig);
    if (0 == ptr) {
      fprintf(LOGSTREAM, "allocation should not have failed.\n");
      abort();
    }
  } catch (...) {
    fprintf(LOGSTREAM, "allocation threw unexpected exception.\n");
    abort();
  }

  // failure test
  // we should always receive a bad_alloc exception
  try {
    (*func)(kTooBig);
    fprintf(LOGSTREAM, "allocation should have failed.\n");
    abort();
  } catch (const std::bad_alloc&) {
    // correct
  } catch (...) {
    fprintf(LOGSTREAM, "allocation threw unexpected exception.\n");
    abort();
  }
}

static void TestNew(void* (*func)(size_t)) {
  news_handled = 0;

  // test without new_handler:
  std::new_handler saved_handler = std::set_new_handler(0);
  TestOneNew(func);

  // test with new_handler:
  std::set_new_handler(TestNewHandler);
  TestOneNew(func);
  if (news_handled != 1) {
    fprintf(LOGSTREAM, "new_handler was not called.\n");
    abort();
  }
  std::set_new_handler(saved_handler);
}

static void TestOneNothrowNew(void* (*func)(size_t, const std::nothrow_t&)) {
  // success test
  try {
    void* ptr = (*func)(kNotTooBig, std::nothrow);
    if (0 == ptr) {
      fprintf(LOGSTREAM, "allocation should not have failed.\n");
      abort();
    }
  } catch (...) {
    fprintf(LOGSTREAM, "allocation threw unexpected exception.\n");
    abort();
  }

  // failure test
  // we should always receive a bad_alloc exception
  try {
    if ((*func)(kTooBig, std::nothrow) != 0) {
      fprintf(LOGSTREAM, "allocation should have failed.\n");
      abort();
    }
  } catch (...) {
    fprintf(LOGSTREAM, "nothrow allocation threw unexpected exception.\n");
    abort();
  }
}

static void TestNothrowNew(void* (*func)(size_t, const std::nothrow_t&)) {
  news_handled = 0;

  // test without new_handler:
  std::new_handler saved_handler = std::set_new_handler(0);
  TestOneNothrowNew(func);

  // test with new_handler:
  std::set_new_handler(TestNewHandler);
  TestOneNothrowNew(func);
  if (news_handled != 1) {
    fprintf(LOGSTREAM, "nothrow new_handler was not called.\n");
    abort();
  }
  std::set_new_handler(saved_handler);
}


// These are used as callbacks by the sanity-check.  Set* and Reset*
// register the hook that counts how many times the associated memory
// function is called.  After each such call, call Verify* to verify
// that we used the tcmalloc version of the call, and not the libc.
// Note the ... in the hook signature: we don't care what arguments
// the hook takes.
#define MAKE_HOOK_CALLBACK(hook_type)                                   \
  static int g_##hook_type##_calls = 0;                                 \
  static void IncrementCallsTo##hook_type(...) {                        \
    g_##hook_type##_calls++;                                            \
  }                                                                     \
  static void Verify##hook_type##WasCalled() {                          \
    CHECK_GT(g_##hook_type##_calls, 0);                                 \
    g_##hook_type##_calls = 0;  /* reset for next call */               \
  }                                                                     \
  static MallocHook::hook_type g_old_##hook_type;                       \
  static void Set##hook_type() {                                        \
    g_old_##hook_type = MallocHook::Set##hook_type(                     \
     (MallocHook::hook_type)&IncrementCallsTo##hook_type);              \
  }                                                                     \
  static void Reset##hook_type() {                                      \
    CHECK_EQ(MallocHook::Set##hook_type(g_old_##hook_type),             \
             (MallocHook::hook_type)&IncrementCallsTo##hook_type);      \
  }

// We do one for each hook typedef in malloc_hook.h
MAKE_HOOK_CALLBACK(NewHook);
MAKE_HOOK_CALLBACK(DeleteHook);
MAKE_HOOK_CALLBACK(MmapHook);
MAKE_HOOK_CALLBACK(MremapHook);
MAKE_HOOK_CALLBACK(MunmapHook);
MAKE_HOOK_CALLBACK(SbrkHook);

static void TestAlignmentForSize(int size) {
  fprintf(LOGSTREAM, "Testing alignment of malloc(%d)\n", size);
  static const int kNum = 100;
  void* ptrs[kNum];
  for (int i = 0; i < kNum; i++) {
    ptrs[i] = malloc(size);
    uintptr_t p = reinterpret_cast<uintptr_t>(ptrs[i]);
    CHECK((p % sizeof(void*)) == 0);
    CHECK((p % sizeof(double)) == 0);

    // Must have 16-byte alignment for large enough objects
#ifndef DEBUGALLOCATION    // debug allocation doesn't need to align like this
    if (size >= 16) {
      CHECK((p % 16) == 0);
    }
#endif
  }
  for (int i = 0; i < kNum; i++) {
    free(ptrs[i]);
  }
}

static void TestMallocAlignment() {
  for (int lg = 0; lg < 16; lg++) {
    TestAlignmentForSize((1<<lg) - 1);
    TestAlignmentForSize((1<<lg) + 0);
    TestAlignmentForSize((1<<lg) + 1);
  }
}

static void TestHugeThreadCache() {
  fprintf(LOGSTREAM, "==== Testing huge thread cache\n");
  // More than 2^16 to cause integer overflow of 16 bit counters.
  static const int kNum = 70000;
  char** array = new char*[kNum];
  for (int i = 0; i < kNum; ++i) {
    array[i] = new char[10];
  }
  for (int i = 0; i < kNum; ++i) {
    delete[] array[i];
  }
  delete[] array;
}

static int RunAllTests(int argc, char** argv) {
  // Optional argv[1] is the seed
  AllocatorState rnd(argc > 1 ? atoi(argv[1]) : 100);

  SetTestResourceLimit();

  // TODO(odo):  This test has been disabled because it is only by luck that it
  // does not result in fragmentation.  When tcmalloc makes an allocation which
  // spans previously unused leaves of the pagemap it will allocate and fill in
  // the leaves to cover the new allocation.  The leaves happen to be 256MiB in
  // the 64-bit build, and with the sbrk allocator these allocations just
  // happen to fit in one leaf by luck.  With other allocators (mmap,
  // memfs_malloc when used with small pages) the allocations generally span
  // two leaves and this results in a very bad fragmentation pattern with this
  // code.  The same failure can be forced with the sbrk allocator just by
  // allocating something on the order of 128MiB prior to starting this test so
  // that the test allocations straddle a 256MiB boundary.

  // TODO(csilvers): port MemoryUsage() over so the test can use that
#if 0
# include <unistd.h>      // for getpid()
  // Allocate and deallocate blocks of increasing sizes to check if the alloc
  // metadata fragments the memory. (Do not put other allocations/deallocations
  // before this test, it may break).
  {
    size_t memory_usage = MemoryUsage(getpid());
    fprintf(LOGSTREAM, "Testing fragmentation\n");
    for ( int i = 200; i < 240; ++i ) {
      int size = i << 20;
      void *test1 = rnd.alloc(size);
      CHECK(test1);
      for ( int j = 0; j < size; j += (1 << 12) ) {
        static_cast<char*>(test1)[j] = 1;
      }
      free(test1);
    }
    // There may still be a bit of fragmentation at the beginning, until we
    // reach kPageMapBigAllocationThreshold bytes so we check for
    // 200 + 240 + margin.
    CHECK_LT(MemoryUsage(getpid()), memory_usage + (450 << 20) );
  }
#endif

  // Check that empty allocation works
  fprintf(LOGSTREAM, "Testing empty allocation\n");
  {
    void* p1 = rnd.alloc(0);
    CHECK(p1 != NULL);
    void* p2 = rnd.alloc(0);
    CHECK(p2 != NULL);
    CHECK(p1 != p2);
    free(p1);
    free(p2);
  }

  // This code stresses some of the memory allocation via STL.
  // In particular, it calls operator delete(void*, nothrow_t).
  fprintf(LOGSTREAM, "Testing STL use\n");
  {
    std::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(0);
    std::stable_sort(v.begin(), v.end());
  }

  // Test each of the memory-allocation functions once, just as a sanity-check
  fprintf(LOGSTREAM, "Sanity-testing all the memory allocation functions\n");
  {
    // We use new-hook and delete-hook to verify we actually called the
    // tcmalloc version of these routines, and not the libc version.
    SetNewHook();      // defined as part of MAKE_HOOK_CALLBACK, above
    SetDeleteHook();   // ditto

    void* p1 = malloc(10);
    VerifyNewHookWasCalled();
    free(p1);
    VerifyDeleteHookWasCalled();

    p1 = calloc(10, 2);
    VerifyNewHookWasCalled();
    p1 = realloc(p1, 30);
    VerifyNewHookWasCalled();
    VerifyDeleteHookWasCalled();
    cfree(p1);  // synonym for free
    VerifyDeleteHookWasCalled();

    CHECK_EQ(posix_memalign(&p1, sizeof(p1), 40), 0);
    VerifyNewHookWasCalled();
    free(p1);
    VerifyDeleteHookWasCalled();

    p1 = memalign(sizeof(p1) * 2, 50);
    VerifyNewHookWasCalled();
    free(p1);
    VerifyDeleteHookWasCalled();

    p1 = valloc(60);
    VerifyNewHookWasCalled();
    free(p1);
    VerifyDeleteHookWasCalled();

    p1 = pvalloc(70);
    VerifyNewHookWasCalled();
    free(p1);
    VerifyDeleteHookWasCalled();

    char* p2 = new char;
    VerifyNewHookWasCalled();
    delete p2;
    VerifyDeleteHookWasCalled();

    p2 = new char[100];
    VerifyNewHookWasCalled();
    delete[] p2;
    VerifyDeleteHookWasCalled();

    p2 = new(std::nothrow) char;
    VerifyNewHookWasCalled();
    delete p2;
    VerifyDeleteHookWasCalled();

    p2 = new(std::nothrow) char[100];
    VerifyNewHookWasCalled();
    delete[] p2;
    VerifyDeleteHookWasCalled();

    // Another way of calling operator new
    p2 = static_cast<char*>(::operator new(100));
    VerifyNewHookWasCalled();
    ::operator delete(p2);
    VerifyDeleteHookWasCalled();

    // Try to call nothrow's delete too.  Compilers use this.
    p2 = static_cast<char*>(::operator new(100, std::nothrow));
    VerifyNewHookWasCalled();
    ::operator delete(p2, std::nothrow);
    VerifyDeleteHookWasCalled();

    // Test mmap too: both anonymous mmap and mmap of a file
    // Note that for right now we only override mmap on linux
    // systems, so those are the only ones for which we check.
    SetMmapHook();
    SetMremapHook();
    SetMunmapHook();
#if defined(HAVE_MMAP) && defined(__linux) && \
       (defined(__i386__) || defined(__x86_64__))
    int size = 8192*2;
    p1 = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_ANONYMOUS|MAP_PRIVATE,
              -1, 0);
    VerifyMmapHookWasCalled();
    p1 = mremap(p1, size, size/2, 0);
    VerifyMremapHookWasCalled();
    size /= 2;
    munmap(p1, size);
    VerifyMunmapHookWasCalled();

    int fd = open("/dev/zero", O_RDONLY);
    CHECK_GE(fd, 0);   // make sure the open succeeded
    p1 = mmap(NULL, 8192, PROT_READ, MAP_SHARED, fd, 0);
    VerifyMmapHookWasCalled();
    munmap(p1, 8192);
    VerifyMunmapHookWasCalled();
    close(fd);
#else   // this is just to quiet the compiler: make sure all fns are called
    IncrementCallsToMmapHook();
    IncrementCallsToMunmapHook();
    IncrementCallsToMremapHook();
    VerifyMmapHookWasCalled();
    VerifyMremapHookWasCalled();
    VerifyMunmapHookWasCalled();
#endif

    // Test sbrk
    SetSbrkHook();
#if defined(HAVE_SBRK) && defined(__linux) && \
       (defined(__i386__) || defined(__x86_64__))
    p1 = sbrk(8192);
    VerifySbrkHookWasCalled();
    p1 = sbrk(-8192);
    VerifySbrkHookWasCalled();
    // However, sbrk hook should *not* be called with sbrk(0)
    p1 = sbrk(0);
    CHECK_EQ(g_SbrkHook_calls, 0);
#else   // this is just to quiet the compiler: make sure all fns are called
    IncrementCallsToSbrkHook();
    VerifySbrkHookWasCalled();
#endif

    // Reset the hooks to what they used to be.  These are all
    // defined as part of MAKE_HOOK_CALLBACK, above.
    ResetNewHook();
    ResetDeleteHook();
    ResetMmapHook();
    ResetMremapHook();
    ResetMunmapHook();
    ResetSbrkHook();
  }

  // Check that "lots" of memory can be allocated
  fprintf(LOGSTREAM, "Testing large allocation\n");
  {
    const int mb_to_allocate = 100;
    void* p = rnd.alloc(mb_to_allocate << 20);
    CHECK(p != NULL);  // could not allocate
    free(p);
  }

  TestMallocAlignment();

  // Check calloc() with various arguments
  fprintf(LOGSTREAM, "Testing calloc\n");
  TestCalloc(0, 0, true);
  TestCalloc(0, 1, true);
  TestCalloc(1, 1, true);
  TestCalloc(1<<10, 0, true);
  TestCalloc(1<<20, 0, true);
  TestCalloc(0, 1<<10, true);
  TestCalloc(0, 1<<20, true);
  TestCalloc(1<<20, 2, true);
  TestCalloc(2, 1<<20, true);
  TestCalloc(1000, 1000, true);

  TestCalloc(kMaxSize, 2, false);
  TestCalloc(2, kMaxSize, false);
  TestCalloc(kMaxSize, kMaxSize, false);

  TestCalloc(kMaxSignedSize, 3, false);
  TestCalloc(3, kMaxSignedSize, false);
  TestCalloc(kMaxSignedSize, kMaxSignedSize, false);

  // Test that realloc doesn't always reallocate and copy memory.
  fprintf(LOGSTREAM, "Testing realloc\n");
  TestRealloc();

  fprintf(LOGSTREAM, "Testing operator new(nothrow).\n");
  TestNothrowNew(&::operator new);
  fprintf(LOGSTREAM, "Testing operator new[](nothrow).\n");
  TestNothrowNew(&::operator new[]);
  fprintf(LOGSTREAM, "Testing operator new.\n");
  TestNew(&::operator new);
  fprintf(LOGSTREAM, "Testing operator new[].\n");
  TestNew(&::operator new[]);

  // Create threads
  fprintf(LOGSTREAM, "Testing threaded allocation/deallocation (%d threads)\n",
          FLAGS_numthreads);
  threads = new TesterThread*[FLAGS_numthreads];
  for (int i = 0; i < FLAGS_numthreads; ++i) {
    threads[i] = new TesterThread(i);
  }

  // This runs all the tests at the same time, with a 1M stack size each
  RunManyThreadsWithId(RunThread, FLAGS_numthreads, 1<<20);

  for (int i = 0; i < FLAGS_numthreads; ++i) delete threads[i];    // Cleanup

  // Do the memory intensive tests after threads are done, since exhausting
  // the available address space can make pthread_create to fail.

  // Check that huge allocations fail with NULL instead of crashing
  fprintf(LOGSTREAM, "Testing huge allocations\n");
  TestHugeAllocations(&rnd);

  // Check that large allocations fail with NULL instead of crashing
#ifndef DEBUGALLOCATION    // debug allocation takes forever for huge allocs
  fprintf(LOGSTREAM, "Testing out of memory\n");
  for (int s = 0; ; s += (10<<20)) {
    void* large_object = rnd.alloc(s);
    if (large_object == NULL) break;
    free(large_object);
  }
#endif

  TestHugeThreadCache();

  return 0;
}

}

using testing::RunAllTests;

int main(int argc, char** argv) {
  RunAllTests(argc, argv);

  // Test tc_version()
  fprintf(LOGSTREAM, "Testing tc_version()\n");
  int major;
  int minor;
  const char* patch;
  char mmp[64];
  const char* human_version = tc_version(&major, &minor, &patch);
  snprintf(mmp, sizeof(mmp), "%d.%d%s", major, minor, patch);
  CHECK(!strcmp(PACKAGE_STRING, human_version));
  CHECK(!strcmp(PACKAGE_VERSION, mmp));

  fprintf(LOGSTREAM, "PASS\n");
}
