#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# valgrind_test.py

"""Runs an exe through Valgrind and puts the intermediate files in a
directory.
"""

import datetime
import glob
import logging
import optparse
import os
import re
import shutil
import stat
import sys
import tempfile
import time

import common

import drmemory_analyze
import memcheck_analyze
import tsan_analyze

import logging_utils

class BaseTool(object):
  """Abstract class for running Valgrind-, PIN-based and other dynamic
  error detector tools.

  Always subclass this and implement ToolCommand with framework- and
  tool-specific stuff.
  """

  # Temporary directory for tools to write logs, create some useful files etc.
  TMP_DIR = "testing.tmp"

  def __init__(self):
    # If we have a testing.tmp directory, we didn't cleanup last time.
    if os.path.exists(self.TMP_DIR):
      shutil.rmtree(self.TMP_DIR)
    os.mkdir(self.TMP_DIR)

    self.option_parser_hooks = []

  def ToolName(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def Analyze(self, check_sanity=False):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def RegisterOptionParserHook(self, hook):
    # Frameworks and tools can add their own flags to the parser.
    self.option_parser_hooks.append(hook)

  def CreateOptionParser(self):
    # Defines Chromium-specific flags.
    self._parser = optparse.OptionParser("usage: %prog [options] <program to "
                                         "test>")
    self._parser.add_option("-t", "--timeout",
                      dest="timeout", metavar="TIMEOUT", default=10000,
                      help="timeout in seconds for the run (default 10000)")
    self._parser.add_option("", "--source_dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")
    self._parser.add_option("", "--gtest_filter", default="",
                            help="which test case to run")
    self._parser.add_option("", "--gtest_repeat",
                            help="how many times to run each test")
    self._parser.add_option("", "--gtest_print_time", action="store_true",
                            default=False,
                            help="show how long each test takes")
    self._parser.add_option("", "--ignore_exit_code", action="store_true",
                            default=False,
                            help="ignore exit code of the test "
                                 "(e.g. test failures)")
    self._parser.add_option("", "--nocleanup_on_exit", action="store_true",
                            default=False,
                            help="don't delete directory with logs on exit")

    # To add framework- or tool-specific flags, please add a hook using
    # RegisterOptionParserHook in the corresponding subclass.
    # See ValgrindTool and ThreadSanitizerBase for examples.
    for hook in self.option_parser_hooks:
      hook(self, self._parser)

  def ParseArgv(self, args):
    self.CreateOptionParser()

    # self._tool_flags will store those tool flags which we don't parse
    # manually in this script.
    self._tool_flags = []
    known_args = []

    """ We assume that the first argument not starting with "-" is a program
    name and all the following flags should be passed to the program.
    TODO(timurrrr): customize optparse instead
    """
    while len(args) > 0 and args[0][:1] == "-":
      arg = args[0]
      if (arg == "--"):
        break
      if self._parser.has_option(arg.split("=")[0]):
        known_args += [arg]
      else:
        self._tool_flags += [arg]
      args = args[1:]

    if len(args) > 0:
      known_args += args

    self._options, self._args = self._parser.parse_args(known_args)

    self._timeout = int(self._options.timeout)
    self._source_dir = self._options.source_dir
    self._nocleanup_on_exit = self._options.nocleanup_on_exit
    self._ignore_exit_code = self._options.ignore_exit_code
    if self._options.gtest_filter != "":
      self._args.append("--gtest_filter=%s" % self._options.gtest_filter)
    if self._options.gtest_repeat:
      self._args.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    if self._options.gtest_print_time:
      self._args.append("--gtest_print_time")

    return True

  def Setup(self, args):
    return self.ParseArgv(args)

  def ToolCommand(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def Cleanup(self):
    # You may override it in the tool-specific subclass
    pass

  def Execute(self):
    """ Execute the app to be tested after successful instrumentation.
    Full execution command-line provided by subclassers via proc."""
    logging.info("starting execution...")

    proc = self.ToolCommand()

    add_env = {
      "G_SLICE" : "always-malloc",
      "NSS_DISABLE_ARENA_FREE_LIST" : "1",
      "GTEST_DEATH_TEST_USE_FORK" : "1",
    }
    if common.IsWine():
      # TODO(timurrrr): Maybe we need it for TSan/Win too?
      add_env["CHROME_ALLOCATOR"] = "winheap"

    for k,v in add_env.iteritems():
      logging.info("export %s=%s", k, v)
      os.putenv(k, v)

    return common.RunSubprocess(proc, self._timeout)

  def RunTestsAndAnalyze(self, check_sanity):
    exec_retcode = self.Execute()
    analyze_retcode = self.Analyze(check_sanity)

    if analyze_retcode:
      logging.error("Analyze failed.")
      logging.info("Search the log for '[ERROR]' to see the error reports.")
      return analyze_retcode

    if exec_retcode:
      if self._ignore_exit_code:
        logging.info("Test execution failed, but the exit code is ignored.")
      else:
        logging.error("Test execution failed.")
        return exec_retcode
    else:
      logging.info("Test execution completed successfully.")

    if not analyze_retcode:
      logging.info("Analysis completed successfully.")

    return 0

  def Main(self, args, check_sanity):
    """Call this to run through the whole process: Setup, Execute, Analyze"""
    start = datetime.datetime.now()
    retcode = -1
    if self.Setup(args):
      retcode = self.RunTestsAndAnalyze(check_sanity)
      if not self._nocleanup_on_exit:
        shutil.rmtree(self.TMP_DIR, ignore_errors=True)
      self.Cleanup()
    else:
      logging.error("Setup failed")
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds = seconds % 3600
    minutes = seconds / 60
    seconds = seconds % 60
    logging.info("elapsed time: %02d:%02d:%02d" % (hours, minutes, seconds))
    return retcode


class ValgrindTool(BaseTool):
  """Abstract class for running Valgrind tools.

  Always subclass this and implement ToolSpecificFlags() and
  ExtendOptionParser() for tool-specific stuff.
  """
  def __init__(self):
    BaseTool.__init__(self)
    self.RegisterOptionParserHook(ValgrindTool.ExtendOptionParser)

  def UseXML(self):
    # Override if tool prefers nonxml output
    return True

  def SelfContained(self):
    # Returns true iff the tool is distibuted as a self-contained
    # .sh script (e.g. ThreadSanitizer)
    return False

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--suppressions", default=[],
                            action="append",
                            help="path to a valgrind suppression file")
    parser.add_option("", "--indirect", action="store_true",
                            default=False,
                            help="set BROWSER_WRAPPER rather than "
                                 "running valgrind directly")
    parser.add_option("", "--trace_children", action="store_true",
                            default=False,
                            help="also trace child processes")
    parser.add_option("", "--num-callers",
                            dest="num_callers", default=30,
                            help="number of callers to show in stack traces")
    parser.add_option("", "--generate_dsym", action="store_true",
                          default=False,
                          help="Generate .dSYM file on Mac if needed. Slow!")

  def Setup(self, args):
    if not BaseTool.Setup(self, args):
      return False
    if common.IsWine():
      self.PrepareForTestWine()
    elif common.IsMac():
      self.PrepareForTestMac()
    return True

  def PrepareForTestMac(self):
    """Runs dsymutil if needed.

    Valgrind for Mac OS X requires that debugging information be in a .dSYM
    bundle generated by dsymutil.  It is not currently able to chase DWARF
    data into .o files like gdb does, so executables without .dSYM bundles or
    with the Chromium-specific "fake_dsym" bundles generated by
    build/mac/strip_save_dsym won't give source file and line number
    information in valgrind.

    This function will run dsymutil if the .dSYM bundle is missing or if
    it looks like a fake_dsym.  A non-fake dsym that already exists is assumed
    to be up-to-date.
    """
    test_command = self._args[0]
    dsym_bundle = self._args[0] + '.dSYM'
    dsym_file = os.path.join(dsym_bundle, 'Contents', 'Resources', 'DWARF',
                             os.path.basename(test_command))
    dsym_info_plist = os.path.join(dsym_bundle, 'Contents', 'Info.plist')

    needs_dsymutil = True
    saved_test_command = None

    if os.path.exists(dsym_file) and os.path.exists(dsym_info_plist):
      # Look for the special fake_dsym tag in dsym_info_plist.
      dsym_info_plist_contents = open(dsym_info_plist).read()

      if not re.search('^\s*<key>fake_dsym</key>$', dsym_info_plist_contents,
                       re.MULTILINE):
        # fake_dsym is not set, this is a real .dSYM bundle produced by
        # dsymutil.  dsymutil does not need to be run again.
        needs_dsymutil = False
      else:
        # fake_dsym is set.  dsym_file is a copy of the original test_command
        # before it was stripped.  Copy it back to test_command so that
        # dsymutil has unstripped input to work with.  Move the stripped
        # test_command out of the way, it will be restored when this is
        # done.
        saved_test_command = test_command + '.stripped'
        os.rename(test_command, saved_test_command)
        shutil.copyfile(dsym_file, test_command)
        shutil.copymode(saved_test_command, test_command)

    if needs_dsymutil:
      if self._options.generate_dsym:
        # Remove the .dSYM bundle if it exists.
        shutil.rmtree(dsym_bundle, True)

        dsymutil_command = ['dsymutil', test_command]

        # dsymutil is crazy slow.  Ideally we'd have a timeout here,
        # but common.RunSubprocess' timeout is only checked
        # after each line of output; dsymutil is silent
        # until the end, and is then killed, which is silly.
        common.RunSubprocess(dsymutil_command)

        if saved_test_command:
          os.rename(saved_test_command, test_command)
      else:
        logging.info("No real .dSYM for test_command.  Line numbers will "
                     "not be shown.  Either tell xcode to generate .dSYM "
                     "file, or use --generate_dsym option to this tool.")

  def PrepareForTestWine(self):
    """Set up the Wine environment.

    We need to run some sanity checks, set up a Wine prefix, and make sure
    wineserver is running by starting a dummy win32 program.
    """
    if not os.path.exists('/usr/share/ca-certificates/root_ca_cert.crt'):
      logging.warning('WARNING: SSL certificate missing! SSL tests will fail.')
      logging.warning('You need to run:')
      logging.warning('sudo cp src/net/data/ssl/certificates/root_ca_cert.crt '
                      '/usr/share/ca-certificates/')
      logging.warning('sudo vi /etc/ca-certificates.conf')
      logging.warning('  (and add the line root_ca_cert.crt)')
      logging.warning('sudo update-ca-certificates')

    # Shutdown the Wine server in case the last run got interrupted.
    common.RunSubprocess([os.environ.get('WINESERVER'), '-k'])

    # Yes, this can be dangerous if $WINEPREFIX is set incorrectly.
    shutil.rmtree(os.environ.get('WINEPREFIX'), ignore_errors=True)

    winetricks = os.path.join(self._source_dir, 'tools', 'valgrind',
                              'wine_memcheck', 'winetricks')
    common.RunSubprocess(['sh', winetricks,
                          'nocrashdialog', 'corefonts', 'gecko'])
    time.sleep(1)

    # Start a dummy program like winemine so Valgrind won't run memcheck on
    # the wineserver startup routine when it launches the test binary, which
    # is slow and not interesting to us.
    common.RunSubprocessInBackground([os.environ.get('WINE'), 'winemine'])
    return

  def ToolCommand(self):
    """Get the valgrind command to run."""
    # Note that self._args begins with the exe to be run.
    tool_name = self.ToolName()

    # Construct the valgrind command.
    if self.SelfContained():
      proc = ["valgrind-%s.sh" % tool_name]
    else:
      proc = ["valgrind", "--tool=%s" % tool_name]

    proc += ["--num-callers=%i" % int(self._options.num_callers)]

    if self._options.trace_children:
      proc += ["--trace-children=yes"]

    proc += self.ToolSpecificFlags()
    proc += self._tool_flags

    suppression_count = 0
    for suppression_file in self._options.suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    logfilename = self.TMP_DIR + ("/%s." % tool_name) + "%p"
    if self.UseXML():
      proc += ["--xml=yes", "--xml-file=" + logfilename]
    else:
      proc += ["--log-file=" + logfilename]

    # The Valgrind command is constructed.

    if self._options.indirect:
      self.CreateBrowserWrapper(" ".join(proc), logfilename)
      proc = []
    proc += self._args
    return proc

  def ToolSpecificFlags(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def Cleanup(self):
    if common.IsWine():
      # Shutdown the Wine server.
      common.RunSubprocess([os.environ.get('WINESERVER'), '-k'])

  def CreateBrowserWrapper(self, command, logfiles):
    """The program being run invokes Python or something else
    that can't stand to be valgrinded, and also invokes
    the Chrome browser.  Set an environment variable to
    tell the program to prefix the Chrome commandline
    with a magic wrapper.  Build the magic wrapper here.
    """
    (fd, indirect_fname) = tempfile.mkstemp(dir=self.TMP_DIR,
                                            prefix="browser_wrapper.",
                                            text=True)
    f = os.fdopen(fd, "w")
    f.write("#!/bin/sh\n")
    f.write('echo "Started Valgrind wrapper for this test, PID=$$"\n')
    # Add the PID of the browser wrapper to the logfile names so we can
    # separate log files for different UI tests at the analyze stage.
    f.write(command.replace("%p", "$$.%p"))
    f.write(' "$@"\n')
    f.close()
    os.chmod(indirect_fname, stat.S_IRUSR|stat.S_IXUSR)
    os.putenv("BROWSER_WRAPPER", indirect_fname)
    logging.info('export BROWSER_WRAPPER=' + indirect_fname)

  def CreateAnalyzer(self, filenames):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def GetAnalyzeResults(self, check_sanity=False):
    # Glob all the files in the "testing.tmp" directory
    filenames = glob.glob(self.TMP_DIR + "/" + self.ToolName() + ".*")

    # If we have browser wrapper, the logfiles are named as
    # "toolname.wrapper_PID.valgrind_PID".
    # Let's extract the list of wrapper_PIDs and name it ppids
    ppids = set([int(f.split(".")[-2]) \
                for f in filenames if re.search("\.[0-9]+\.[0-9]+$", f)])

    if len(ppids) == 0:
      # Fast path - no browser wrapper was set.
      return self.CreateAnalyzer(filenames).Report(check_sanity)

    ret = 0
    for ppid in ppids:
      print "====================================================="
      print " Below is the report for valgrind wrapper PID=%d." % ppid
      print " You can find the corresponding test "
      print " by searching the above log for 'PID=%d'" % ppid
      sys.stdout.flush()

      ppid_filenames = [f for f in filenames \
                        if re.search("\.%d\.[0-9]+$" % ppid, f)]
      # check_sanity won't work with browser wrappers
      assert check_sanity == False
      ret |= self.CreateAnalyzer(ppid_filenames).Report()
      print "====================================================="
      sys.stdout.flush()

    if ret != 0:
      print ""
      print "The Valgrind reports are grouped by test names."
      print "Each test has its PID printed in the log when the test was run"
      print "and at the beginning of its Valgrind report."
      sys.stdout.flush()

    return ret

# TODO(timurrrr): Split into a separate file.
class Memcheck(ValgrindTool):
  """Memcheck
  Dynamic memory error detector for Linux & Mac

  http://valgrind.org/info/tools.html#memcheck
  """

  def __init__(self):
    ValgrindTool.__init__(self)
    self.RegisterOptionParserHook(Memcheck.ExtendOptionParser)

  def ToolName(self):
    return "memcheck"

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--show_all_leaks", action="store_true",
                      default=False,
                      help="also show less blatant leaks")
    parser.add_option("", "--track_origins", action="store_true",
                      default=False,
                      help="Show whence uninitialized bytes came. 30% slower.")

  def ToolSpecificFlags(self):
    ret = ["--leak-check=full", "--gen-suppressions=all", "--demangle=no"]

    if self._options.show_all_leaks:
      ret += ["--show-reachable=yes"]
    else:
      ret += ["--show-possible=no"]

    if self._options.track_origins:
      ret += ["--track-origins=yes"]

    return ret

  def CreateAnalyzer(self, filenames):
    use_gdb = common.IsMac()
    return memcheck_analyze.MemcheckAnalyze(self._source_dir, filenames,
                                            self._options.show_all_leaks,
                                            use_gdb=use_gdb)

  def Analyze(self, check_sanity=False):
    ret = self.GetAnalyzeResults(check_sanity)

    if ret != 0:
      logging.info("Please see http://dev.chromium.org/developers/how-tos/"
                   "using-valgrind for the info on Memcheck/Valgrind")
    return ret

class PinTool(BaseTool):
  """Abstract class for running PIN tools.

  Always subclass this and implement ToolSpecificFlags() and
  ExtendOptionParser() for tool-specific stuff.
  """
  def __init__(self):
    BaseTool.__init__(self)

  def PrepareForTest(self):
    pass

  def ToolSpecificFlags(self):
    raise NotImplementedError, "This method should be implemented " \
                               "in the tool-specific subclass"

  def ToolCommand(self):
    """Get the PIN command to run."""

    # Construct the PIN command.
    pin_cmd = os.getenv("PIN_COMMAND")
    if not pin_cmd:
      raise RuntimeError, "Please set PIN_COMMAND environment variable " \
                          "with the path to pin.exe"
    proc = pin_cmd.split(" ")

    proc += self.ToolSpecificFlags()

    # The PIN command is constructed.

    # PIN requires -- to separate PIN flags from the executable name.
    # self._args begins with the exe to be run.
    proc += ["--"]

    proc += self._args
    return proc

class ThreadSanitizerBase(object):
  """ThreadSanitizer
  Dynamic data race detector for Linux, Mac and Windows.

  http://code.google.com/p/data-race-test/wiki/ThreadSanitizer

  Since TSan works on both Valgrind (Linux, Mac) and PIN (Windows), we need
  to have multiple inheritance
  """

  INFO_MESSAGE="Please see http://dev.chromium.org/developers/how-tos/" \
               "using-valgrind/threadsanitizer for the info on " \
               "ThreadSanitizer"

  def __init__(self):
    self.RegisterOptionParserHook(ThreadSanitizerBase.ExtendOptionParser)

  def ToolName(self):
    return "tsan"

  def UseXML(self):
    return False

  def SelfContained(self):
    return True

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--hybrid", default="no",
                      dest="hybrid",
                      help="Finds more data races, may give false positive "
                      "reports unless the code is annotated")
    parser.add_option("", "--announce-threads", default="yes",
                      dest="announce_threads",
                      help="Show the the stack traces of thread creation")

  def EvalBoolFlag(self, flag_value):
    if (flag_value in ["1", "true", "yes"]):
      return True
    elif (flag_value in ["0", "false", "no"]):
      return False
    raise RuntimeError, "Can't parse flag value (%s)" % flag_value

  def ToolSpecificFlags(self):
    ret = []

    ignore_files = ["ignores.txt"]
    for platform_suffix in common.PlatformNames():
      ignore_files.append("ignores_%s.txt" % platform_suffix)
    for ignore_file in ignore_files:
      fullname =  os.path.join(self._source_dir,
          "tools", "valgrind", "tsan", ignore_file)
      if os.path.exists(fullname):
        ret += ["--ignore=%s" % fullname]

    # This should shorten filepaths for local builds.
    ret += ["--file-prefix-to-cut=%s/" % self._source_dir]

    # This should shorten filepaths on bots.
    ret += ["--file-prefix-to-cut=build/src/"]

    # This should shorten filepaths for functions intercepted in TSan.
    ret += ["--file-prefix-to-cut=scripts/tsan/tsan/"]

    if self.EvalBoolFlag(self._options.hybrid):
      ret += ["--hybrid=yes"] # "no" is the default value for TSAN

    if self.EvalBoolFlag(self._options.announce_threads):
      ret += ["--announce-threads"]

    # --show-pc flag is needed for parsing the error logs on Darwin.
    if platform_suffix == 'mac':
      ret += ["--show-pc=yes"]

    # Don't show googletest frames in stacks.
    ret += ["--cut_stack_below=testing*Test*Run*"]

    return ret

class ThreadSanitizerPosix(ThreadSanitizerBase, ValgrindTool):
  def __init__(self):
    ValgrindTool.__init__(self)
    ThreadSanitizerBase.__init__(self)

  def ToolSpecificFlags(self):
    proc = ThreadSanitizerBase.ToolSpecificFlags(self)
    # The -v flag is needed for printing the list of used suppressions and
    # obtaining addresses for loaded shared libraries on Mac.
    proc += ["-v"]
    return proc

  def CreateAnalyzer(self, filenames):
    use_gdb = common.IsMac()
    return tsan_analyze.TsanAnalyze(self._source_dir, filenames)

  def Analyze(self, check_sanity=False):
    ret = self.GetAnalyzeResults(check_sanity)

    if ret != 0:
      logging.info(self.INFO_MESSAGE)
    return ret

class ThreadSanitizerWindows(ThreadSanitizerBase, PinTool):
  def __init__(self):
    PinTool.__init__(self)
    ThreadSanitizerBase.__init__(self)
    self.RegisterOptionParserHook(ThreadSanitizerWindows.ExtendOptionParser)

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--suppressions", default=[],
                      action="append",
                      help="path to TSan suppression file")


  def ToolSpecificFlags(self):
    proc = ThreadSanitizerBase.ToolSpecificFlags(self)
    # On PIN, ThreadSanitizer has its own suppression mechanism
    # and --log-file flag which work exactly on Valgrind.
    suppression_count = 0
    for suppression_file in self._options.suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    logfilename = self.TMP_DIR + "/tsan.%p"
    proc += ["--log-file=" + logfilename]

    # TODO(timurrrr): Add flags for Valgrind trace children analog when we
    # start running complex tests (e.g. UI) under TSan/Win.

    return proc

  def Analyze(self, check_sanity=False):
    filenames = glob.glob(self.TMP_DIR + "/tsan.*")
    analyzer = tsan_analyze.TsanAnalyze(self._source_dir, filenames)
    ret = analyzer.Report(check_sanity)
    if ret != 0:
      logging.info(self.INFO_MESSAGE)
    return ret


class DrMemory(BaseTool):
  """Dr.Memory
  Dynamic memory error detector for Windows.

  http://dynamorio.org/drmemory.html
  It is not very mature at the moment, some things might not work properly.
  """

  def __init__(self):
    BaseTool.__init__(self)
    self.RegisterOptionParserHook(DrMemory.ExtendOptionParser)

  def ToolName(self):
    return "DrMemory"

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--suppressions", default=[],
                      action="append",
                      help="path to a drmemory suppression file")

  def ToolCommand(self):
    """Get the valgrind command to run."""
    tool_name = self.ToolName()

    pin_cmd = os.getenv("DRMEMORY_COMMAND")
    if not pin_cmd:
      raise RuntimeError, "Please set DRMEMORY_COMMAND environment variable " \
                          "with the path to drmemory.exe"
    proc = pin_cmd.split(" ")

    # Dump Dr.Memory events on error
    # proc += ["-dr_ops", "dumpcore_mask 0x8bff"]

    suppression_count = 0
    for suppression_file in self._options.suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        # DrMemory doesn't support multiple suppressions file... oh well
        assert suppression_count <= 1
        proc += ["-suppress", suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    proc += ["-logdir", (os.getcwd() + "\\" + self.TMP_DIR)]
    proc += ["-batch", "-quiet"]

    # Dr.Memory requires -- to separate tool flags from the executable name.
    proc += ["--"]

    # Note that self._args begins with the name of the exe to be run.
    self._args[0] += ".exe" # HACK
    proc += self._args
    return proc

  def Analyze(self, check_sanity=False):
    # Glob all the results files in the "testing.tmp" directory
    filenames = glob.glob(self.TMP_DIR + "/*/results.txt")

    analyzer = drmemory_analyze.DrMemoryAnalyze(self._source_dir, filenames)
    ret = analyzer.Report(check_sanity)
    return ret


class ToolFactory:
  def Create(self, tool_name):
    if tool_name == "memcheck" and not common.IsWine():
      return Memcheck()
    if tool_name == "wine_memcheck" and common.IsWine():
      return Memcheck()
    if tool_name == "tsan":
      if common.IsWindows():
        logging.info("WARNING: ThreadSanitizer on Windows is experimental.")
        return ThreadSanitizerWindows()
      else:
        return ThreadSanitizerPosix()
    if tool_name == "drmemory":
      return DrMemory()
    try:
      platform_name = common.PlatformNames()[0]
    except common.NotImplementedError:
      platform_name = sys.platform + "(Unknown)"
    raise RuntimeError, "Unknown tool (tool=%s, platform=%s)" % (tool_name,
                                                                 platform_name)

def RunTool(argv, module):
  # TODO(timurrrr): customize optparse instead
  tool_name = "memcheck"
  args = argv[1:]
  for arg in args:
    if arg.startswith("--tool="):
      tool_name = arg[7:]
      args.remove(arg)
      break

  tool = ToolFactory().Create(tool_name)
  MODULES_TO_SANITY_CHECK = ["base"]

  # TODO(timurrrr): this is a temporary workaround for http://crbug.com/47844
  if tool_name == "tsan" and common.IsMac():
    MODULES_TO_SANITY_CHECK = []

  check_sanity = module in MODULES_TO_SANITY_CHECK
  return tool.Main(args, check_sanity)

if __name__ == "__main__":
  if sys.argv.count("-v") > 0 or sys.argv.count("--verbose") > 0:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()
  # TODO(timurrrr): valgrind tools may use -v/--verbose as well

  ret = RunTool(sys.argv)
  sys.exit(ret)
