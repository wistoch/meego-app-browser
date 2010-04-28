
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.cc : Implementation crash reporting.
#include "chrome_frame/crash_reporting/crash_report.h"

#include "base/basictypes.h"
#include "base/lock.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

// TODO(joshia): factor out common code with chrome used for crash reporting
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
static google_breakpad::ExceptionHandler * g_breakpad = NULL;
static Lock g_breakpad_lock;

// These minidump flag combinations have been tested safe agains the
// DbgHelp.dll version that ships with Windows XP SP2.
const MINIDUMP_TYPE kSmallDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

// Large dump with all process memory.
const MINIDUMP_TYPE kFullDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithFullMemory |  // Full memory from process.
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithHandleData |  // Get all handle information.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

#pragma code_seg(push, ".text$va")
static void veh_segment_start() {}
#pragma code_seg(pop)

#pragma code_seg(push, ".text$vz")
static void veh_segment_end() {}
#pragma code_seg(pop)

// Place code in .text$veh_m.
#pragma code_seg(push, ".text$vm")
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"

// Use Win32 API; use breakpad for dumps; checks for single (current) module.
class CrashHandlerTraits : public Win32VEHTraits,
                           public ModuleOfInterestWithExcludedRegion {
 public:
  CrashHandlerTraits() {}

  // Note that breakpad_lock must be held when this is called.
  void Init(google_breakpad::ExceptionHandler* breakpad, Lock* breakpad_lock) {
    DCHECK(breakpad);
    DCHECK(breakpad_lock);
    breakpad_lock->AssertAcquired();

    Win32VEHTraits::InitializeIgnoredBlocks();
    ModuleOfInterestWithExcludedRegion::SetCurrentModule();
    // Pointers to static (non-extern) functions take the address of the
    // function's first byte, as opposed to an entry in the compiler generated
    // JMP table. In release builds /OPT:REF wipes away the JMP table, but debug
    // builds are not so lucky.
    ModuleOfInterestWithExcludedRegion::SetExcludedRegion(&veh_segment_start,
                                                          &veh_segment_end);
  }

  void Shutdown() {
  }

  inline bool WriteDump(EXCEPTION_POINTERS* p) {
    return WriteMinidumpForException(p);
  }
};

class CrashHandler {
 public:
  CrashHandler() : veh_id_(NULL), handler_(&crash_api_) {}

  // Note that breakpad_lock is used to protect accesses to breakpad and must
  // be held when Init() is called.
  bool Init(google_breakpad::ExceptionHandler* breakpad, Lock* breakpad_lock);

  void Shutdown();
 private:
  VectoredHandlerT<CrashHandlerTraits> handler_;
  CrashHandlerTraits crash_api_;
  void* veh_id_;

  static LONG WINAPI VectoredHandlerEntryPoint(EXCEPTION_POINTERS* exptrs);
};

static CrashHandler g_crash_handler;

// Turn off FPO optimization, so ::RtlCaptureStackBackTrace returns sane result.
#pragma optimize("y", off)
LONG WINAPI CrashHandler::VectoredHandlerEntryPoint(
    EXCEPTION_POINTERS* exptrs) {
  return g_crash_handler.handler_.Handler(exptrs);
}
#pragma optimize("y", on)

#pragma code_seg(pop)

bool CrashHandler::Init(google_breakpad::ExceptionHandler* breakpad,
                        Lock* breakpad_lock) {
  DCHECK(breakpad);
  DCHECK(breakpad_lock);
  breakpad_lock->AssertAcquired();

  if (veh_id_)
    return true;

  void* id = ::AddVectoredExceptionHandler(FALSE, &VectoredHandlerEntryPoint);
  if (id != NULL) {
    veh_id_ = id;
    crash_api_.Init(breakpad, breakpad_lock);
    return true;
  }

  return false;
}

void CrashHandler::Shutdown() {
  if (veh_id_) {
    ::RemoveVectoredExceptionHandler(veh_id_);
    veh_id_ = NULL;
  }

  crash_api_.Shutdown();
}

std::wstring GetCrashServerPipeName(const std::wstring& user_sid) {
  std::wstring pipe_name = kGoogleUpdatePipeName;
  pipe_name += user_sid;
  return pipe_name;
}

bool InitializeVectoredCrashReportingWithPipeName(
    bool full_dump,
    const wchar_t* pipe_name,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info) {
  AutoLock lock(g_breakpad_lock);
  if (g_breakpad)
    return true;

  if (dump_path.empty()) {
    return false;
  }

  // TODO(siggi): Consider switching to kSmallerDumpType post-beta.
  MINIDUMP_TYPE dump_type = full_dump ? kFullDumpType : kLargerDumpType;
  g_breakpad = new google_breakpad::ExceptionHandler(
      dump_path, NULL, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_INVALID_PARAMETER |
      google_breakpad::ExceptionHandler::HANDLER_PURECALL, dump_type,
      pipe_name, client_info);

  if (!g_breakpad)
    return false;

  if (!g_crash_handler.Init(g_breakpad, &g_breakpad_lock)) {
    delete g_breakpad;
    g_breakpad = NULL;
    return false;
  }

  return true;
}

bool InitializeVectoredCrashReporting(
    bool full_dump,
    const wchar_t* user_sid,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info) {
  DCHECK(user_sid);
  DCHECK(client_info);

  std::wstring pipe_name = GetCrashServerPipeName(user_sid);

  return InitializeVectoredCrashReportingWithPipeName(full_dump,
                                                      pipe_name.c_str(),
                                                      dump_path,
                                                      client_info);
}

bool ShutdownVectoredCrashReporting() {
  g_crash_handler.Shutdown();
  AutoLock lock(g_breakpad_lock);
  delete g_breakpad;
  g_breakpad = NULL;
  return true;
}

bool WriteMinidumpForException(EXCEPTION_POINTERS* p) {
  AutoLock lock(g_breakpad_lock);
  bool success = false;
  if (g_breakpad) {
    success = g_breakpad->WriteMinidumpForException(p);
  }
  return success;
}
