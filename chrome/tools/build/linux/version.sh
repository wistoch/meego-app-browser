#!/bin/sh
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(mmoss) This is (mostly) just a conversion to sh syntax of
# tools/build/win/version.bat. Rewrite both as common python.

TMPL="$1"
OUTFILE="$2"
CHROMEDIR="$3"

# Load version digits as environment variables.
source "$CHROMEDIR/VERSION"

# Load branding strings as environment variables
DISTRIBUTION="chromium"
if [ "$CHROMIUM_BUILD" = "_google_chrome" ]; then
  DISTRIBUTION="google_chrome"
fi
# Make sure the string values are quoted.
eval $(sed -e 's/\([^=]*\)=\(.*\)/\1="\2"/' \
  "$CHROMEDIR/app/theme/$DISTRIBUTION/BRANDING")

# Determine the current repository revision number.
LASTCHANGE=$(svn info 2>/dev/null | grep "Revision:" | cut -d" " -f2-)
if [ -z "$LASTCHANGE" ]; then
  # Maybe it's a git client
  LASTCHANGE=$(git-svn info 2>/dev/null | grep "Revision:" | cut -d" " -f2-)
fi

OFFICIAL_BUILD="false"
if [ "$CHROME_BUILD_TYPE" = "_official" ]; then
  OFFICIAL_BUILD="true"
fi

# TODO(mmoss) Make sure no sed special chars in substitutions.
sed -e "s/@MAJOR@/$MAJOR/" \
    -e "s/@MINOR@/$MINOR/" \
    -e "s/@BUILD@/$BUILD/" \
    -e "s/@PATCH@/$PATCH/" \
    -e "s/@COMPANY_FULLNAME@/$COMPANY_FULLNAME/" \
    -e "s/@COMPANY_SHORTNAME@/$COMPANY_SHORTNAME/" \
    -e "s/@PRODUCT_FULLNAME@/$PRODUCT_FULLNAME/" \
    -e "s/@PRODUCT_SHORTNAME@/$PRODUCT_SHORTNAME/" \
    -e "s/@PRODUCT_EXE@/$PRODUCT_EXE/" \
    -e "s/@COPYRIGHT@/$COPYRIGHT/" \
    -e "s/@OFFICIAL_BUILD@/$OFFICIAL_BUILD/" \
    -e "s/@LASTCHANGE@/$LASTCHANGE/" "$TMPL" > "$OUTFILE"
