// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
#define CHROME_FRAME_CHROME_FRAME_DELEGATE_H_

#include <atlbase.h>
#include <atlwin.h>
#include <queue>

#include "base/file_path.h"
#include "base/lock.h"
#include "chrome/test/automation/automation_messages.h"
#include "ipc/ipc_message.h"

// A common interface supported by all the browser specific ChromeFrame
// implementations.
class ChromeFrameDelegate {
 public:
  typedef HWND WindowType;

  virtual WindowType GetWindow() const = 0;
  virtual void GetBounds(RECT* bounds) = 0;
  virtual std::string GetDocumentUrl() = 0;
  virtual void OnAutomationServerReady() = 0;
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) = 0;
  virtual void OnExtensionInstalled(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues response) = 0;
  virtual void OnGetEnabledExtensionsComplete(
      void* user_data,
      const std::vector<FilePath>& extension_directories) = 0;
  virtual void OnMessageReceived(const IPC::Message& msg) = 0;

  // This remains in interface since we call it if Navigate()
  // returns immediate error.
  virtual void OnLoadFailed(int error_code, const std::string& url) = 0;

  // Returns true if this instance is alive and well for processing automation
  // messages.
  virtual bool IsValid() const = 0;

  // To be called when the top-most window of an application hosting
  // ChromeFrame is moved.
  virtual void OnHostMoved() = 0;

 protected:
  ~ChromeFrameDelegate() {}
};

// Template specialization
template <> struct RunnableMethodTraits<ChromeFrameDelegate> {
  void RetainCallee(ChromeFrameDelegate* obj) {}
  void ReleaseCallee(ChromeFrameDelegate* obj) {}
};

extern UINT kAutomationServerReady;
extern UINT kMessageFromChromeFrame;

class ChromeFrameDelegateImpl : public ChromeFrameDelegate {
 public:
  virtual WindowType GetWindow() { return NULL; }
  virtual void GetBounds(RECT* bounds) {}
  virtual std::string GetDocumentUrl() { return std::string(); }
  virtual void OnAutomationServerReady() {}
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {}
  virtual void OnExtensionInstalled(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues response) {}
  virtual void OnGetEnabledExtensionsComplete(
      void* user_data,
      const std::vector<FilePath>& extension_directories) {}
  virtual void OnLoadFailed(int error_code, const std::string& url) {}
  virtual void OnMessageReceived(const IPC::Message& msg);

  static bool IsTabMessage(const IPC::Message& message, int* tab_handle);

  virtual bool IsValid() const {
    return true;
  }

  virtual void OnHostMoved() {}

 protected:
  // Protected methods to be overriden.
  virtual void OnNavigationStateChanged(int tab_handle, int flags,
                                        const IPC::NavigationInfo& nav_info) {}
  virtual void OnUpdateTargetUrl(int tab_handle,
                                 const std::wstring& new_target_url) {}
  virtual void OnAcceleratorPressed(int tab_handle, const MSG& accel_message) {}
  virtual void OnTabbedOut(int tab_handle, bool reverse) {}
  virtual void OnOpenURL(int tab_handle, const GURL& url,
                         const GURL& referrer, int open_disposition) {}
  virtual void OnDidNavigate(int tab_handle,
                             const IPC::NavigationInfo& navigation_info) {}
  virtual void OnNavigationFailed(int tab_handle, int error_code,
                                  const GURL& gurl) {}
  virtual void OnLoad(int tab_handle, const GURL& url) {}
  virtual void OnMessageFromChromeFrame(int tab_handle,
                                        const std::string& message,
                                        const std::string& origin,
                                        const std::string& target) {}
  virtual void OnHandleContextMenu(int tab_handle, HANDLE menu_handle,
                                   int align_flags,
                                   const IPC::ContextMenuParams& params) {}
  virtual void OnRequestStart(int tab_handle, int request_id,
                              const IPC::AutomationURLRequest& request) {}
  virtual void OnRequestRead(int tab_handle, int request_id,
                             int bytes_to_read) {}
  virtual void OnRequestEnd(int tab_handle, int request_id,
                            const URLRequestStatus& status) {}
  virtual void OnDownloadRequestInHost(int tab_handle, int request_id) {}
  virtual void OnSetCookieAsync(int tab_handle, const GURL& url,
                                const std::string& cookie) {}
  virtual void OnAttachExternalTab(int tab_handle,
      const IPC::AttachExternalTabParams& attach_params) {}
  virtual void OnGoToHistoryEntryOffset(int tab_handle, int offset) {}

  virtual void OnGetCookiesFromHost(int tab_handle, const GURL& url,
                                    int cookie_id) {}
};

// This interface enables tasks to be marshalled to desired threads.
class TaskMarshaller {
 public:
  virtual void PostTask(const tracked_objects::Location& from_here,
                        Task* task) = 0;
};

// T is expected to be something CWindowImpl derived, or at least to have
// PostMessage(UINT, WPARAM) method. Do not forget to CHAIN_MSG_MAP
template <class T> class TaskMarshallerThroughWindowsMessages
    : public TaskMarshaller {
 public:
  TaskMarshallerThroughWindowsMessages() {}
  virtual void PostTask(const tracked_objects::Location& from_here,
                        Task* task) {
    task->SetBirthPlace(from_here);
    T* this_ptr = static_cast<T*>(this);
    if (this_ptr->IsWindow()) {
      this_ptr->AddRef();
      PushTask(task);
      this_ptr->PostMessage(MSG_EXECUTE_TASK, reinterpret_cast<WPARAM>(task));
    } else {
      DLOG(INFO) << "Dropping MSG_EXECUTE_TASK message for destroyed window.";
      delete task;
    }
  }

 protected:
  ~TaskMarshallerThroughWindowsMessages() {
    DeleteAllPendingTasks();
  }

  void DeleteAllPendingTasks() {
    AutoLock lock(lock_);
    DLOG_IF(INFO, !pending_tasks_.empty()) << "Destroying " <<
      pending_tasks_.size() << "  pending tasks";
    while (!pending_tasks_.empty()) {
      Task* task = pending_tasks_.front();
      pending_tasks_.pop();
      delete task;
    }
  }

  BEGIN_MSG_MAP(PostMessageMarshaller)
    MESSAGE_HANDLER(MSG_EXECUTE_TASK, ExecuteTask)
  END_MSG_MAP()

 private:
  enum { MSG_EXECUTE_TASK = WM_APP + 6 };
  inline LRESULT ExecuteTask(UINT, WPARAM wparam, LPARAM,
                             BOOL& handled) {  // NOLINT
    Task* task = reinterpret_cast<Task*>(wparam);
    if (task && PopTask(task)) {
      task->Run();
      delete task;
    }

    T* this_ptr = static_cast<T*>(this);
    this_ptr->Release();
    return 0;
  }

  inline void PushTask(Task* task) {
    AutoLock lock(lock_);
    pending_tasks_.push(task);
  }

  // If the given task is front of the queue, removes the task and returns true,
  // otherwise we assume this is an already destroyed task (but Window message
  // had remained in the thread queue).
  inline bool PopTask(Task* task) {
    AutoLock lock(lock_);
    if (!pending_tasks_.empty() && task == pending_tasks_.front()) {
      pending_tasks_.pop();
      return true;
    }

    return false;
  }

  Lock lock_;
  std::queue<Task*> pending_tasks_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
