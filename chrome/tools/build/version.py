#/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
version.py -- Chromium version string substitution utility.
"""

import getopt
import os
import re
import subprocess
import sys


class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def svn_fetch_revision():
  """
  Fetch the Subversion revision for the local tree.

  Errors are swallowed.
  """
  try:
    p = subprocess.Popen(['svn', 'info'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
  except OSError:
    # 'svn' is apparently either not installed or not executable.
    return None
  revision = None
  if p:
    svn_re = re.compile('^Revision:\s+(\S+)$', re.M)
    m = svn_re.search(p.stdout.read())
    if m:
      revision = m.group(1)
  return revision


def git_fetch_id():
  """
  Fetch the GIT identifier for the local tree.

  Errors are swallowed.
  """
  try:
    p = subprocess.Popen(['git', 'log', '-1'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
  except OSError:
    # 'git' is apparently either not installed or not executable.
    return None
  id = None
  if p:
    git_re = re.compile('^\s*git-svn-id:\s+(\S+)@(\d+)', re.M)
    m = git_re.search(p.stdout.read())
    if m:
      id = m.group(1)
  return id


def fetch_values_from_file(values_dict, file_name):
  """
  Fetches KEYWORD=VALUE settings from the specified file.

  Everything to the left of the first '=' is the keyword,
  everything to the right is the value.  No stripping of
  white space, so beware.

  The file must exist, otherwise you get the Python exception from open().
  """
  for line in open(file_name, 'r').readlines():
    key, val = line.rstrip('\r\n').split('=', 1)
    values_dict[key] = val


def fetch_values(file_list):
  """
  Returns a dictionary of values to be used for substitution, populating
  the dictionary with KEYWORD=VALUE settings from the files in 'file_list'.

  Explicitly adds the following values from internal calculations:

    LASTCHANGE  (the SVN revision, or GIT id of the local tree)
    OFFICIAL_BUILD
  """
  change = svn_fetch_revision()
  if not change and sys.platform in ('linux2',):
    change = git_fetch_id()
  if not change:
    change = '0'

  CHROME_BUILD_TYPE = os.environ.get('CHROME_BUILD_TYPE')
  if CHROME_BUILD_TYPE == '_official':
    official_build = '1'
  else:
    official_build = '0'

  values = dict(
    LASTCHANGE = change,
    OFFICIAL_BUILD = official_build,
  )

  for file_name in file_list:
    fetch_values_from_file(values, file_name)

  return values


def subst_contents(file_name, values):
  """
  Returns the contents of the specified file_name with substited
  values from the specified dictionary.

  Keywords to be substituted are surrounded by '@':  @KEYWORD@.

  No attempt is made to avoid recursive substitution.  The order
  of evaluation is random based on the order of the keywords returned
  by the Python dictionary.  So do NOT substitute a value that
  contains any @KEYWORD@ strings expecting them to be recursively
  substituted, okay?
  """
  contents = open(file_name, 'r').read()
  for key, val in values.items():
    try:
      contents = contents.replace('@' + key + '@', val)
    except TypeError:
      print repr(key), repr(val)
  return contents


def write_if_changed(file_name, contents):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  """
  try:
    old_contents = open(file_name, 'r').read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents:
      return
    os.unlink(file_name)
  open(file_name, 'w').write(contents)


def main(argv=None):
  if argv is None:
    argv = sys.argv

  short_options = 'f:h'
  long_options = ['file=', 'help']

  helpstr = """\
Usage:  version.py [-h] [-f FILE] INFILE OUTFILE

  -f FILE, --file=FILE          Read variables from FILE.
  -h, --help                    Print this help and exit.
"""

  variable_files = []

  try:
    try:
      opts, args = getopt.getopt(argv[1:], short_options, long_options)
    except getopt.error, msg:
      raise Usage(msg)
    for o, a in opts:
      if o in ('-f', '--file'):
        variable_files.append(a)
      elif o in ('-h', '--help'):
        print helpstr
        return 0
    try:
      in_file, out_file = args
    except ValueError:
      msg = 'Incorrect number of arguments: %r' % args
      raise Usage(msg)
  except Usage, err:
    sys.stderr.write(err.msg)
    sys.stderr.write('Use -h to get help.')
    return 2

  values = fetch_values(variable_files)

  contents = subst_contents(in_file, values)

  write_if_changed(out_file, contents)

  return 0


if __name__ == '__main__':
  sys.exit(main())
