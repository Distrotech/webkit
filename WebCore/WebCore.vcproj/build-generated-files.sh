#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitOutputDirUnix=`cygpath -a -u "$2"`
WebKitOutputConfigDirUnix="${WebKitOutputDirUnix}/$1"

SRCROOT="`pwd`/../../../../OpenSource/WebCore"
SRCROOT=`realpath "$SRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SRCROOT=`cygpath -m -s "$SRCROOT"`
SRCROOT=`cygpath -u "$SRCROOT"`
export SRCROOT

# FIXME: Eventually win32 might wish to generate to the Debug/Release output directories.
export BUILT_PRODUCTS_DIR="$SRCROOT"
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/kjs/create_hash_table"

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore"

export WebCore="${SRCROOT}"
export ENCODINGS_FILE="${WebCore}/platform/win/win-encodings.txt";
export ENCODINGS_PREFIX=""
# FIXME: Should make XPath Support configurable someday on Win32.
export FEATURE_DEFINES="XPATH_SUPPORT"
make -f "$WebCore/DerivedSources.make" || exit 1
