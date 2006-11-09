#!/usr/bin/bash

SRCPATH=`cygpath -u "$1"`
VERSIONPATH=`cygpath -u "$2"`
VERSIONPATH=$VERSIONPATH/include
VERSIONFILE=$VERSIONPATH/autoversion.h
mkdir -p "$VERSIONPATH"

PRODUCTVERSION=`cat "$SRCPATH/PRODUCTVERSION"`
MAJORVERSION=`sed 's/\([^\.]*\)\.\([^.]*\)/\1/' "$SRCPATH/PRODUCTVERSION"`
MINORVERSION=`sed 's/\([^\.]*\)\.\([^.]*\)/\2/' "$SRCPATH/PRODUCTVERSION"`

echo -n `cat "$SRCPATH/VERSION"` > "$VERSIONFILE"

BLDMAJORVERSION=`sed 's/\([^\.]*\)\.\([^.]*\)\.\([^\.]*\)/\1/' "$VERSIONFILE"`
BLDMINORVERSION=`sed 's/\([^\.]*\)\.\([^.]*\)\.\([^\.]*\)/\2/' "$VERSIONFILE"`
BLDVARIANTVERSION=`sed 's/\([^\.]*\)\.\([^.]*\)\.\([^\.]*\)/\3/' "$VERSIONFILE"`
SVNINTERNALREVISION=`svn info | grep '^Revision' | sed 's/^Revision: \(.*\)/\1/'`
SVNOPENSOURCEREVISION=`svn info "$SRCPATH/../../../../OpenSource" | grep '^Revision' | sed 's/^Revision: \(.*\)/\1/'`

BLDNMBR=`cat "$VERSIONFILE"`
BLDNMBRSHORT=`cat "$VERSIONFILE"`
BUILDER=""

if [ "$BUILDBOT" == "" -o "$ARCHIVE_BUILD" == "1" ]; then
    echo -n "+" >> "$VERSIONFILE"
    BLDNMBRSHORT=`cat "$VERSIONFILE"`
    echo -n " " >> "$VERSIONFILE"
    echo -n `whoami` >> "$VERSIONFILE"
    echo -n " - " >> "$VERSIONFILE"
    echo -n `date` >> "$VERSIONFILE"
    echo -n " - r$SVNOPENSOURCEREVISION/r$SVNINTERNALREVISION" >> "$VERSIONFILE"
    BLDNMBR=`cat "$VERSIONFILE"`
fi

echo -n '#define __VERSION_TEXT__ "' > "$VERSIONFILE"
echo -n $PRODUCTVERSION >> "$VERSIONFILE"
echo -n " (" >> "$VERSIONFILE"
echo -n $BLDNMBR >> "$VERSIONFILE"
echo ')"' >> "$VERSIONFILE"
echo -n '#define __BUILD_NUMBER_SHORT__ "' >> "$VERSIONFILE"
echo -n $BLDNMBRSHORT >> "$VERSIONFILE"
echo '"' >> "$VERSIONFILE"

echo -n '#define __VERSION_MAJOR__ ' >> "$VERSIONFILE"
echo $MAJORVERSION >> "$VERSIONFILE"
echo -n '#define __VERSION_MINOR__ ' >> "$VERSIONFILE"
echo $MINORVERSION >> "$VERSIONFILE"

echo -n '#define __BUILD_NUMBER_MAJOR__ ' >> "$VERSIONFILE"
echo $BLDMAJORVERSION >> "$VERSIONFILE"
echo -n '#define __BUILD_NUMBER_MINOR__ ' >> "$VERSIONFILE"
echo $BLDMINORVERSION >> "$VERSIONFILE"
echo -n '#define __BUILD_NUMBER_VARIANT__ ' >> "$VERSIONFILE"
echo $BLDVARIANTVERSION >> "$VERSIONFILE"

echo -n '#define __SVN_REVISION__ ' >> "$VERSIONFILE"
echo $SVNREVISION >> "$VERSIONFILE"
