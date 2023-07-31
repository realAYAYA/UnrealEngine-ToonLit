#!/bin/bash
# Copyright Epic Games, Inc. All rights reserved.

LIB_NAME="PLCrashReporter"

DROP_TO_PL_ROOT=../..
DROP_TO_LIBROOT=..
DROP_TO_THIRDPARTY=.


################################################################################
# Set up script env.
#

echo 🔵 Building PLCrashReporter for mac
pushd . > /dev/null

[[ $0 = /* ]] && SCRIPT_PATH="$0" || SCRIPT_PATH="$PWD/${0#./}"
SCRIPT_DIR="`dirname "${SCRIPT_PATH}"`"
PL_ROOT_DIR=${SCRIPT_DIR}/${DROP_TO_PL_ROOT}
CLCRASH_GIT_ROOT=${PL_ROOT_DIR}/PLCrashReporter


FULL_XCODE_VERSION=$(xcodebuild -version | grep Xcode)
XCODE_PREFIX="Xcode"
XCODE_VERSION=${FULL_XCODE_VERSION#"$XCODE_PREFIX"}
XCODE_VERSION_TRIMMED=$(echo $XCODE_VERSION | xargs)


LIB_OUTPUT_DIR=${PL_ROOT_DIR}/lib/lib-Xcode-$XCODE_VERSION_TRIMMED

LIBFILES=(
	"${LIB_OUTPUT_DIR}/Mac/Release/libCrashReporter.a"
	"${LIB_OUTPUT_DIR}/Mac/Debug/libCrashReporter.a"
)

cd ${PL_ROOT_DIR}/${DROP_TO_LIBROOT}
LIB_ROOT_DIR=${PWD}

echo Changed to ${LIB_ROOT_DIR}

source ${DROP_TO_THIRDPARTY}/BuildScripts/Mac/Common/Common.sh

################################################################################
# Set up build env.
#

TMPDIR=${PL_ROOT_DIR}/_generated
ISYSROOT=$(xcrun --sdk macosx --show-sdk-path)
ARCHFLAGS="-arch x86_64 -arch arm64"
PREFIXDIR=${TMPDIR}/Deploy

mkdir -p ${PREFIXDIR} > /dev/null 2>&1
mkdir -p ${LIB_OUTPUT_DIR} > /dev/null 2>&1

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

################################################################################
# Checkout the library list and save their state
#

# checkoutFiles ${LIBFILES[@]}
# saveFileStates ${LIBFILES[@]}

################################################################################
# Build the protobuf-c dependency, if requested.
#

if [[ -z "${DONT_BUILD_PROTOBUF_C_DEPEND}" ]]; then

	echo 🔵 Building protobuf-c dependency

	################################################################################
	# Remove previously generated files
	#

	echo 🔵 Remove previously generated files
	rm -rf ${PL_ROOT_DIR}/_generated > /dev/null
	mkdir -p ${PREFIXDIR} > /dev/null 2>&1


	PROTOBUFLIBDEPS=(
		"lib/Mac/Debug/libprotobuf-c.a"
		"lib/Mac/Release/libprotobuf-c.a"
	)

	PROTOBUFDEPS=(
		"Dependencies/protobuf/include/protobuf-c/protobuf-c.h"
	)
	# checkoutFiles ${PROTOBUFDEPS[@]}
	# saveFileStates ${PROTOBUFDEPS[@]}

	pushd ${TMPDIR} > /dev/null
	
	# Pull down and build the protobuf and protobuf-c repositories

	echo 🔵 Cloning protobuf and protobuf-c repositories

	git clone https://github.com/protocolbuffers/protobuf.git
	git clone https://github.com/protobuf-c/protobuf-c.git

	############################################################################
	# Build protobuf (as a dependency for protobuf-c)
	#

	echo 🔵 Build protobuf \(as a dependency for protobuf-c\)

	pushd protobuf > /dev/null

	# Use a known good release/tag
	git checkout tags/v3.13.0 -b v3.13.0

	# Initialize the submodules
	git submodule update --init --recursive

	# General the build config
	./autogen.sh
	./configure --prefix=${PREFIXDIR} "CFLAGS=${ARCHFLAGS} -isysroot ${ISYSROOT}" "CXXFLAGS=${ARCHFLAGS} -isysroot ${ISYSROOT}" "LDFLAGS=${ARCHFLAGS}"

	# Build and install protobuf
	make -j`sysctl -n hw.ncpu`
	make install

	popd > /dev/null

	############################################################################
	# Build protobuf-c
	#

	echo 🔵 Build protobuf-c

	export PROTOC=${TMPDIR}/Deploy/bin/protoc
	
	pushd protobuf-c > /dev/null

	# Use a known good release/tag (that is compatible with the protobuf version)
	git checkout tags/v1.3.3 -b v1.3.3

	# General the build config
	./autogen.sh
	./configure --prefix=${PREFIXDIR} "CFLAGS=${ARCHFLAGS} -isysroot ${ISYSROOT}" "CXXFLAGS=${ARCHFLAGS} -isysroot ${ISYSROOT}" "LDFLAGS=${ARCHFLAGS}" "protobuf_CFLAGS=-I${PREFIXDIR}/include" "protobuf_LIBS=-L${PREFIXDIR}/lib -lprotobuf" "PKG_CONFIG_PATH=$PKG_CONFIG_PATH:${PREFIXDIR}/lib/pkgconfig"

	# Build and install protobuf-c
	make -j`sysctl -n hw.ncpu`
	make install

	popd > /dev/null

	# Return to LIB_ROOT_DIR
	popd > /dev/null

fi
	############################################################################
	# Update Dependencies for the Xcode project
	#
	# echo 🔵 Update Dependencies for the Xcode project

	# pushd ${SCRIPT_DIR}  > /dev/null

	# pwd
	
	# mkdir -p plcrashreporter/Dependencies/protobuf/bin/protoc-c > /dev/null 2>&1
	# cp -a ${PREFIXDIR}/bin/protoc-gen-c ${TMPDIR}/_generated/Dependencies/protobuf/bin/protoc-c

	# mkdir -p plcrashreporter/Dependencies/protobuf/include > /dev/null 2>&1
	# cp -a ${PREFIXDIR}/include/protobuf-c ${TMPDIR}/_generated/Dependencies/protobuf/include/

	# mkdir -p plcrashreporter/lib/Mac/Release > /dev/null 2>&1
	# cp -a ${PREFIXDIR}/lib/libprotobuf-c.a ${TMPDIR}/_generated/Dependencies/lib/Mac/Release

	# mkdir -p plcrashreporter/lib/Mac/Debug > /dev/null 2>&1
	# cp -a ${PREFIXDIR}/lib/libprotobuf-c.a ${TMPDIR}/_generated/lib/Mac/Debug

	# popd > /dev/null

	# pushd ${CLCRASH_GIT_ROOT}  > /dev/null

	# # checkFilesWereUpdated ${PROTOBUFDEPS[@]}
	# checkFilesAreFatBinaries ${PROTOBUFLIBDEPS[0]}

	# popd > /dev/null

	# echo The protobuf-c dependency for ${LIB_NAME} was rebuilt

# fi

############################################################################
# Update the generated protobuffer source files
#

echo 🔵 Update the generated protobuffer source files

PROTO_SOURCE_DIR=${PL_ROOT_DIR}/PLCrashReporter/Source
${PL_ROOT_DIR}/_generated/Deploy/bin/protoc-c PLCrashReport.proto --proto_path=${PROTO_SOURCE_DIR} --c_out=${PROTO_SOURCE_DIR}

################################################################################
# Build the PLCrashReporter Libraries
pushd ${CLCRASH_GIT_ROOT}  > /dev/null

mkdir -p ${LIB_OUTPUT_DIR}/Mac/Debug > /dev/null 2>&1

xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter macOS" -configuration Debug clean
xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter macOS" -configuration Debug
cp ${CLCRASH_GIT_ROOT}/build/Debug-macosx/*.a ${LIB_OUTPUT_DIR}/Mac/Debug

mkdir -p ${LIB_OUTPUT_DIR}/Mac/Release > /dev/null 2>&1

xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter macOS" -configuration Release clean
xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter macOS" -configuration Release

cp ${CLCRASH_GIT_ROOT}/build/Release-macosx/*.a ${LIB_OUTPUT_DIR}/Mac/Release

mkdir -p ${LIB_OUTPUT_DIR}/iOS/Debug > /dev/null 2>&1

xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter iOS" -configuration Debug clean
xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter iOS" -configuration Debug
cp ${CLCRASH_GIT_ROOT}/build/Debug-iphoneos/*.a ${LIB_OUTPUT_DIR}/iOS/Debug

mkdir -p ${LIB_OUTPUT_DIR}/iOS/Release > /dev/null 2>&1

xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter iOS" -configuration Release clean
xcodebuild -project CrashReporter.xcodeproj -target "CrashReporter iOS" -configuration Release

cp ${CLCRASH_GIT_ROOT}/build/Release-iphoneos/*.a ${LIB_OUTPUT_DIR}/iOS/Release


popd > /dev/null

################################################################################
# Check that the files were all touched
#

pushd ${PL_ROOT_DIR}  > /dev/null

# checkFilesWereUpdated ${LIBFILES[@]}
checkFilesAreFatBinaries ${LIBFILES[@]}

popd > /dev/null

################################################################################
popd > /dev/null
