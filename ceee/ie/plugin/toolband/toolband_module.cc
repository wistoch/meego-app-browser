// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Declaration of ATL module object and DLL exports.

#include "base/at_exit.h"
#include "base/atomic_ref_count.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/thread.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/install_utils.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/browser_helper_object.h"
#include "ceee/ie/plugin/bho/executor.h"
#include "ceee/ie/plugin/toolband/toolband_module_reporting.h"
#include "ceee/ie/plugin/toolband/tool_band.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "ceee/common/windows_constants.h"
#include "chrome/common/url_constants.h"

#include "toolband.h"  // NOLINT

namespace {

const wchar_t kLogFileName[] = L"ceee.log";

// {73213C1A-C369-4740-A75C-FA849E6CE540}
static const GUID kCeeeIeLogProviderName =
    { 0x73213c1a, 0xc369, 0x4740,
        { 0xa7, 0x5c, 0xfa, 0x84, 0x9e, 0x6c, 0xe5, 0x40 } };

// This is the Script Debugging state for all script engines we instantiate.
ScriptHost::DebugApplication debug_application(L"CEEE");

}  // namespace

// Object entries go here instead of with each object, so that we can move
// the objects in a lib, and also to decrease the amount of magic.
OBJECT_ENTRY_AUTO(CLSID_BrowserHelperObject, BrowserHelperObject)
OBJECT_ENTRY_AUTO(CLSID_ToolBand, ToolBand)
OBJECT_ENTRY_AUTO(CLSID_CeeeExecutorCreator, CeeeExecutorCreator)
OBJECT_ENTRY_AUTO(CLSID_CeeeExecutor, CeeeExecutor)

class ToolbandModule : public CAtlDllModuleT<ToolbandModule> {
 public:
  ToolbandModule();
  ~ToolbandModule();

  DECLARE_LIBID(LIBID_ToolbandLib)

  // Needed to make sure we call Init/Term outside the loader lock.
  HRESULT DllCanUnloadNow();
  HRESULT DllGetClassObject(REFCLSID clsid, REFIID iid, void** object);
  void Init();
  void Term();
  bool module_initialized() const {
    return module_initialized_;
  }

  // Fires an event to the broker, so that the call can be made with an
  // instance of a broker proxy that was CoCreated in the worker thread.
  void FireEventToBroker(const std::string& event_name,
                         const std::string& event_args);

 private:
  class ComWorkerThread : public base::Thread {
   public:
    ComWorkerThread();

    // Called just prior to starting the message loop
    virtual void Init();

    // Called just after the message loop ends
    virtual void CleanUp();

    // Called by FireEventTask so that the broker we instantiate in the
    // worker thread can be used.
    void FireEventToBroker(BSTR event_name, BSTR event_args);
   protected:
    CComPtr<ICeeeBroker> broker_;
    static const int kMaxNumberOfRetries = 5;
    static const int64 kRetryDelayMs = 10;
    int current_number_of_retries_;
  };

  class FireEventTask : public Task {
   public:
    FireEventTask(ComWorkerThread* worker_thread,
                  const std::string& event_name,
                  const std::string& event_args)
        : worker_thread_(worker_thread),
          event_name_(event_name.c_str()),
          event_args_(event_args.c_str()) {
    }
    FireEventTask(ComWorkerThread* worker_thread,
                  const BSTR event_name,
                  const BSTR event_args)
        : worker_thread_(worker_thread),
          event_name_(event_name),
          event_args_(event_args) {
    }
    virtual void Run() {
      worker_thread_->FireEventToBroker(event_name_, event_args_);
    }
   private:
    ComWorkerThread* worker_thread_;
    CComBSTR event_name_;
    CComBSTR event_args_;
  };
  // We only start the thread on first use. If we would start it on
  // initialization, when our DLL is loaded into the broker process,
  // it would try to start this thread which tries to CoCreate a Broker
  // and this could cause a complex deadlock...
  void EnsureThreadStarted();

  // We use a pointer so that we can make sure we only destroy the object
  // when the thread is properly stopped. Otherwise, we would get a DCHECK
  // if the thread is killed before we get to Stop it when DllCanUnloadNow
  // returns S_OK, which happens when the application quits with live objects,
  // this causes the destructor to DCHECK.
  ComWorkerThread* worker_thread_;
  base::AtExitManager at_exit_;
  bool module_initialized_;
  bool crash_reporting_initialized_;

  int worker_thread_ref_count_;

  friend void ceee_module_util::AddRefModuleWorkerThread();
  friend void ceee_module_util::ReleaseModuleWorkerThread();

  void IncThreadRefCount();
  void DecThreadRefCount();
};

ToolbandModule::ToolbandModule()
    : crash_reporting_initialized_(false),
      module_initialized_(false),
      worker_thread_(NULL) {
  wchar_t logfile_path[MAX_PATH];
  DWORD len = ::GetTempPath(arraysize(logfile_path), logfile_path);
  ::PathAppend(logfile_path, kLogFileName);

  // It seems we're obliged to initialize the current command line
  // before initializing logging. This feels a little strange for
  // a plugin.
  CommandLine::Init(0, NULL);

  logging::InitLogging(
      logfile_path,
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE);

  // Initialize ETW logging.
  logging::LogEventProvider::Initialize(kCeeeIeLogProviderName);

  // Initialize control hosting.
  BOOL initialized = AtlAxWinInit();
  DCHECK(initialized);

  // Needs to be called before we can use GURL.
  chrome::RegisterChromeSchemes();

  ScriptHost::set_default_debug_application(&debug_application);
}

ToolbandModule::~ToolbandModule() {
  ScriptHost::set_default_debug_application(NULL);

  // Just leave thread as is. Releasing interface from this thread may hang IE.
  DCHECK(worker_thread_ref_count_ == 0);
  DCHECK(worker_thread_ == NULL);

  // Uninitialize control hosting.
  BOOL uninitialized = AtlAxWinTerm();
  DCHECK(uninitialized);

  logging::CloseLogFile();
}

HRESULT ToolbandModule::DllCanUnloadNow() {
  HRESULT hr = CAtlDllModuleT<ToolbandModule>::DllCanUnloadNow();
  if (hr == S_OK) {
    // We must protect our data member against concurrent calls to check if we
    // can be unloaded. We must also making the call to Term within the lock
    // to make sure we don't try to re-initialize in case a new
    // DllGetClassObject would occur in the mean time, in another thread.
    m_csStaticDataInitAndTypeInfo.Lock();
    if (module_initialized_) {
      Term();
    }
    m_csStaticDataInitAndTypeInfo.Unlock();
  }
  return hr;
}

HRESULT ToolbandModule::DllGetClassObject(REFCLSID clsid, REFIID iid,
                                         void** object)  {
  // Same comment as above in ToolbandModule::DllCanUnloadNow().
  m_csStaticDataInitAndTypeInfo.Lock();
  if (!module_initialized_) {
    Init();
  }
  m_csStaticDataInitAndTypeInfo.Unlock();
  return CAtlDllModuleT<ToolbandModule>::DllGetClassObject(clsid, iid, object);
}

void ToolbandModule::Init() {
  crash_reporting_initialized_ = InitializeCrashReporting();
  module_initialized_ = true;
}

void ToolbandModule::Term() {
  if (worker_thread_ != NULL) {
    // It is OK to call Stop on a thread even when it isn't running.
    worker_thread_->Stop();
    delete worker_thread_;
    worker_thread_ = NULL;
  }
  if (crash_reporting_initialized_) {
    bool crash_reporting_deinitialized = ShutdownCrashReporting();
    DCHECK(crash_reporting_deinitialized);
    crash_reporting_initialized_ = false;
  }
  module_initialized_ = false;
}

void ToolbandModule::IncThreadRefCount() {
  m_csStaticDataInitAndTypeInfo.Lock();
  DCHECK_GE(worker_thread_ref_count_, 0);
  worker_thread_ref_count_++;
  m_csStaticDataInitAndTypeInfo.Unlock();
}

void ToolbandModule::DecThreadRefCount() {
  ComWorkerThread* thread = NULL;

  m_csStaticDataInitAndTypeInfo.Lock();
  // If we're already at 0, we have a problem, so we check if we're >=.
  DCHECK_GT(worker_thread_ref_count_, 0);

  // If this was our last reference, we delete the thread. This is okay even if
  // we increment the count again, because the thread is created on the "first"
  // FireEventToBroker, thus it will be created again if needed.
  if (--worker_thread_ref_count_ == 0) {
    if (worker_thread_ != NULL) {
      // Store the worker_thread to a temporary pointer. It will be freed later.
      thread = worker_thread_;
      worker_thread_ = NULL;
    }
  }
  m_csStaticDataInitAndTypeInfo.Unlock();

  // Clean the thread after the unlock to be certain we don't get a deadlock
  // (the CriticalSection could be used in the worker thread).
  if (thread) {
    // It is OK to call Stop on a thread even when it isn't running.
    thread->Stop();
    delete thread;
  }
}

void ToolbandModule::EnsureThreadStarted() {
  m_csStaticDataInitAndTypeInfo.Lock();
  if (worker_thread_ == NULL) {
    worker_thread_ = new ComWorkerThread;
    // The COM worker thread must be a UI thread so that it can pump windows
    // messages and allow COM to handle cross apartment calls.
    worker_thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_UI,
                                                           0));  // stack_size
  }
  m_csStaticDataInitAndTypeInfo.Unlock();
}

void ToolbandModule::FireEventToBroker(const std::string& event_name,
                                      const std::string& event_args) {
  EnsureThreadStarted();
  DCHECK(worker_thread_ != NULL);
  MessageLoop* message_loop = worker_thread_->message_loop();
  if (message_loop) {
    message_loop->PostTask(FROM_HERE,
        new FireEventTask(worker_thread_, event_name, event_args));
  } else {
    LOG(ERROR) << "Trying to post a message before the COM worker thread is"
                  "completely initialized and ready.";
  }
}


ToolbandModule::ComWorkerThread::ComWorkerThread()
    : base::Thread("CEEE-COM Worker Thread"),
      current_number_of_retries_(0) {
}

void ToolbandModule::ComWorkerThread::Init() {
  ::CoInitializeEx(0, COINIT_MULTITHREADED);
  HRESULT hr = broker_.CoCreateInstance(CLSID_CeeeBroker);
  DCHECK(SUCCEEDED(hr)) << "Failed to create broker. " << com::LogHr(hr);
}

void ToolbandModule::ComWorkerThread::CleanUp() {
  broker_.Release();
  ::CoUninitialize();
}

void ToolbandModule::ComWorkerThread::FireEventToBroker(BSTR event_name,
                                                       BSTR event_args) {
  DCHECK(broker_ != NULL);
  if (broker_ != NULL) {
    HRESULT hr = broker_->FireEvent(event_name, event_args);
    if (SUCCEEDED(hr)) {
      current_number_of_retries_ = 0;
      return;
    }
    // If the server is busy (which can happen if it is calling in as we try to
    // to call out to it), then we should retry a few times a little later.
    if (current_number_of_retries_ < kMaxNumberOfRetries && message_loop()) {
      ++current_number_of_retries_;
      LOG(WARNING) << "Retrying Broker FireEvent Failure. " << com::LogHr(hr);
      message_loop()->PostDelayedTask(FROM_HERE,
          new FireEventTask(this, event_name, event_args), kRetryDelayMs);
    } else {
      current_number_of_retries_ = 0;
      DCHECK(SUCCEEDED(hr)) << "Broker FireEvent Failed. " << com::LogHr(hr);
    }
  }
}

ToolbandModule module;

void ceee_module_util::AddRefModuleWorkerThread() {
  module.IncThreadRefCount();
}
void ceee_module_util::ReleaseModuleWorkerThread() {
  module.DecThreadRefCount();
}

void ceee_module_util::FireEventToBroker(const std::string& event_name,
                                             const std::string& event_args) {
  module.FireEventToBroker(event_name, event_args);
}

void ceee_module_util::Lock() {
  module.m_csStaticDataInitAndTypeInfo.Lock();
}

void ceee_module_util::Unlock() {
  module.m_csStaticDataInitAndTypeInfo.Unlock();
}

LONG ceee_module_util::LockModule() {
  return module.Lock();
}

LONG ceee_module_util::UnlockModule() {
  return module.Unlock();
}


// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  // Prevent us from being loaded by older versions of the shell.
  if (reason == DLL_PROCESS_ATTACH) {
    wchar_t main_exe[MAX_PATH] = { 0 };
    ::GetModuleFileName(NULL, main_exe, arraysize(main_exe));

    // We don't want to be loaded in the explorer process.
    _wcslwr_s(main_exe, arraysize(main_exe));
    if (wcsstr(main_exe, windows::kExplorerModuleName))
      return FALSE;
  }

  return module.DllMain(reason, reserved);
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void) {
  return module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  return module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry
//
// This is not the actual entrypoint; see the define right below this
// function, which keeps us safe from ever forgetting to check for
// the --enable-ceee flag.
STDAPI DllRegisterServerImpl(void) {
  // registers object, typelib and all interfaces in typelib
  HRESULT hr = module.DllRegisterServer();
  return hr;
}

CEEE_DEFINE_DLL_REGISTER_SERVER()

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void) {
  // We always allow unregistration, even if no --enable-ceee install flag.
  HRESULT hr = module.DllUnregisterServer();
  return hr;
}
