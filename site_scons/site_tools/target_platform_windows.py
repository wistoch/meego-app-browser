#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Build tool setup for Windows.

This module is a SCons tool which should be include in the topmost windows
environment.
It is used as follows:
  env = base_env.Clone(tools = ['component_setup'])
  win_env = base_env.Clone(tools = ['target_platform_windows'])
"""


import os
import time
import command_output
import SCons.Script


def WaitForWritable(target, source, env):
  """Waits for the target to become writable.

  Args:
    target: List of target nodes.
    source: List of source nodes.
    env: Environment context.

  Returns:
    Zero if success, nonzero if error.

  This is a necessary hack on Windows, where antivirus software can lock exe
  files briefly after they're written.  This can cause subsequent reads of the
  file by env.Install() to fail.  To prevent these failures, wait for the file
  to be writable.
  """
  target_path = target[0].abspath
  if not os.path.exists(target_path):
    return 0      # Nothing to wait for

  for retries in range(10):
    try:
      f = open(target_path, 'a+b')
      f.close()
      return 0    # Successfully opened file for write, so we're done
    except (IOError, OSError):
      print 'Waiting for access to %s...' % target_path
      time.sleep(1)

  # If we're still here, fail
  print 'Timeout waiting for access to %s.' % target_path
  return 1


def RunManifest(target, source, env, cmd):
  """Run the Microsoft Visual Studio manifest tool (mt.exe).

  Args:
    target: List of target nodes.
    source: List of source nodes.
    env: Environment context.
    cmd: Command to run.

  Returns:
    Zero if success, nonzero if error.

  The mt.exe tool seems to experience intermittent failures trying to write to
  .exe or .dll files.  Antivirus software makes this worse, but the problem
  can still occur even if antivirus software is disabled.  The failures look
  like:

      mt.exe : general error c101008d: Failed to write the updated manifest to
      the resource of file "(name of exe)". Access is denied.

  with mt.exe returning an errorlevel (return code) of 31.  The workaround is
  to retry running mt.exe after a short delay.
  """
  cmdline = env.subst(cmd, target=target, source=source)

  for retry in range(5):
    # If this is a retry, print a message and delay first
    if retry:
      # mt.exe failed to write to the target file.  Print a warning message,
      # delay 3 seconds, and retry.
      print 'Warning: mt.exe failed to write to %s; retrying.' % target[0]
      time.sleep(3)

    return_code, output = command_output.RunCommand(
        cmdline, env=env['ENV'], echo_output=False)
    if return_code != 31:    # Something other than the intermittent error
      break

  # Pass through output (if any) and return code from manifest
  if output:
    print output
  return return_code


def RunManifestExe(target, source, env):
  """Calls RunManifest for updating an executable (resource_num=1)."""
  return RunManifest(target, source, env, cmd='$MANIFEST_COM')


def RunManifestDll(target, source, env):
  """Calls RunManifest for updating a dll (resource_num=2)."""
  return RunManifest(target, source, env, cmd='$SHMANIFEST_COM')


def ComponentPlatformSetup(env, builder_name):
  """Hook to allow platform to modify environment inside a component builder.

  This is called on a clone of the environment passed into the component
  builder, and is the last modification done to that environment before using
  it to call the underlying SCons builder (env.Program(), env.Library(), etc.)

  Args:
    env: Environment to modify
    builder_name: Name of the builder
  """
  if env.get('ENABLE_EXCEPTIONS'):
    env.FilterOut(
        CPPDEFINES=['_HAS_EXCEPTIONS=0'],
        # There are problems with LTCG when some files are compiled with
        # exceptions and some aren't (the v-tables for STL and BOOST classes
        # don't match).  Therefore, turn off LTCG when exceptions are enabled.
        CCFLAGS=['/GL'],
        LINKFLAGS=['/LTCG'],
        ARFLAGS=['/LTCG'],
    )
    env.Append(CCFLAGS=['/EHsc'])

  if builder_name in ('ComponentObject', 'ComponentLibrary'):
    if env.get('COMPONENT_STATIC'):
      env.Append(CPPDEFINES=['_LIB'])
    else:
      env.Append(CPPDEFINES=['_USRDLL', '_WINDLL'])

  if builder_name == 'ComponentTestProgram':
    env.FilterOut(
        CPPDEFINES=['_WINDOWS'],
        LINKFLAGS=['/SUBSYSTEM:WINDOWS'],
    )
    env.Append(
        CPPDEFINES=['_CONSOLE'],
        LINKFLAGS=['/SUBSYSTEM:CONSOLE'],
    )

  # Make sure link methods are lists, so we can append to them below
  env['LINKCOM'] = [env['LINKCOM']]
  env['SHLINKCOM'] = [env['SHLINKCOM']]

  # Support manifest file generation and consumption
  if env.get('MANIFEST_FILE'):
    env.Append(
        LINKCOM=[SCons.Script.Action(RunManifestExe, '$MANIFEST_COMSTR')],
        SHLINKCOM=[SCons.Script.Action(RunManifestDll, '$SHMANIFEST_COMSTR')],
    )

    # If manifest file should be autogenerated, add the -manifest link line and
    # delete the generated manfest after running mt.exe.
    if env.get('MANFEST_FILE_GENERATED_BY_LINK'):
      env.Append(
          LINKFLAGS=['-manifest'],
          LINKCOM=[SCons.Script.Delete('$MANFEST_FILE_GENERATED_BY_LINK')],
          SHLINKCOM=[SCons.Script.Delete('$MANFEST_FILE_GENERATED_BY_LINK')],
      )

  # Wait for the output file to be writable before releasing control to
  # SCons.  Windows virus scanners temporarily lock modified executable files
  # for scanning, which causes SCons's env.Install() to fail intermittently.
  env.Append(
      LINKCOM=[SCons.Script.Action(WaitForWritable, None)],
      SHLINKCOM=[SCons.Script.Action(WaitForWritable, None)],
  )

#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Bring in the outside PATH, INCLUDE, and LIB if not blocked.
  if not env.get('MSVC_BLOCK_ENVIRONMENT_CHANGES'):
    env.AppendENVPath('PATH', os.environ.get('PATH', '[]'))
    env.AppendENVPath('INCLUDE', os.environ.get('INCLUDE', '[]'))
    env.AppendENVPath('LIB', os.environ.get('LIB', '[]'))

  # Load various Visual Studio related tools.
  env.Tool('as')
  env.Tool('msvs')
  env.Tool('windows_hard_link')

  pre_msvc_env = env['ENV'].copy()

  env.Tool('msvc')
  env.Tool('mslib')
  env.Tool('mslink')

  # Find VC80_DIR if it isn't already set.
  if not env.get('VC80_DIR'):
    # Look in each directory in the path for cl.exe.
    for p in env['ENV']['PATH'].split(os.pathsep):
      # Use the directory two layers up if it exists.
      if os.path.exists(os.path.join(p, 'cl.exe')):
        env['VC80_DIR'] = os.path.dirname(os.path.dirname(p))

  # The msvc, mslink, and mslib tools search the registry for installed copies
  # of Visual Studio and prepends them to the PATH, INCLUDE, and LIB
  # environment variables.  Block these changes if necessary.
  if env.get('MSVC_BLOCK_ENVIRONMENT_CHANGES'):
    env['ENV'] = pre_msvc_env

  # Declare bits
  DeclareBit('windows', 'Target platform is windows.',
             exclusive_groups=('target_platform'))
  env.SetBits('windows')

  env.Replace(
      TARGET_PLATFORM='WINDOWS',
      COMPONENT_PLATFORM_SETUP=ComponentPlatformSetup,

      # A better rebuild command (actually cleans, then rebuild)
      MSVSREBUILDCOM=''.join(['$MSVSSCONSCOM -c "$MSVSBUILDTARGET" && ',
                              '$MSVSSCONSCOM "$MSVSBUILDTARGET"']),
  )

  env.SetDefault(
      # Command line option to include a header
      CCFLAG_INCLUDE='/FI',

      # Generate PDBs matching target name by default.
      PDB='${TARGET.base}.pdb',

      # Code coverage related.
      COVERAGE_LINKFLAGS='/PROFILE',  # Requires vc_80 or higher.
      COVERAGE_LINKCOM_EXTRAS='$COVERAGE_VSINSTR /COVERAGE $TARGET',
      # NOTE: need to ignore error in return type here, the tool has issues.
      #   Thus a - is added.
      COVERAGE_START_CMD=[
          # If a previous build was cancelled or crashed, VSPerfCmd may still
          # be running, which causes future coverage runs to fail.  Make sure
          # it's shut down before starting coverage up again.
          '-$COVERAGE_VSPERFCMD -shutdown',
          '$COVERAGE_VSPERFCMD -start:coverage '
          '-output:${COVERAGE_OUTPUT_FILE}.pre'],
      COVERAGE_STOP_CMD=[
          '-$COVERAGE_VSPERFCMD -shutdown',
          '$COVERAGE_ANALYZER -sym_path=. ${COVERAGE_OUTPUT_FILE}.pre.coverage',
          SCons.Script.Copy('$COVERAGE_OUTPUT_FILE',
                            '${COVERAGE_OUTPUT_FILE}.pre.coverage.lcov'),
      ],
      COVERAGE_EXTRA_PATHS=['$COVERAGE_ANALYZER_DIR'],

      # Manifest options
      # When link.exe is run with '-manifest', it always generated a manifest
      # with this name.
      MANFEST_FILE_GENERATED_BY_LINK='${TARGET}.manifest',
      # Manifest file to use as input to mt.exe.  Can be overridden to pass in
      # a pregenerated manifest file.
      MANIFEST_FILE='$MANFEST_FILE_GENERATED_BY_LINK',
      MANIFEST_COM=('mt.exe -nologo -manifest "$MANIFEST_FILE" '
                    '-outputresource:"$TARGET";1'),
      MANIFEST_COMSTR='$MANIFEST_COM',
      SHMANIFEST_COM=('mt.exe -nologo -manifest "$MANIFEST_FILE" '
                      '-outputresource:"$TARGET";2'),
      SHMANIFEST_COMSTR='$SHMANIFEST_COM',
  )

  env.Append(
      HOST_PLATFORMS=['WINDOWS'],
      CPPDEFINES=['OS_WINDOWS=OS_WINDOWS'],

      # Turn up the warning level
      CCFLAGS=['/W3'],

      # Force x86 platform, generate manifests
      LINKFLAGS=['/MACHINE:X86'],
      ARFLAGS=['/MACHINE:X86'],

      # Settings for debug
      CCFLAGS_DEBUG=[
          '/Od',     # disable optimizations
          '/RTC1',   # enable fast checks
          '/MTd',    # link with LIBCMTD.LIB debug lib
      ],
      LINKFLAGS_DEBUG=['/DEBUG'],

      # Settings for optimized
      CCFLAGS_OPTIMIZED=[
          '/O1',     # optimize for size
          '/MT',     # link with LIBCMT.LIB (multi-threaded, static linked crt)
          '/GS',     # enable security checks
      ],
      LINKFLAGS_OPTIMIZED=['/PDBPATH:none'],

      # Settings for component_builders
      COMPONENT_LIBRARY_LINK_SUFFIXES=['.lib'],
      COMPONENT_LIBRARY_DEBUG_SUFFIXES=['.pdb'],
  )

  # TODO(sgk): mslink.py creates a shlibLinkAction which doesn't specify
  # '$SHLINKCOMSTR' as its command string.  This breaks --brief.  For now,
  # hack into the existing action and override its command string.
  env['SHLINKCOM'].list[0].cmdstr = '$SHLINKCOMSTR'
