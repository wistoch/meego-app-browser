// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/global_descriptors_posix.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/sys_info.h"
#include "base/unix_domain_socket_posix.h"

#include "chrome/browser/zygote_host_linux.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/sandbox_methods_linux.h"

#include "media/base/media.h"

#include "skia/ext/SkFontHost_fontconfig_control.h"

#if defined(CHROMIUM_SELINUX)
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#include "unicode/timezone.h"

// http://code.google.com/p/chromium/wiki/LinuxZygote

static const int kMagicSandboxIPCDescriptor = 5;

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, at the the bottom and simple constructs one
// of these objects and runs it.
class Zygote {
 public:
  bool ProcessRequests() {
    // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
    // browser on it.
    // A SOCK_DGRAM is installed in fd 4. This is the sandbox IPC channel.
    // See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

    // We need to accept SIGCHLD, even though our handler is a no-op because
    // otherwise we cannot wait on children. (According to POSIX 2001.)
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGCHLDHandler;
    CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

    for (;;) {
      if (HandleRequestFromBrowser(3))
        return true;
    }
  }

 private:
  // See comment below, where sigaction is called.
  static void SIGCHLDHandler(int signal) { }

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd) {
    std::vector<int> fds;
    static const unsigned kMaxMessageLength = 1024;
    char buf[kMaxMessageLength];
    const ssize_t len = base::RecvMsg(fd, buf, sizeof(buf), &fds);
    if (len == -1) {
      LOG(WARNING) << "Error reading message from browser: " << errno;
      return false;
    }

    if (len == 0) {
      // EOF from the browser. We should die.
      _exit(0);
      return false;
    }

    Pickle pickle(buf, len);
    void* iter = NULL;

    int kind;
    if (pickle.ReadInt(&iter, &kind)) {
      switch (kind) {
        case ZygoteHost::kCmdFork:
          return HandleForkRequest(fd, pickle, iter, fds);
        case ZygoteHost::kCmdReap:
          if (!fds.empty())
            break;
          return HandleReapRequest(fd, pickle, iter);
        case ZygoteHost::kCmdDidProcessCrash:
          if (!fds.empty())
            break;
          return HandleDidProcessCrash(fd, pickle, iter);
        default:
          NOTREACHED();
          break;
      }
    }

    LOG(WARNING) << "Error parsing message from browser";
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    return false;
  }

  bool HandleReapRequest(int fd, Pickle& pickle, void* iter) {
    pid_t child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing reap request from browser";
      return false;
    }

    ProcessWatcher::EnsureProcessTerminated(child);

    return false;
  }

  bool HandleDidProcessCrash(int fd, Pickle& pickle, void* iter) {
    base::ProcessHandle child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing DidProcessCrash request from browser";
      return false;
    }

    bool child_exited;
    bool did_crash = base::DidProcessCrash(&child_exited, child);

    Pickle write_pickle;
    write_pickle.WriteBool(did_crash);
    write_pickle.WriteBool(child_exited);
    HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));

    return false;
  }

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer.
  bool HandleForkRequest(int fd, Pickle& pickle, void* iter,
                         std::vector<int>& fds) {
    std::vector<std::string> args;
    int argc, numfds;
    base::GlobalDescriptors::Mapping mapping;
    pid_t child;

    if (!pickle.ReadInt(&iter, &argc))
      goto error;

    for (int i = 0; i < argc; ++i) {
      std::string arg;
      if (!pickle.ReadString(&iter, &arg))
        goto error;
      args.push_back(arg);
    }

    if (!pickle.ReadInt(&iter, &numfds))
      goto error;
    if (numfds != static_cast<int>(fds.size()))
      goto error;

    for (int i = 0; i < numfds; ++i) {
      base::GlobalDescriptors::Key key;
      if (!pickle.ReadUInt32(&iter, &key))
        goto error;
      mapping.push_back(std::make_pair(key, fds[i]));
    }

    mapping.push_back(std::make_pair(
        static_cast<uint32_t>(kSandboxIPCChannel), 5));

    child = fork();

    if (!child) {
      close(3);  // our socket from the browser is in fd 3
      Singleton<base::GlobalDescriptors>()->Reset(mapping);

      // Reset the process-wide command line to our new command line.
      CommandLine::Reset();
      CommandLine::Init(0, NULL);
      CommandLine::ForCurrentProcess()->InitFromArgv(args);
      CommandLine::SetProcTitle();
      return true;
    }

    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);

    HANDLE_EINTR(write(fd, &child, sizeof(child)));
    return false;

   error:
    LOG(WARNING) << "Error parsing fork request from browser";
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    return false;
  }
};

// With SELinux we can carve out a precise sandbox, so we don't have to play
// with intercepting libc calls.
#if !defined(CHROMIUM_SELINUX)

static void ProxyLocaltimeCallToBrowser(time_t input, struct tm* output,
                                        char* timezone_out,
                                        size_t timezone_out_len) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  uint8_t reply_buf[512];
  const ssize_t r = base::SendRecvMsg(
      kMagicSandboxIPCDescriptor, reply_buf, sizeof(reply_buf), NULL, request);
  if (r == -1) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  void* iter = NULL;
  std::string result, timezone;
  if (!reply.ReadString(&iter, &result) ||
      !reply.ReadString(&iter, &timezone) ||
      result.size() != sizeof(struct tm)) {
    memset(output, 0, sizeof(struct tm));
    return;
  }

  memcpy(output, result.data(), sizeof(struct tm));
  if (timezone_out_len) {
    const size_t copy_len = std::min(timezone_out_len - 1, timezone.size());
    memcpy(timezone_out, timezone.data(), copy_len);
    timezone_out[copy_len] = 0;
    output->tm_zone = timezone_out;
  } else {
    output->tm_zone = NULL;
  }
}

static bool g_am_zygote_or_renderer = false;

// Sandbox interception of libc calls.
//
// Because we are running in a sandbox certain libc calls will fail (localtime
// being the motivating example - it needs to read /etc/localtime). We need to
// intercept these calls and proxy them to the browser. However, these calls
// may come from us or from our libraries. In some cases we can't just change
// our code.
//
// It's for these cases that we have the following setup:
//
// We define global functions for those functions which we wish to override.
// Since we will be first in the dynamic resolution order, the dynamic linker
// will point callers to our versions of these functions. However, we have the
// same binary for both the browser and the renderers, which means that our
// overrides will apply in the browser too.
//
// The global |g_am_zygote_or_renderer| is true iff we are in a zygote or
// renderer process. It's set in ZygoteMain and inherited by the renderers when
// they fork. (This means that it'll be incorrect for global constructor
// functions and before ZygoteMain is called - beware).
//
// Our replacement functions can check this global and either proxy
// the call to the browser over the sandbox IPC
// (http://code.google.com/p/chromium/wiki/LinuxSandboxIPC) or they can use
// dlsym with RTLD_NEXT to resolve the symbol, ignoring any symbols in the
// current module.
//
// Other avenues:
//
// Our first attempt involved some assembly to patch the GOT of the current
// module. This worked, but was platform specific and doesn't catch the case
// where a library makes a call rather than current module.
//
// We also considered patching the function in place, but this would again by
// platform specific and the above technique seems to work well enough.

static void WarnOnceAboutBrokenDlsym();

struct tm* localtime(const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  } else {
    typedef struct tm* (*LocaltimeFunction)(const time_t* timep);
    static LocaltimeFunction libc_localtime;
    static bool have_libc_localtime = false;
    if (!have_libc_localtime) {
      libc_localtime = (LocaltimeFunction) dlsym(RTLD_NEXT, "localtime");
      have_libc_localtime = true;
    }

    if (!libc_localtime) {
      // http://code.google.com/p/chromium/issues/detail?id=16800
      //
      // Nvidia's libGL.so overrides dlsym for an unknown reason and replaces
      // it with a version which doesn't work. In this case we'll get a NULL
      // result. There's not a lot we can do at this point, so we just bodge it!
      WarnOnceAboutBrokenDlsym();

      return gmtime(timep);
    }

    return libc_localtime(timep);
  }
}

struct tm* localtime_r(const time_t* timep, struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, NULL, 0);
    return result;
  } else {
    typedef struct tm* (*LocaltimeRFunction)(const time_t* timep,
                                             struct tm* result);
    static LocaltimeRFunction libc_localtime_r;
    static bool have_libc_localtime_r = false;
    if (!have_libc_localtime_r) {
      libc_localtime_r = (LocaltimeRFunction) dlsym(RTLD_NEXT, "localtime_r");
      have_libc_localtime_r = true;
    }

    if (!libc_localtime_r) {
      // See |localtime|, above.
      WarnOnceAboutBrokenDlsym();

      return gmtime_r(timep, result);
    }

    return libc_localtime_r(timep, result);
  }
}

// See the comments at the callsite in |localtime| about this function.
static void WarnOnceAboutBrokenDlsym() {
  static bool have_shown_warning = false;
  if (!have_shown_warning) {
    LOG(ERROR) << "Your system is broken: dlsym doesn't work! This has been "
                  "reported to be caused by Nvidia's libGL. You should expect "
                  "time related functions to misbehave. "
                  "http://code.google.com/p/chromium/issues/detail?id=16800";
    have_shown_warning = true;
  }
}
#endif  // !CHROMIUM_SELINUX

// This function triggers the static and lazy construction of objects that need
// to be created before imposing the sandbox.
static void PreSandboxInit() {
    base::RandUint64();

    base::SysInfo::MaxSharedMemorySize();

    // To make wcstombs/mbstowcs work in a renderer, setlocale() has to be
    // called before the sandbox is triggered. It's possible to avoid calling
    // setlocale() by pulling out the conversion between FilePath and
    // WebCore String out of the renderer and using string16 in place of
    // FilePath for IPC.
    const char* locale = setlocale(LC_ALL, "");
    LOG_IF(WARNING, locale == NULL) << "setlocale failed.";

    // ICU DateFormat class (used in base/time_format.cc) needs to get the
    // Olson timezone ID by accessing the zoneinfo files on disk. After
    // TimeZone::createDefault is called once here, the timezone ID is
    // cached and there's no more need to access the file system.
    scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

    FilePath module_path;
    if (PathService::Get(base::DIR_MODULE, &module_path))
      media::InitializeMediaLibrary(module_path);
}

#if !defined(CHROMIUM_SELINUX)
static bool EnterSandbox() {
  const char* const sandbox_fd_string = getenv("SBX_D");
  if (sandbox_fd_string) {
    // The SUID sandbox sets this environment variable to a file descriptor
    // over which we can signal that we have completed our startup and can be
    // chrooted.

    char* endptr;
    const long fd_long = strtol(sandbox_fd_string, &endptr, 10);
    if (!*sandbox_fd_string || *endptr || fd_long < 0 || fd_long > INT_MAX)
      return false;
    const int fd = fd_long;

    PreSandboxInit();

    static const char kChrootMe = 'C';
    static const char kChrootMeSuccess = 'O';

    if (HANDLE_EINTR(write(fd, &kChrootMe, 1)) != 1) {
      LOG(ERROR) << "Failed to write to chroot pipe: " << errno;
      return false;
    }

    // We need to reap the chroot helper process in any event:
    wait(NULL);

    char reply;
    if (HANDLE_EINTR(read(fd, &reply, 1)) != 1) {
      LOG(ERROR) << "Failed to read from chroot pipe: " << errno;
      return false;
    }

    if (reply != kChrootMeSuccess) {
      LOG(ERROR) << "Error code reply from chroot helper";
      return false;
    }

    SkiaFontConfigUseIPCImplementation(kMagicSandboxIPCDescriptor);

    // Previously, we required that the binary be non-readable. This causes the
    // kernel to mark the process as non-dumpable at startup. The thinking was
    // that, although we were putting the renderers into a PID namespace (with
    // the SUID sandbox), they would nonetheless be in the /same/ PID
    // namespace. So they could ptrace each other unless they were non-dumpable.
    //
    // If the binary was readable, then there would be a window between process
    // startup and the point where we set the non-dumpable flag in which a
    // compromised renderer could ptrace attach.
    //
    // However, now that we have a zygote model, only the (trusted) zygote
    // exists at this point and we can set the non-dumpable flag which is
    // inherited by all our renderer children.
    //
    // Note: a non-dumpable process can't be debugged. To debug sandbox-related
    // issues, one can specify --allow-sandbox-debugging to let the process be
    // dumpable.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kAllowSandboxDebugging)) {
      prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
      if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
        LOG(ERROR) << "Failed to set non-dumpable flag";
        return false;
      }
    }
  } else {
    SkiaFontConfigUseDirectImplementation();
  }

  return true;
}
#else  // CHROMIUM_SELINUX

static bool EnterSandbox() {
  PreSandboxInit();
  SkiaFontConfigUseIPCImplementation(kMagicSandboxIPCDescriptor);

  security_context_t security_context;
  if (getcon(&security_context)) {
    LOG(ERROR) << "Cannot get SELinux context";
    return false;
  }

  context_t context = context_new(security_context);
  context_type_set(context, "chromium_renderer_t");
  const int r = setcon(context_str(context));
  context_free(context);
  freecon(security_context);

  if (r) {
    LOG(ERROR) << "dynamic transition to type 'chromium_renderer_t' failed. "
                  "(this binary has been built with SELinux support, but maybe "
                  "the policies haven't been loaded into the kernel?";
    return false;
  }

  return true;
}

#endif  // CHROMIUM_SELINUX

bool ZygoteMain(const MainFunctionParams& params) {
#if !defined(CHROMIUM_SELINUX)
  g_am_zygote_or_renderer = true;
#endif

  if (!EnterSandbox()) {
    LOG(FATAL) << "Failed to enter sandbox. Fail safe abort. (errno: "
               << errno << ")";
    return false;
  }

  Zygote zygote;
  return zygote.ProcessRequests();
}
