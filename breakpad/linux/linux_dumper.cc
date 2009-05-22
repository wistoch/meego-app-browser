// Copyright (c) 2009, Google Inc.
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

// This code deals with the mechanics of getting information about a crashed
// process. Since this code may run in a compromised address space, the same
// rules apply as detailed at the top of minidump_writer.h: no libc calls and
// use the alternative allocator.

#include "breakpad/linux/linux_dumper.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "breakpad/linux/directory_reader.h"
#include "breakpad/linux/line_reader.h"
#include "breakpad/linux/linux_libc_support.h"
#include "breakpad/linux/linux_syscall_support.h"

// Suspend a thread by attaching to it.
static bool SuspendThread(pid_t pid) {
  // This may fail if the thread has just died or debugged.
  errno = 0;
  if (sys_ptrace(PTRACE_ATTACH, pid, NULL, NULL) != 0 &&
      errno != 0) {
    return false;
  }
  while (sys_waitpid(pid, NULL, __WALL) < 0) {
    if (errno != EINTR) {
      sys_ptrace(PTRACE_DETACH, pid, NULL, NULL);
      return false;
    }
  }
  return true;
}

// Resume a thread by detaching from it.
static bool ResumeThread(pid_t pid) {
  return sys_ptrace(PTRACE_DETACH, pid, NULL, NULL) >= 0;
}

namespace google_breakpad {

LinuxDumper::LinuxDumper(int pid)
    : pid_(pid),
      threads_suspened_(false),
      threads_(&allocator_, 8),
      mappings_(&allocator_) {
}

bool LinuxDumper::Init() {
  return EnumerateThreads(&threads_) &&
         EnumerateMappings(&mappings_);
}

bool LinuxDumper::ThreadsSuspend() {
  if (threads_suspened_)
    return true;
  bool good = true;
  for (size_t i = 0; i < threads_.size(); ++i)
    good &= SuspendThread(threads_[i]);
  threads_suspened_ = true;
  return good;
}

bool LinuxDumper::ThreadsResume() {
  if (!threads_suspened_)
    return false;
  bool good = true;
  for (size_t i = 0; i < threads_.size(); ++i)
    good &= ResumeThread(threads_[i]);
  threads_suspened_ = false;
  return good;
}

bool
LinuxDumper::EnumerateMappings(wasteful_vector<MappingInfo*> *result) const {
  char maps_path[80];
  memcpy(maps_path, "/proc/", 6);
  const unsigned pid_len = my_int_len(pid_);
  my_itos(maps_path + 6, pid_, pid_len);
  memcpy(maps_path + 6 + pid_len, "/maps", 6);

  const int fd = sys_open(maps_path, O_RDONLY, 0);
  if (fd < 0)
    return false;
  LineReader *const line_reader = new(allocator_) LineReader(fd);

  const char *line;
  unsigned line_len;
  while (line_reader->GetNextLine(&line, &line_len)) {
    uintptr_t start_addr;
    uintptr_t end_addr;

    const char* i1 = my_read_hex_ptr(&start_addr, line);
    if (*i1 == '-') {
      const char *i2 = my_read_hex_ptr(&end_addr, i1 + 1);
      if (*i2 == ' ') {
        MappingInfo *const module = new(allocator_) MappingInfo;
        memset(module, 0, sizeof(MappingInfo));
        module->start_addr = start_addr;
        module->size = end_addr - start_addr;
        const char *name = NULL;
        // Only copy name if the name is a valid path name.
        if ((name = my_strchr(line, '/')) != NULL) {
          const unsigned l = my_strlen(name);
          if (l < sizeof(module->name))
            memcpy(module->name, name, l);
        }

        result->push_back(module);
      }
    }

    line_reader->PopLine(line_len);
  }

  sys_close(fd);
  return result->size() > 0;
}

// Parse /proc/$pid/task to list all the threads of the process identified by
// pid.
bool LinuxDumper::EnumerateThreads(wasteful_vector<pid_t> *result) const {
  char task_path[80];
  memcpy(task_path, "/proc/", 6);
  const unsigned pid_len = my_int_len(pid_);
  my_itos(task_path + 6, pid_, pid_len);
  memcpy(task_path + 6 + pid_len, "/task", 6);

  const int fd = sys_open(task_path, O_RDONLY | O_DIRECTORY, 0);
  if (fd < 0)
    return false;
  DirectoryReader *dir_reader = new(allocator_) DirectoryReader(fd);

  // The directory may contain duplicate entries which we filter by assuming
  // that they are consecutive 
  int last_tid = -1;
  const char *dent_name;
  while (dir_reader->GetNextEntry(&dent_name)) {
    if (my_strcmp(dent_name, ".") &&
        my_strcmp(dent_name, "..")) {
      int tid = 0;
      if (my_strtoui(&tid, dent_name) &&
          last_tid != tid) {
        last_tid = tid;
        result->push_back(tid);
      }
    }
    dir_reader->PopEntry();
  }

  sys_close(fd);
  return true;
}

// Read thread info from /proc/$pid/status.
// Fill out the |tgid|, |ppid| and |pid| members of |info|. If unavailible,
// these members are set to -1. Returns true iff all three members are
// availible.
bool LinuxDumper::ThreadInfoGet(pid_t tid, ThreadInfo *info) {
  assert(info != NULL);
  char status_path[80];
  memcpy(status_path, "/proc/", 6);
  const unsigned tid_len = my_int_len(tid);
  my_itos(status_path + 6, tid, tid_len);
  memcpy(status_path + 6 + tid_len, "/status", 8);
  const int fd = open(status_path, O_RDONLY);
  if (fd < 0)
    return false;

  LineReader *const line_reader = new(allocator_) LineReader(fd);
  const char *line;
  unsigned line_len;

  info->ppid = info->tgid = -1;

  while (line_reader->GetNextLine(&line, &line_len)) {
    if (my_strncmp("Tgid:\t", line, 6) == 0) {
      my_strtoui(&info->tgid, line + 6);
    } else if (my_strncmp("PPid:\t", line, 6) == 0) {
      my_strtoui(&info->ppid, line + 6);
    }

    line_reader->PopLine(line_len);
  }

  if (info->ppid == -1 || info->tgid == -1)
    return false;

  if (sys_ptrace(PTRACE_GETREGS, tid, NULL, &info->regs) == -1 ||
      sys_ptrace(PTRACE_GETFPREGS, tid, NULL, &info->fpregs) == -1) {
    return false;
  }

#if defined(__i386) || defined(__x86_64)
  if (sys_ptrace(PTRACE_GETFPXREGS, tid, NULL, &info->fpxregs) == -1)
    return false;

  for (unsigned i = 0; i < ThreadInfo::kNumDebugRegisters; ++i) {
    if (sys_ptrace(
        PTRACE_PEEKUSER, tid,
        (void *) (offsetof(struct user,
                           u_debugreg[0]) + i * sizeof(debugreg_t)),
        &info->dregs[i]) == -1) {
      return false;
    }
  }
#endif

  const uint8_t *stack_pointer;
#if defined(__i386)
  memcpy(&stack_pointer, &info->regs.esp, sizeof(info->regs.esp));
#elif defined(__x86_64)
  memcpy(&stack_pointer, &info->regs.rsp, sizeof(info->regs.rsp));
#else
#error "This code hasn't been ported to your platform yet."
#endif

  if (!GetStackInfo(&info->stack, &info->stack_len,
                    (uintptr_t) stack_pointer))
    return false;

  return true;
}

// Get information about the stack, given the stack pointer. We don't try to
// walk the stack since we might not have all the information needed to do
// unwind. So we just grab, up to, 32k of stack.
bool LinuxDumper::GetStackInfo(const void** stack, size_t* stack_len,
                               uintptr_t int_stack_pointer) {
#if defined(__i386) || defined(__x86_64)
  static const bool stack_grows_down = true;
  static const uintptr_t page_size = 4096;
#else
#error "This code has not been ported to your platform yet."
#endif
  // Move the stack pointer to the bottom of the page that it's in.
  uint8_t* const stack_pointer =
      (uint8_t *) (int_stack_pointer & ~(page_size - 1));

  // The number of bytes of stack which we try to capture.
  static unsigned kStackToCapture = 32 * 1024;

  const MappingInfo *mapping = FindMapping(stack_pointer);
  if (!mapping)
    return false;
  if (stack_grows_down) {
    const ptrdiff_t offset = stack_pointer - (uint8_t*) mapping->start_addr;
    const ptrdiff_t distance_to_end =
        static_cast<ptrdiff_t>(mapping->size) - offset;
    *stack_len = distance_to_end > kStackToCapture ?
                 kStackToCapture : distance_to_end;
    *stack = stack_pointer;
  } else {
    const ptrdiff_t offset = stack_pointer - (uint8_t*) mapping->start_addr;
    *stack_len = offset > kStackToCapture ? kStackToCapture : offset;
    *stack = stack_pointer - *stack_len;
  }

  return true;
}

// static
void LinuxDumper::CopyFromProcess(void* dest, pid_t child, const void* src,
                                  size_t length) {
  unsigned long tmp;
  size_t done = 0;
  static const size_t word_size = sizeof(tmp);
  uint8_t* const local = (uint8_t*) dest;
  uint8_t* const remote = (uint8_t*) src;

  while (done < length) {
    const size_t l = length - done > word_size ? word_size : length - done;
    if (sys_ptrace(PTRACE_PEEKDATA, child, remote + done, &tmp) == -1)
      tmp = 0;
    memcpy(local + done, &tmp, l);
    done += l;
  }
}

// Find the mapping which the given memory address falls in.
const MappingInfo *LinuxDumper::FindMapping(const void *address) const {
  const uintptr_t addr = (uintptr_t) address;

  for (size_t i = 0; i < mappings_.size(); ++i) {
    const uintptr_t start = static_cast<uintptr_t>(mappings_[i]->start_addr);
    if (addr >= start && addr - start < mappings_[i]->size)
      return mappings_[i];
  }

  return NULL;
}

}  // namespace google_breakpad
