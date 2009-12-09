// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_process_launcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"

#if defined(OS_WIN)
#include "chrome/browser/sandbox_policy.h"
#elif defined(OS_LINUX)
#include "base/singleton.h"
#include "chrome/browser/crash_handler_host_linux.h"
#include "chrome/browser/zygote_host_linux.h"
#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif

#if defined(OS_MACOSX)
#include "ipc/ipc_switches.h"
#include "chrome/browser/mach_broker_mac.h"
#include "chrome/common/mach_ipc_mac.h"
#endif

#if defined(OS_MACOSX)
class MachTask : public Task {
 public:
  MachTask(std::string channel_name, mach_port_t* task, mach_port_t* host)
      : task_(task), host_(host) {
    // TODO(thakis): Move some place central
    const std::string kMachChannelPrefix = "com.Google.Chrome";
    std::string channel = kMachChannelPrefix + channel_name;

    // This creates our named server port -- needs to happen on the current
    // thread.
printf("Creating receive port %s\n", channel.c_str());
    port_.reset(new ReceivePort(channel.c_str()));
  }

  virtual void Run() {
    // TODO(thakis): Move some place central
    const int kMachPortMessageID = 57;

    const int kMachPortMessageReceiveWaitMs = 1000;



    //ReceivePort receivePort(channel_name.c_str());

    // TODO(thakis): time histogram between creation and port reception?
    MachReceiveMessage message;
    kern_return_t result = port_->WaitForMessage(
        &message, kMachPortMessageReceiveWaitMs);
    if (result == KERN_SUCCESS) {
      CHECK(kMachPortMessageID == message.GetMessageID());
      CHECK(2 == message.GetDescriptorCount());

      // TODO(thakis): Constants for the indices?
      *task_ = message.GetTranslatedPort(0);
      *host_ = message.GetTranslatedPort(1);
      printf("yay\n");
    } else {
      // TODO(thakis): Log somewhere?
      printf("nay\n");
    }
  }

 private:
  scoped_ptr<ReceivePort> port_;
  mach_port_t* task_;
  mach_port_t* host_;
};

class MachTask2 : public Task {
 public:
  MachTask2(mach_port_t task, mach_port_t host, base::ProcessHandle pid)
      : task_(task), host_(host), pid_(pid) {}

  virtual void Run() {
    MachBroker::instance()->RegisterPid(
        pid_,
        MachBroker::MachInfo().SetTask(task_).SetHost(host_));
  }
 private:
  mach_port_t task_;
  mach_port_t host_;
  base::ProcessHandle pid_;
};

class MachThread : public base::Thread {
 public:
  MachThread() : base::Thread("MachThread"), task_(0), host_(0) {}

  void DoIt(const std::string& channel_name) {
    DCHECK(message_loop());
    message_loop()->PostTask(FROM_HERE,
                             new MachTask(channel_name, &task_, &host_));
  }

  void DoIt2(base::ProcessHandle pid) {
    DCHECK(message_loop());
    message_loop()->PostTask(FROM_HERE,
                             new MachTask2(task_, host_, pid));
  }

 private:
  mach_port_t task_;
  mach_port_t host_;
};
#endif

// Having the functionality of ChildProcessLauncher be in an internal
// ref counted object allows us to automatically terminate the process when the
// parent class destructs, while still holding on to state that we need.
class ChildProcessLauncher::Context
    : public base::RefCountedThreadSafe<ChildProcessLauncher::Context> {
 public:
  Context()
      : starting_(true)
#if defined(OS_LINUX)
        , zygote_(false)
#endif
        {
  }

  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      const base::environment_vector& environ,
      int ipcfd,
#endif
      CommandLine* cmd_line,
      Client* client) {
    client_ = client;

    CHECK(ChromeThread::GetCurrentThreadIdentifier(&client_thread_id_));

    ChromeThread::PostTask(
        ChromeThread::PROCESS_LAUNCHER, FROM_HERE,
        NewRunnableMethod(
            this,
            &Context::LaunchInternal,
#if defined(OS_WIN)
            exposed_dir,
#elif defined(POSIX)
            environ,
            ipcfd,
#endif
            cmd_line));
  }

  void ResetClient() {
    // No need for locking as this function gets called on the same thread that
    // client_ would be used.
    CHECK(ChromeThread::CurrentlyOn(client_thread_id_));
    client_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncher::Context>;
  friend class ChildProcessLauncher;

  ~Context() {
    Terminate();
  }

  void LaunchInternal(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      const base::environment_vector& env,
      int ipcfd,
#endif
      CommandLine* cmd_line) {
    scoped_ptr<CommandLine> cmd_line_deleter(cmd_line);
    base::ProcessHandle handle = base::kNullProcessHandle;
#if defined(OS_WIN)
    handle = sandbox::StartProcessWithAccess(cmd_line, exposed_dir);
#elif defined(OS_POSIX)

#if defined(OS_LINUX)
    bool zygote = false;
    // On Linux, normally spawn renderer processes with zygotes. We can't do
    // this when we're spawning child processes through an external program
    // (i.e. there is a command prefix) like GDB so fall through to the POSIX
    // case then.
    bool is_renderer = cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
        switches::kRendererProcess;
    bool is_extension = cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
        switches::kExtensionProcess;
    bool is_plugin = cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
        switches::kPluginProcess;
    if ((is_renderer || is_extension) &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kRendererCmdPrefix)) {
      zygote = true;

      base::GlobalDescriptors::Mapping mapping;
      mapping.push_back(std::pair<uint32_t, int>(kPrimaryIPCChannel, ipcfd));
      const int crash_signal_fd =
          Singleton<RendererCrashHandlerHostLinux>()->GetDeathSignalSocket();
      if (crash_signal_fd >= 0) {
        mapping.push_back(std::pair<uint32_t, int>(kCrashDumpSignal,
                                                   crash_signal_fd));
      }
      handle = Singleton<ZygoteHost>()->ForkRenderer(cmd_line->argv(), mapping);
    }

    if (!zygote)
#endif
    {
      base::file_handle_mapping_vector fds_to_map;
      fds_to_map.push_back(std::make_pair(
          ipcfd,
          kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));

#if defined(OS_LINUX)
      // On Linux, we need to add some extra file descriptors for crash handling
      // and the sandbox.
      if (is_renderer || is_plugin) {
        int crash_signal_fd;
        if (is_renderer) {
          crash_signal_fd = Singleton<RendererCrashHandlerHostLinux>()->
              GetDeathSignalSocket();
        } else {
          crash_signal_fd = Singleton<PluginCrashHandlerHostLinux>()->
              GetDeathSignalSocket();
        }
        if (crash_signal_fd >= 0) {
          fds_to_map.push_back(std::make_pair(
              crash_signal_fd,
              kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor));
        }
      }
      if (is_renderer) {
        const int sandbox_fd =
            Singleton<RenderSandboxHostLinux>()->GetRendererSocket();
        fds_to_map.push_back(std::make_pair(
            sandbox_fd,
            kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
      }
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
      // TODO(thakis): Possibly somewhere else?
      // (then again, the fds duping stuff is here too, so maybe it's ok)

      MachThread mach_thread;
      CHECK(mach_thread.Start());
      mach_thread.DoIt(
          cmd_line->GetSwitchValueASCII(switches::kProcessChannelID));
#endif


      // Actually launch the app.
      if (!base::LaunchApp(cmd_line->argv(), env, fds_to_map, false, &handle))
        handle = base::kNullProcessHandle;

#if defined(OS_MACOSX)
      // TODO(thakis): Check |handle| first.
      mach_thread.DoIt2(handle);
#endif
    }
#endif

    ChromeThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(
            this,
            &ChildProcessLauncher::Context::Notify,
#if defined(OS_LINUX)
            zygote,
#endif
            handle));
  }

  void Notify(
#if defined(OS_LINUX)
      bool zygote,
#endif
      base::ProcessHandle handle) {
    starting_ = false;
    process_.set_handle(handle);
#if defined(OS_LINUX)
    zygote_ = zygote;
#endif
    if (client_) {
      client_->OnProcessLaunched();
    } else {
      Terminate();
    }
  }

  void Terminate() {
    if (!process_.handle())
      return;

    // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
    // don't this on the UI/IO threads.
    ChromeThread::PostTask(
        ChromeThread::PROCESS_LAUNCHER, FROM_HERE,
        NewRunnableFunction(
            &ChildProcessLauncher::Context::TerminateInternal,
#if defined(OS_LINUX)
            zygote_,
#endif
            process_.handle()));
    process_.set_handle(base::kNullProcessHandle);
  }

  static void TerminateInternal(
#if defined(OS_LINUX)
      bool zygote,
#endif
      base::ProcessHandle handle) {
    base::Process process(handle);
     // Client has gone away, so just kill the process.  Using exit code 0
    // means that UMA won't treat this as a crash.
    process.Terminate(ResultCodes::NORMAL_EXIT);
    // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
#if defined(OS_LINUX)
    if (zygote) {
      // If the renderer was created via a zygote, we have to proxy the reaping
      // through the zygote process.
      Singleton<ZygoteHost>()->EnsureProcessTerminated(handle);
    } else
#endif  // defined(OS_LINUX)
    {
      ProcessWatcher::EnsureProcessTerminated(handle);
    }
#endif
    process.Close();
  }

  Client* client_;
  ChromeThread::ID client_thread_id_;
  base::Process process_;
  bool starting_;

#if defined(OS_LINUX)
  bool zygote_;
#endif
};


ChildProcessLauncher::ChildProcessLauncher(
#if defined(OS_WIN)
    const FilePath& exposed_dir,
#elif defined(OS_POSIX)
    const base::environment_vector& environ,
    int ipcfd,
#endif
    CommandLine* cmd_line,
    Client* client) {
  context_ = new Context();
  context_->Launch(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      environ,
      ipcfd,
#endif
      cmd_line,
      client);
}

ChildProcessLauncher::~ChildProcessLauncher() {
  context_->ResetClient();
}

bool ChildProcessLauncher::IsStarting() {
  return context_->starting_;
}

base::ProcessHandle ChildProcessLauncher::GetHandle() {
  DCHECK(!context_->starting_);
  return context_->process_.handle();
}

bool ChildProcessLauncher::DidProcessCrash() {
  bool did_crash, child_exited;
  base::ProcessHandle handle = context_->process_.handle();
#if defined(OS_LINUX)
  if (context_->zygote_) {
    did_crash = Singleton<ZygoteHost>()->DidProcessCrash(handle, &child_exited);
  } else
#endif
  {
    did_crash = base::DidProcessCrash(&child_exited, handle);
  }

  // POSIX: If the process crashed, then the kernel closed the socket for it
  // and so the child has already died by the time we get here. Since
  // DidProcessCrash called waitpid with WNOHANG, it'll reap the process.
  // However, if DidProcessCrash didn't reap the child, we'll need to in
  // Terminate via ProcessWatcher. So we can't close the handle here.
  //
  // This is moot on Windows where |child_exited| will always be true.
  if (child_exited)
    context_->process_.Close();

  return did_crash;
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  DCHECK(!context_->starting_);
  context_->process_.SetProcessBackgrounded(background);
}
