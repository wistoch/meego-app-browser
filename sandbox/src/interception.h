// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines InterceptionManager, the class in charge of setting up interceptions
// for the sandboxed process. For more datails see
// http://wiki/Main/ChromeSandboxInterceptionDesign

#ifndef SANDBOX_SRC_INTERCEPTION_H__
#define SANDBOX_SRC_INTERCEPTION_H__

#include <list>
#include <string>

#include "base/basictypes.h"
#include "sandbox/src/sandbox_types.h"

#pragma warning(push, 3)
#include "testing/gtest/include/gtest/gtest.h"
#pragma warning(pop)

namespace sandbox {

class TargetProcess;

// Internal structures used for communication between the broker and the target.
struct DllPatchInfo;
struct DllInterceptionData;

// The InterceptionManager executes on the parent application, and it is in
// charge of setting up the desired interceptions, and placing the Interception
// Agent into the child application.
//
// The exposed API consists of two methods: AddToPatchedFunctions to set up a
// particular interception, and InitializeInterceptions to actually go ahead and
// perform all interceptions and transfer data to the child application.
//
// The typical usage is something like this:
//
// InterceptionManager interception_manager(child);
// if (!interception_manager.AddToPatchedFunctions(
//         L"ntdll.dll", "NtCreateFile",
//         sandbox::INTERCEPTION_SERVICE_CALL, &MyNtCreateFile))
//   return false;
//
// if (!interception_manager.AddToPatchedFunctions(
//         L"kernel32.dll", "CreateDirectoryW",
//         sandbox::INTERCEPTION_EAT, L"MyCreateDirectoryW@12"))
//   return false;
//
// if (!interception_manager.InitializeInterceptions()) {
//   DWORD error = ::GetLastError();
//   return false;
// }
//
// Any required syncronization must be performed outside this class. Also, it is
// not possible to perform further interceptions after InitializeInterceptions
// is called.
//
class InterceptionManager {
  // The unit test will access private members.
  FRIEND_TEST(InterceptionManagerTest, BufferLayout);

 public:
  // An interception manager performs interceptions on a given child process.
  // If we are allowed to intercept functions that have been patched by somebody
  // else, relaxed should be set to true.
  // Note: We increase the child's reference count internally.
  InterceptionManager(TargetProcess* child_process, bool relaxed);
  ~InterceptionManager();

  // Patches function_name inside dll_name to point to replacement_code_address.
  // function_name has to be an exported symbol of dll_name.
  // Returns true on success.
  //
  // The new function should match the prototype and calling convention of the
  // function to intercept except for one extra argument (the first one) that
  // contains a pointer to the original function, to simplify the development
  // of interceptors.
  //
  // For example, to intercept NtClose, the following code could be used:
  //
  // typedef NTSTATUS (WINAPI *NtCloseFunction) (IN HANDLE Handle);
  // NTSTATUS WINAPI MyNtCose (IN NtCloseFunction OriginalClose,
  //                           IN HANDLE Handle) {
  //   // do something
  //   // call the original function
  //   return OriginalClose(Handle);
  // }
  bool AddToPatchedFunctions(const wchar_t* dll_name,
                             const char* function_name,
                             InterceptionType interception_type,
                             const void* replacement_code_address);

  // Patches function_name inside dll_name to point to
  // replacement_function_name.
  bool AddToPatchedFunctions(const wchar_t* dll_name,
                             const char* function_name,
                             InterceptionType interception_type,
                             const char* replacement_function_name);

  // Initializes all interceptions on the client.
  // Returns true on success.
  //
  // The child process must be created suspended, and cannot be resumed until
  // after this method returns. In addition, no action should be perfomed on
  // the child that may cause it to resume momentarily, such as injecting
  // threads or APCs.
  //
  // This function must be called only once, after all interceptions have been
  // set up using AddToPatchedFunctions.
  bool InitializeInterceptions();

 private:
  // Used to store the interception information until the actual set-up.
  struct InterceptionData {
    InterceptionType type;            // Interception type
    std::wstring dll;                 // Name of dll to intercept
    std::string function;             // Name of function to intercept
    std::string interceptor;          // Name of interceptor function
    const void* interceptor_address;  // Interceptor's entry point
  };

  // Calculates the size of the required configuration buffer.
  size_t GetBufferSize() const;

  // Rounds up the size of a given buffer, considering alignment (padding).
  // value is the current size of the buffer, and alignment is specified in
  // bytes.
  static inline size_t RoundUpToMultiple(size_t value, size_t alignment) {
    return ((value + alignment -1) / alignment) * alignment;
  }

  // Sets up a given buffer with all the information that has to be transfered
  // to the child.
  // Returns true on success.
  //
  // The buffer size should be at least the value returned by GetBufferSize
  bool SetupConfigBuffer(void* buffer, size_t buffer_bytes);

  // Fills up the part of the transfer buffer that corresponds to information
  // about one dll to patch.
  // data is the first recorded interception for this dll.
  // Returns true on success.
  //
  // On successful return, buffer will be advanced from it's current position
  // to the point where the next block of configuration data should be written
  // (the actual interception info), and the current size of the buffer will
  // decrease to account the space used by this method.
  bool SetupDllInfo(const InterceptionData& data,
                    void** buffer, size_t* buffer_bytes) const;

  // Fills up the part of the transfer buffer that corresponds to a single
  // function to patch.
  // dll_info points to the dll being updated with the interception stored on
  // data. The buffer pointer and remaining size are updated by this call.
  // Returns true on success.
  bool SetupInterceptionInfo(const InterceptionData& data, void** buffer,
                             size_t* buffer_bytes,
                             DllPatchInfo* dll_info) const;

  // Returns true if this interception is to be performed by the child
  // as opposed to from the parent.
  bool IsInterceptionPerformedByChild(const InterceptionData& data) const;

  // Allocates a buffer on the child's address space (returned on
  // remote_buffer), and fills it with the contents of a local buffer.
  // Returns true on success.
  bool CopyDataToChild(const void* local_buffer, size_t buffer_bytes,
                       void** remote_buffer) const;

  // Performs the cold patch (from the parent) of ntdll.dll.
  // Returns true on success.
  //
  // This method will inser aditional interceptions to launch the interceptor
  // agent on the child process, if there are additional interceptions to do.
  bool PatchNtdll(bool hot_patch_needed);

  // Peforms the actual interceptions on ntdll.
  // thunks is the memory to store all the thunks for this dll (on the child),
  // and dll_data is a local buffer to hold global dll interception info.
  // Returns true on success.
  bool PatchClientFunctions(DllInterceptionData* thunks,
                            size_t thunk_bytes,
                            DllInterceptionData* dll_data);

  // The process to intercept.
  TargetProcess* child_;
  // Holds all interception info until the call to initialize (perform the
  // actual patch).
  std::list<InterceptionData> interceptions_;

  // Keep track of patches added by name.
  bool names_used_;

  // true if we are allowed to patch already-patched functions.
  bool relaxed_;

  DISALLOW_EVIL_CONSTRUCTORS(InterceptionManager);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTION_H__

