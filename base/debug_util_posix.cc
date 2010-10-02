// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#endif

#include <iostream>

#include "base/basictypes.h"
#include "base/compat_execinfo.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/safe_strerror_posix.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"

#if defined(USE_SYMBOLIZE)
#include "base/third_party/symbolize/symbolize.h"
#endif

namespace {
// The prefix used for mangled symbols, per the Itanium C++ ABI:
// http://www.codesourcery.com/cxx-abi/abi.html#mangling
const char kMangledSymbolPrefix[] = "_Z";

// Characters that can be used for symbols, generated by Ruby:
// (('a'..'z').to_a+('A'..'Z').to_a+('0'..'9').to_a + ['_']).join
const char kSymbolCharacters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

#if !defined(USE_SYMBOLIZE)
// Demangles C++ symbols in the given text. Example:
//
// "sconsbuild/Debug/base_unittests(_ZN10StackTraceC1Ev+0x20) [0x817778c]"
// =>
// "sconsbuild/Debug/base_unittests(StackTrace::StackTrace()+0x20) [0x817778c]"
void DemangleSymbols(std::string* text) {
#if defined(__GLIBCXX__)

  std::string::size_type search_from = 0;
  while (search_from < text->size()) {
    // Look for the start of a mangled symbol, from search_from.
    std::string::size_type mangled_start =
        text->find(kMangledSymbolPrefix, search_from);
    if (mangled_start == std::string::npos) {
      break;  // Mangled symbol not found.
    }

    // Look for the end of the mangled symbol.
    std::string::size_type mangled_end =
        text->find_first_not_of(kSymbolCharacters, mangled_start);
    if (mangled_end == std::string::npos) {
      mangled_end = text->size();
    }
    std::string mangled_symbol =
        text->substr(mangled_start, mangled_end - mangled_start);

    // Try to demangle the mangled symbol candidate.
    int status = 0;
    scoped_ptr_malloc<char> demangled_symbol(
        abi::__cxa_demangle(mangled_symbol.c_str(), NULL, 0, &status));
    if (status == 0) {  // Demangling is successful.
      // Remove the mangled symbol.
      text->erase(mangled_start, mangled_end - mangled_start);
      // Insert the demangled symbol.
      text->insert(mangled_start, demangled_symbol.get());
      // Next time, we'll start right after the demangled symbol we inserted.
      search_from = mangled_start + strlen(demangled_symbol.get());
    } else {
      // Failed to demangle.  Retry after the "_Z" we just found.
      search_from = mangled_start + 2;
    }
  }

#endif  // defined(__GLIBCXX__)
}
#endif  // !defined(USE_SYMBOLIZE)

// Gets the backtrace as a vector of strings. If possible, resolve symbol
// names and attach these. Otherwise just use raw addresses. Returns true
// if any symbol name is resolved.  Returns false on error and *may* fill
// in |error_message| if an error message is available.
bool GetBacktraceStrings(void **trace, int size,
                         std::vector<std::string>* trace_strings,
                         std::string* error_message) {
  bool symbolized = false;

#if defined(USE_SYMBOLIZE)
  for (int i = 0; i < size; ++i) {
    char symbol[1024];
    // Subtract by one as return address of function may be in the next
    // function when a function is annotated as noreturn.
    if (google::Symbolize(static_cast<char *>(trace[i]) - 1,
                          symbol, sizeof(symbol))) {
      // Don't call DemangleSymbols() here as the symbol is demangled by
      // google::Symbolize().
      trace_strings->push_back(
          base::StringPrintf("%s [%p]", symbol, trace[i]));
      symbolized = true;
    } else {
      trace_strings->push_back(base::StringPrintf("%p", trace[i]));
    }
  }
#else
  scoped_ptr_malloc<char*> trace_symbols(backtrace_symbols(trace, size));
  if (trace_symbols.get()) {
    for (int i = 0; i < size; ++i) {
      std::string trace_symbol = trace_symbols.get()[i];
      DemangleSymbols(&trace_symbol);
      trace_strings->push_back(trace_symbol);
    }
    symbolized = true;
  } else {
    if (error_message)
      *error_message = safe_strerror(errno);
    for (int i = 0; i < size; ++i) {
      trace_strings->push_back(base::StringPrintf("%p", trace[i]));
    }
  }
#endif  // defined(USE_SYMBOLIZE)

  return symbolized;
}

}  // namespace

// static
bool DebugUtil::SpawnDebuggerOnProcess(unsigned /* process_id */) {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX)

// Based on Apple's recommended method as described in
// http://developer.apple.com/qa/qa2004/qa1361.html
// static
bool DebugUtil::BeingDebugged() {
  // If the process is sandboxed then we can't use the sysctl, so cache the
  // value.
  static bool is_set = false;
  static bool being_debugged = false;

  if (is_set) {
    return being_debugged;
  }

  // Initialize mib, which tells sysctl what info we want.  In this case,
  // we're looking for information about a specific process ID.
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid()
  };

  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.  The source and
  // binary interfaces may change.
  struct kinfo_proc info;
  size_t info_size = sizeof(info);

  int sysctl_result = sysctl(mib, arraysize(mib), &info, &info_size, NULL, 0);
  DCHECK_EQ(sysctl_result, 0);
  if (sysctl_result != 0) {
    is_set = true;
    being_debugged = false;
    return being_debugged;
  }

  // This process is being debugged if the P_TRACED flag is set.
  is_set = true;
  being_debugged = (info.kp_proc.p_flag & P_TRACED) != 0;
  return being_debugged;
}

#elif defined(OS_LINUX)

// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
// static
bool DebugUtil::BeingDebugged() {
  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf)));
  if (HANDLE_EINTR(close(status_fd)) < 0)
    return false;

  if (num_read <= 0)
    return false;

  base::StringPiece status(buf, num_read);
  base::StringPiece tracer("TracerPid:\t");

  base::StringPiece::size_type pid_index = status.find(tracer);
  if (pid_index == base::StringPiece::npos)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += tracer.size();
  return pid_index < status.size() && status[pid_index] != '0';
}

#elif defined(OS_FREEBSD)

bool DebugUtil::BeingDebugged() {
  // TODO(benl): can we determine this under FreeBSD?
  NOTIMPLEMENTED();
  return false;
}

#endif  // defined(OS_FREEBSD)

// We want to break into the debugger in Debug mode, and cause a crash dump in
// Release mode. Breakpad behaves as follows:
//
// +-------+-----------------+-----------------+
// | OS    | Dump on SIGTRAP | Dump on SIGABRT |
// +-------+-----------------+-----------------+
// | Linux |       N         |        Y        |
// | Mac   |       Y         |        N        |
// +-------+-----------------+-----------------+
//
// Thus we do the following:
// Linux: Debug mode, send SIGTRAP; Release mode, send SIGABRT.
// Mac: Always send SIGTRAP.

#if defined(NDEBUG) && !defined(OS_MACOSX)
#define DEBUG_BREAK() abort()
#elif defined(ARCH_CPU_ARM_FAMILY)
#define DEBUG_BREAK() asm("bkpt 0")
#else
#define DEBUG_BREAK() asm("int3")
#endif

// static
void DebugUtil::BreakDebugger() {
  DEBUG_BREAK();
}

StackTrace::StackTrace() {
#if defined(OS_MACOSX) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
  if (backtrace == NULL) {
    count_ = 0;
    return;
  }
#endif
  // Though the backtrace API man page does not list any possible negative
  // return values, we take no chance.
  count_ = std::max(backtrace(trace_, arraysize(trace_)), 0);
}

void StackTrace::PrintBacktrace() {
#if defined(OS_MACOSX) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
  if (backtrace_symbols_fd == NULL)
    return;
#endif
  fflush(stderr);
  std::vector<std::string> trace_strings;
  GetBacktraceStrings(trace_, count_, &trace_strings, NULL);
  for (size_t i = 0; i < trace_strings.size(); ++i) {
    std::cerr << "\t" << trace_strings[i] << "\n";
  }
}

void StackTrace::OutputToStream(std::ostream* os) {
#if defined(OS_MACOSX) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
  if (backtrace_symbols == NULL)
    return;
#endif
  std::vector<std::string> trace_strings;
  std::string error_message;
  if (GetBacktraceStrings(trace_, count_, &trace_strings, &error_message)) {
    (*os) << "Backtrace:\n";
  } else {
    if (!error_message.empty())
      error_message = " (" + error_message + ")";
    (*os) << "Unable to get symbols for backtrace" << error_message << ". "
          << "Dumping raw addresses in trace:\n";
  }

  for (size_t i = 0; i < trace_strings.size(); ++i) {
    (*os) << "\t" << trace_strings[i] << "\n";
  }
}
