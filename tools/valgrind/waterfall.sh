#!/bin/bash

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script can be used by waterfall sheriffs to fetch the status
# of Valgrind bots on the memory waterfall and test if their local
# suppressions match the reports on the waterfall.

set -e

THISDIR=$(dirname "${0}")
LOGS_DIR=$THISDIR/waterfall.tmp
WATERFALL_PAGE="http://build.chromium.org/buildbot/memory/builders"

download() {
  # Download a file.
  # $1 = URL to download
  # $2 = Path to the output file
  # {{{1
  if [ "$(which curl)" != "" ]
  then
    if ! curl -s -o "$2" "$1"
    then
      echo
      echo "Failed to download '$1'... aborting"
      exit 1
    fi
  elif [ "$(which wget)" != "" ]
  then
    if ! wget "$1" -O "$2" -q
    then
      echo
      echo "Failed to download '$1'... aborting"
      exit 1
    fi
  else
    echo "Need either curl or wget to download stuff... aborting"
    exit 1
  fi
  # }}}
}

fetch_logs() {
  # Fetch Valgrind logs from the waterfall {{{1

  rm -rf "$LOGS_DIR" # Delete old logs
  mkdir "$LOGS_DIR"

  echo "Fetching the list of builders..."
  download $WATERFALL_PAGE "$LOGS_DIR/builders"
  SLAVES=$(grep "<a href=\"builders\/" "$LOGS_DIR/builders" | \
           sed "s/.*<a href=\"builders\///" | sed "s/\".*//" | \
           sort | uniq)

  for S in $SLAVES
  do
    SLAVE_URL=$WATERFALL_PAGE/$S
    SLAVE_NAME=$(echo $S | sed -e "s/%20/ /g" -e "s/%28/(/g" -e "s/%29/)/g")
    echo -n "Fetching builds by slave '${SLAVE_NAME}'"
    download $SLAVE_URL "$LOGS_DIR/slave_${S}"

    # We speed up the 'fetch' step by skipping the builds/tests which succeeded.
    # TODO(timurrrr): OTOH, we won't be able to check
    # if some suppression is not used anymore.
    LIST_OF_BUILDS=$(grep "<a href=\"\.\./builders/.*/builds/[0-9]\+.*failed" \
                     "$LOGS_DIR/slave_$S" | grep -v "failed compile" | \
                     sed "s/.*\/builds\///" | sed "s/\".*//" | head -n 2)

    for BUILD in $LIST_OF_BUILDS
    do
      BUILD_RESULTS="$LOGS_DIR/slave_${S}_build_${BUILD}"
      download $SLAVE_URL/builds/$BUILD "$BUILD_RESULTS"
      LIST_OF_TESTS=$(grep "<a href=\"[0-9]\+/steps/memory.*/logs/stdio\"" \
                      "$BUILD_RESULTS" | \
                      sed "s/.*a href=\"//" | sed "s/\".*//")
      for TEST in $LIST_OF_TESTS
      do
        REPORT_FILE=$(echo "report_${S}_$TEST" | sed "s/\/logs\/stdio//" | \
                    sed "s/\/steps//" | sed "s/\//_/g")
        echo -n "."
        download $SLAVE_URL/builds/$TEST "$LOGS_DIR/$REPORT_FILE"
        echo $SLAVE_URL/builds/$TEST >> "$LOGS_DIR/$REPORT_FILE"
      done
    done
    echo " DONE"
  done
  # }}}
}

match_suppressions() {
  PYTHONPATH=$THISDIR/../python/google \
             python "$THISDIR/test_suppressions.py" "$LOGS_DIR/report_"*
}

find_blame() {
  # Return the blame list for the first revision for a suppression occured in
  # the logs of a given test on a given builder.
  # $1 - Name of the builder.
  # $2 - Name of the test.
  # $3 - Hash of the suppression.
  # {{{1

  # Validate arguments.
  if [ "$1" = "" -o "$2" = "" -o "$3" = "" ]
  then
    echo "Missing arguments, use waterfall.sh blame <builder> <test> <hash>"
    exit 1
  fi

  SLAVE_NAME="$(echo "$1" | sed -e "s/ /%20/g" -e "s/(/%28/g" -e "s/)/%29/g")"
  if [ ! -f "$LOGS_DIR/slave_$SLAVE_NAME" ]
  then
    echo "I don't know about builder '$1'. Did you run waterfall.sh fetch?"
    exit 1
  fi

  TEST_NAME="memory%20test%3A%20$(echo "$2" | sed -e "s/ /%20/g")"
  if ! ls -1 "$LOGS_DIR"/report_${SLAVE_NAME}_*_${TEST_NAME} >/dev/null 2>&1
  then
    echo "I don't know about the test '$2'. Did you run waterfall.sh fetch?"
    exit 1
  fi

  HASH="#$3#"
  if ! echo "$3" | grep -q -e '^[0-9A-F]\{16\}$'
  then
    echo "The hash '$3' looks strange, it should match ^[0-9A-F]{16}$"
    exit 1
  fi

  # Determine the number of the latest build from our cache.
  LATEST_BUILD=$(grep -l $HASH "$LOGS_DIR"/report_${SLAVE_NAME}_*_${TEST_NAME} |
      sed "s/.*_\\([0-9]*\\)_${TEST_NAME}/\\1/" | sort -rn | head -n 1)

  # Find a version where this hash does not occur.
  STEP_SIZE=1
  FIRST_BUILD=$LATEST_BUILD
  DOWNLOAD_BASE="$WATERFALL_PAGE/$SLAVE_NAME/builds/"
  LOG_FILE="/steps/$TEST_NAME/logs/stdio"
  echo -n "Searching for latest revision without this hash"
  while true
  do
    FIRST_BUILD=$(($FIRST_BUILD - $STEP_SIZE))
    if [ $STEP_SIZE -lt 128 ]
    then
      STEP_SIZE=$(($STEP_SIZE * 2))
    fi
    echo -n "."
    download \
        "$DOWNLOAD_BASE$FIRST_BUILD$LOG_FILE" \
        /tmp/waterfall.$$.report
    if ! grep -ql $HASH /tmp/waterfall.$$.report
    then
      break
    fi
    LATEST_BUILD=$FIRST_BUILD
    rm -f /tmp/waterfall.$$.report
  done
  rm -f /tmp/waterfall.$$.report

  # Now search backwards for the first version where this hash occurs.
  while [ $(($FIRST_BUILD + 1)) -ne $LATEST_BUILD ]
  do
    if [ $STEP_SIZE -gt 1 ]
    then
      STEP_SIZE=$(($STEP_SIZE / 2))
    fi
    TEST_BUILD=$(($FIRST_BUILD + $STEP_SIZE))
    echo -n "."
    download \
        "$DOWNLOAD_BASE$TEST_BUILD$LOG_FILE" \
        /tmp/waterfall.$$.report
    if ! grep -ql $HASH /tmp/waterfall.$$.report
    then
      FIRST_BUILD=$TEST_BUILD
    else
      LATEST_BUILD=$TEST_BUILD
    fi
    rm -f /tmp/waterfall.$$.report
  done
  echo ""
  echo "Hash $HASH occurs in build #$LATEST_BUILD for the first time."
  echo -n "Receiving blame list"
  download "$DOWNLOAD_BASE$LATEST_BUILD" /tmp/waterfall.$$.report
  echo "."
  echo -n "Revision: "
  awk 'BEGIN{FS="[<> ]"}
      /Got Revision:/{print $7}' /tmp/waterfall.$$.report
  echo "Blamelist:"
  awk 'BEGIN{ STATE=0; FS="[<>]" }
      /Blamelist:/{STATE=1}
      /<ol>/{if (STATE == 1) STATE=2}
      /<li>/{if (STATE == 2) print $3}
      /<\/ol>/{STATE=3}' /tmp/waterfall.$$.report
  rm -f /tmp/waterfall.$$.report
  # }}}
}

if [ "$1" = "fetch" ]
then
  fetch_logs
elif [ "$1" = "match" ]
then
  match_suppressions
elif [ "$1" = "blame" ]
then
  find_blame "$2" "$3" "$4"
else
  THISNAME=$(basename "${0}")
  echo "Usage: $THISNAME fetch|match|blame <builder> <test> <hash>"
  echo "  fetch - Fetch Valgrind logs from the memory waterfall"
  echo "  match - Test the local suppression files against the downloaded logs"
  echo "  blame - Return the blame list for the revision where the suppression"
  echo "          <hash> occured for the first time in the log for <test> on"
  echo "          <builder>"
fi
