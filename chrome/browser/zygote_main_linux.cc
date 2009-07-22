// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/global_descriptors_posix.h"
#include "base/pickle.h"
#include "base/rand_util.h"
#include "base/unix_domain_socket_posix.h"

#include "chrome/browser/zygote_host_linux.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/sandbox_methods_linux.h"

#include "skia/ext/SkFontHost_fontconfig_control.h"

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
      CommandLine::Reset();
      CommandLine::Init(args);
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

// Patched dynamic symbol wrapper functions...
namespace sandbox_wrapper {

void do_localtime(time_t input, struct tm* output, char* timezone_out,
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

struct tm* localtime(const time_t* timep) {
  static struct tm time_struct;
  static char timezone_string[64];
  do_localtime(*timep, &time_struct, timezone_string, sizeof(timezone_string));
  return &time_struct;
}

struct tm* localtime_r(const time_t* timep, struct tm* result) {
  do_localtime(*timep, result, NULL, 0);
  return result;
}

}  // namespace sandbox_wrapper

/* On IA-32, function calls which need to be resolved by the dynamic linker are
 * directed to the producure linking table (PLT). Each PLT entry contains code
 * which jumps (indirectly) via the global offset table (GOT):
 *   Dump of assembler code for function f@plt:
 *   0x0804830c <f@plt+0>:   jmp    *0x804a004  # GOT indirect jump
 *   0x08048312 <f@plt+6>:   push   $0x8
 *   0x08048317 <f@plt+11>:  jmp    0x80482ec <_init+48>
 *
 * At the beginning of a process's lifetime, the GOT entry jumps back to
 * <f@plt+6> end then enters the dynamic linker. Once the symbol has been
 * resolved, the GOT entry is patched so that future calls go directly to the
 * resolved function.
 *
 * This macro finds the PLT entry for a given symbol, |symbol|, and reads the
 * GOT entry address from the first instruction. It then patches that address
 * with the address of a replacement function, |replacement|.
 */
#define PATCH_GLOBAL_OFFSET_TABLE(symbol, replacement) \
       /* First, get the current instruction pointer since the PLT address */ \
       /* is IP relative */ \
  asm ("call 0f\n" \
       "0: pop %%ecx\n" \
       /* Move the IP relative address of the PLT entry into EAX */ \
       "mov $" #symbol "@plt,%%eax\n" \
       /* Add EAX to ECX to get an absolute entry */ \
       "add %%eax,%%ecx\n" \
       /* The value in ECX was relative to the add instruction, however, */ \
       /* the IP value was that of the pop. The pop and mov take 6 */ \
       /* bytes, so adding 6 gets us the correct address for the PLT. The */ \
       /* first instruction at the PLT is FF 25 <abs address>, so we skip 2 */ \
       /* bytes to get to the address. 6 + 2 = 8: */ \
       "movl 8(%%ecx),%%ecx\n" \
       /* Now ECX contains the address of the GOT entry, we poke our */ \
       /* replacement function in there: */ \
       "movl %0,(%%ecx)\n" \
       :  /* no output */ \
       :  "r" (replacement) \
       : "memory", "%eax", "%ecx");

static bool MaybeEnterChroot() {
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

    // Before entering the sandbox, "prime" any systems that need to open
    // files and cache the results or the descriptors.
    base::RandUint64();

    PATCH_GLOBAL_OFFSET_TABLE(localtime, sandbox_wrapper::localtime);
    PATCH_GLOBAL_OFFSET_TABLE(localtime_r, sandbox_wrapper::localtime_r);

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

bool ZygoteMain(const MainFunctionParams& params) {
  if (!MaybeEnterChroot()) {
    LOG(FATAL) << "Failed to enter sandbox. Fail safe abort. (errno: "
               << errno << ")";
    return false;
  }

  Zygote zygote;
  return zygote.ProcessRequests();
}
