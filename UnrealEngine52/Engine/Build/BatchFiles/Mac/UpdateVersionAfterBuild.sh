#!/bin/bash

# $1 is path to product directory (engine, program or project)
# $2 is the platform we are incrementing

PRODUCT_NAME=$(basename $1)
VERSION_FILE="$1/Build/$2/$PRODUCT_NAME.PackageVersionCounter"

VERSION="0.1"
# increment version in the counter file
if [ -f $VERSION_FILE ]; then
	VERSION=`cat $VERSION_FILE`
fi

# convert to array with . delimiter
VERSION_ARRAY=(${VERSION//./ })
# increment and write out
echo ${VERSION_ARRAY[0]}.$((VERSION_ARRAY[1]+1)) > "$VERSION_FILE"


MAC_VERSION="0.1"
VERSION_FILE="$1/Build/Mac/$PRODUCT_NAME.PackageVersionCounter"
if [ -f $VERSION_FILE ]; then
	MAC_VERSION=`cat $VERSION_FILE`
fi

IOS_VERSION="0.1"
VERSION_FILE="$1/Build/IOS/$PRODUCT_NAME.PackageVersionCounter"
if [ -f $VERSION_FILE ]; then
	IOS_VERSION=`cat $VERSION_FILE`
fi

TVOS_VERSION="0.1"
VERSION_FILE="$1/Build/$2TVOSPRODUCT_NAME.PackageVersionCounter"
if [ -f $VERSION_FILE ]; then
	TVOS_VERSION=`cat $VERSION_FILE`
fi

# now write out an xcconfig containing all platform build versions relative to the project
XCCONFIG_FILE="$1/Intermediate/Build/Versions.xcconfig"

echo UE_MAC_BUILD_VERSION = $MAC_VERSION > $XCCONFIG_FILE
echo UE_IOS_BUILD_VERSION = $IOS_VERSION >> $XCCONFIG_FILE
echo UE_TVOS_BUILD_VERSION = $TVOS_VERSION >> $XCCONFIG_FILE

