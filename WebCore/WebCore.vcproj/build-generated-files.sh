#!/usr/bin/bash

if [ -e "../../../../OpenSource/WebCore" ]; then
    SRCROOT="`pwd`/../../../../OpenSource/WebCore"
else
    SRCROOT="`pwd`/.."
fi
SRCROOT=`realpath "$SRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SRCROOT=`cygpath -m -s "$SRCROOT"`
SRCROOT=`cygpath -u "$SRCROOT"`
export SRCROOT
export SOURCE_ROOT=$SRCROOT

DSTROOT="$1"
export DSTROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
DSTROOT=`cygpath -m -s "$DSTROOT"`
DSTROOT=`cygpath -u "$DSTROOT"`
export DSTROOT

SDKROOT="$2"
export SDKROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SDKROOT=`cygpath -m -s "$SDKROOT"`
SDKROOT=`cygpath -u "$SDKROOT"`
export SDKROOT

export BUILT_PRODUCTS_DIR="$DSTROOT/obj/WebCore"
export CREATE_HASH_TABLE="$SDKROOT/include/JavaScriptCore/kjs/create_hash_table"

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources"

export WebCore="${SRCROOT}"
export ENCODINGS_FILE="${WebCore}/platform/win/win-encodings.txt";
export ENCODINGS_PREFIX=""
# FIXME: Should make XPath Support configurable someday on Win32.
export FEATURE_DEFINES="XPATH_SUPPORT"
make -f "$WebCore/DerivedSources.make" || exit 1
