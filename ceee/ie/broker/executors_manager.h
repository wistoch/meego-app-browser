// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// ExecutorsManager implementation, an object to keep track of the
// CeeeExecutor objects that were instantiated in destination threads.

#ifndef CEEE_IE_BROKER_EXECUTORS_MANAGER_H_
#define CEEE_IE_BROKER_EXECUTORS_MANAGER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <map>

#include "base/lock.h"
#include "base/singleton.h"

#include "toolband.h"  // NOLINT

// This class is to be used as a single instance for the broker module to
// hold on a map of executor objects per thread that won't go away when
// the instance of the Broker object does.
//
// See the @ref ExecutorsManagerDoc page for more details.

// Manages a map of destination threads to CeeeExecutor interfaces.
class ExecutorsManager {
 public:
  // Identifiers for destination threads where to run executors.
  typedef DWORD ThreadId;

  // To avoid lint errors, even though we are only virtual for unittests.
  virtual ~ExecutorsManager() {}

  // Adds a new executor to the map associated to the given thread_id.
  //
  // @param thread_id The thread for which we want to register a new executor.
  // @param executor  The executor we want to register for the given thread_id.
  // @return S_OK iff we didn't already have an executor, and we had a pending
  //              request to add one for that exact same thread.
  virtual HRESULT RegisterWindowExecutor(ThreadId thread_id,
                                         IUnknown* executor);
  // TODO(mad@chromium.org): Implement the proper manual/secure registration.
  //
  // @param thread_id The thread for which we want to register a new executor.
  // @param executor  The executor we want to register for the given thread_id.
  // @return S_OK iff we didn't already have an executor, and we had a pending
  //              request to add one for that exact same thread.
  virtual HRESULT RegisterTabExecutor(ThreadId thread_id, IUnknown* executor);

  // Gets the executor associated to the given thread_id. Gets if from the map
  // if there was already one in there or create a new one otherwise.
  //
  // @param thread_id The thread for which we want the executor.
  // @param window The window handle for which we want the executor.
  // @param riid Which interface is to be returned in @p executor.
  // @param executor  Where to return the pointer to that executor.
  // @return S_OK iff we found an existing or successfully created an executor.
  virtual HRESULT GetExecutor(ThreadId thread_id, HWND window, REFIID riid,
                              void** executor);

  // Removes an executor from our map.
  //
  // @param thread_id The thread for which we want to remove the executor.
  // @return S_OK if we removed the executor or S_FALSE if it wasn't there.
  virtual HRESULT RemoveExecutor(ThreadId thread_id);

  // Terminates the usage of the map by freeing our resources.
  virtual HRESULT Terminate();

  // Return a tab handle associated with the id.
  //
  // @param tab_id The tab identifier.
  // @return The corresponding HWND (or INVALID_HANDLE_VALUE if tab_id isn't
  //         found).
  virtual HWND GetTabHandleFromId(int tab_id);

  // Return a tab id associated with the HWND.
  //
  // @param tab_handle The tab HWND.
  // @return The corresponding tab id (or 0 if tab_handle isn't found).
  virtual int GetTabIdFromHandle(HWND tab_handle);

  // Register the relation between a tab_id and a HWND.
  virtual void SetTabIdForHandle(long tab_id, HWND tab_handle);

  // Unregister the HWND and its corresponding tab_id.
  virtual void DeleteTabHandle(HWND handle);

  // Traits for Singleton<ExecutorsManager> so that we can pass an argument
  // to the constructor.
  struct SingletonTraits {
    static ExecutorsManager* New() {
      return new ExecutorsManager(false);  // By default, we want a thread.
    }
    static void Delete(ExecutorsManager* x) {
      delete x;
    }
    static const bool kRegisterAtExit = true;
  };

 protected:
  // The data we pass to start our worker thread.
  // THERE IS A COPY OF THIS CLASS IN THE UNITTEST WHICH YOU NEED TO UPDATE IF
  // you change this one...
  struct ThreadStartData {
    ExecutorsManager* me;
    CHandle thread_started_gate;
  };

  // A structures holding on the info about an executor and thread it runs in.
  struct ExecutorInfo {
    ExecutorInfo(IUnknown* new_executor = NULL, HANDLE handle = NULL)
        : executor(new_executor), thread_handle(handle) {
    }
    ExecutorInfo(const ExecutorInfo& executor_info)
        : executor(executor_info.executor),
        thread_handle(executor_info.thread_handle) {
    }
    CComPtr<IUnknown> executor;
    // mutable so that we can assign/Detach a const copy.
    mutable CHandle thread_handle;
  };

  typedef std::map<ThreadId, ExecutorInfo> ExecutorsMap;
  typedef std::map<ThreadId, CHandle> Tid2Event;
  typedef std::map<int, HWND> TabIdMap;
  typedef std::map<HWND, int> HandleMap;

  // The index of the termination event in the array of handles we wait for.
  static const size_t kTerminationHandleIndexOffset;

  // The index of the update event in the array of handles we wait for.
  static const size_t kUpdateHandleIndexOffset;

  // The index of the last event in the array of handles we wait for.
  static const size_t kLastHandleIndexOffset;

  // The number of extra handles we used for the events described above.
  static const size_t kExtraHandles;

  // Protected constructor to ensure single instance and initialize some
  // members. Set no_thread for testing...
  explicit ExecutorsManager(bool no_thread);

  // Creates an executor creator in a virtual method so we can override it in
  // Our unit test code.
  //
  // @param executor_creator Where to return the executor creator.
  virtual HRESULT GetExecutorCreator(
      ICeeeExecutorCreator** executor_creator);

  // Returns a list of HANDLEs of threads for which we have an executor.
  //
  // @param thread_handles Where to return at most @p num_threads handles.
  // @param thread_ids Where to return at most @p num_threads ThreadIds.
  // @param num_threads How many handles can fit in @p thread_handles.
  // @return How many handles have been added in @p thread_handles and the same
  //         ThreadIds have been added to @p thread_ids.
  virtual size_t GetThreadHandles(CHandle thread_handles[],
                                  ThreadId thread_ids[], size_t num_threads);

  // A few seams so that we don't have to mock the kernel functions.
  virtual DWORD WaitForSingleObject(HANDLE wait_handle, DWORD timeout);
  virtual DWORD WaitForMultipleObjects(DWORD num_handles,
      const HANDLE* wait_handles, BOOL wait_all, DWORD timeout);

  // The thread procedure that we use to clean up dead threads from the map.
  //
  // @param thread_data A small structure containing this and an event to
  //  signal when the thread has finished initializing itself.
  static DWORD WINAPI ThreadProc(LPVOID thread_data);

  // The map of executor and their thread handle keyed by thread identifiers.
  // Thread protected by lock_.
  ExecutorsMap executors_;

  // We remember the thread identifiers for which we are pending a registration
  // so that we make sure that we only accept registration that we initiate.
  // Also, for each pending registration we must wait on a different event
  // per thread_id that we are waiting for the registration of.
  // Thread protected by ExecutorsManager::lock_.
  Tid2Event pending_registrations_;

  // The mapping between a tab_id and the HWND of the window holding the BHO.
  // In DEBUG, this mapping will grow over time since we don't remove it on
  // DeleteTabHandle. This is useful for debugging as we know if a mapping has
  // been deleted and is invalidly used.
  // Thread protected by ExecutorsManager::lock_.
  TabIdMap tab_id_map_;
  HandleMap handle_map_;

  // The handle to the thread running ThreadProc.
  CHandle thread_;

  // Used to signal the thread to reload the list of thread handles.
  CHandle update_threads_list_gate_;

  // Used to signal the thread to terminate.
  CHandle termination_gate_;

  // To protect the access to the maps (ExecutorsManager::executors_ &
  // ExecutorsManager::pending_registrations_ & tab_id_map_/handle_map_).
  Lock lock_;

  DISALLOW_EVIL_CONSTRUCTORS(ExecutorsManager);
};

#endif  // CEEE_IE_BROKER_EXECUTORS_MANAGER_H_
