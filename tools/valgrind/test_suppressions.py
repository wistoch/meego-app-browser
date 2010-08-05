# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import os
import re
import sys

import path_utils

import suppressions


def ReadReportsFromFile(filename):
  input_file = file(filename, 'r')
  reports = []
  in_suppression = False
  cur_supp = []
  for line in input_file:
    line = line.strip()
    line = line.replace("</span><span class=\"stdout\">", "")
    line = line.replace("</span><span class=\"stderr\">", "")
    line = line.replace("&lt;", "<")
    line = line.replace("&gt;", ">")
    if in_suppression:
      if line == "}":
        cur_supp += ["}"]
        reports += ["\n".join(cur_supp)]
        in_suppression = False
        cur_supp = []
      else:
        cur_supp += [" "*3 + line]
    elif line == "{":
      in_suppression = True
      cur_supp = ["{"]
  return reports,line

suppressions_root = path_utils.ScriptDir()

supp_filename = os.path.join(suppressions_root,
                             "memcheck", "suppressions.txt")
common_suppressions = suppressions.ReadSuppressionsFromFile(supp_filename)

supp_filename = os.path.join(suppressions_root,
                             "memcheck", "suppressions_mac.txt")
mac_suppressions = suppressions.ReadSuppressionsFromFile(supp_filename)

# all_reports is a map {report: list of urls containing this report}
all_reports = defaultdict(list)

for f in sys.argv[1:]:
  f_reports, url = ReadReportsFromFile(f)
  for report in f_reports:
    all_reports[report] += [url]

reports_count = 0
for r in all_reports:
  if set([False]) == set([not re.search("%20Mac%20", url)\
                         for url in all_reports[r]]):
    # Include mac suppressions if the report is only present on Mac
    cur_supp = common_suppressions + mac_suppressions
  else:
    cur_supp = common_suppressions

  match = False
  for s in cur_supp:
    if s.Match(r.split("\n")):
      match = True
      break
  if not match:
    reports_count += 1
    print "==================================="
    print "This report observed at"
    for url in all_reports[r]:
      print "  %s" % url
    print "didn't match any suppressions:"
    print r
    print "==================================="

if reports_count > 0:
  print "%d unique reports don't match any of the suppressions" % reports_count
else:
  print "Congratulations! All reports are suppressed!"
  # TODO(timurrrr): also make sure none of the old suppressions
  # were narrowed too much.
