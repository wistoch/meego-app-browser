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

"""Source concatenation builder for SCons."""


import SCons.Script


def ConcatSourceBuilder(target, source, env):
  """ConcatSource builder.

  Args:
    target: List of target nodes
    source: List of source nodes
    env: Environment in which to build

  Returns:
    None if successful; 1 if error.
  """
  if len(target) != 1:
    print 'ERROR: multiple ConcatSource targets when 1 expected'
    return 1

  output_lines = [
      '// This file is auto-generated by the ConcatSource builder.']

  for source_path in map(str, source):
    if env.get('CC') == 'cl':
      # Add message pragma for nicer progress indication when building with
      # MSVC.
      output_lines.append('#pragma message("--%s")' % (
          source_path.replace("\\", "/")))

    output_lines.append('#include "%s"' % source_path)

  output_file = open(str(target[0]), 'w')
  # Need an EOL at the end of the file for more finicky build tools
  output_file.write('\n'.join(output_lines) + '\n')
  output_file.close()


def ConcatSourcePseudoBuilder(self, target, source):
  """ConcatSource pseudo-builder; calls builder or passes through source nodes.

  Args:
    self: Environment in which to build
    target: List of target nodes
    source: List of source nodes

  Returns:
    If self['CONCAT_SOURCE_ENABLE'], calls self.ConcatSource and returns
    the list of target nodes.  Otherwise, returns the list of source nodes.
    Source nodes which are not CPP files are passed through unchanged to the
    list of output nodes.
  """
  if self.get('CONCAT_SOURCE_ENABLE', True):
    # Scan down source list and separate CPP sources (which we concatenate)
    # from other files (which we pass through).
    cppsource = []
    outputs = []
    suffixes = self.Flatten(self.subst_list('$CONCAT_SOURCE_SUFFIXES'))
    for source_file in self.arg2nodes(source):
      if source_file.suffix in suffixes:
        cppsource.append(source_file)
      else:
        outputs.append(source_file)

    if len(cppsource) > 1:
      # More than one file, so concatenate them together
      outputs += self.ConcatSourceBuilder(target, cppsource)
    else:
      # <2 files, so pass them through; no need for a ConcatSource target
      outputs += cppsource
    return outputs
  else:
    # ConcatSource is disabled, so pass through the list of source nodes.
    return source


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  # Add the builder
  action = SCons.Script.Action(ConcatSourceBuilder,
                               varlist = ['CONCAT_SOURCE_SUFFIXES'])
  builder = SCons.Script.Builder(action = action, suffix = '$CXXFILESUFFIX')
  env.Append(BUILDERS={'ConcatSourceBuilder': builder})

  # Suffixes of sources we can concatenate.  Files not in this list will be
  # passed through untouched.  (Note that on Mac, Objective C/C++ files
  # cannot be concatenated with regular C/C++ files.)
  # TODO(rspangler): Probably shouldn't mix C, C++ either...
  env['CONCAT_SOURCE_SUFFIXES'] = ['.c', '.C', '.cxx', '.cpp', '.c++', '.cc',
                                   '.h', '.H', '.hxx', '.hpp', '.hh']

  # Add a psuedo-builder method which can look at the environment to determine
  # whether to call the ConcatSource builder or not
  env.AddMethod(ConcatSourcePseudoBuilder, 'ConcatSource')
