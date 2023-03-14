#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

##
## Most of the following script is intended to be consistent for building all Mac
## third-party source. The sequence of steps are -
## 1) Set up constants, create temp dir, checkout files, save file info
## 2) lib-specific build steps
## 3) Check files were updated

ENABLE_CHECKOUT_FILES="1"

##
# Common library constants

# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..

# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=..

ZLIB_VERSION="v1.2.8"
ZLIB_BRANCH="${ZLIB_VERSION}"

OSSL_VERSION="1.1.1k"
OSSL_TAG="OpenSSL_1_1_1k"

LWS_VERSION="3.0.0"
LWS_BRANCH="${LWS_VERSION}"

PY_VERSION="3.7.7"

## TODO: Enable when/if needed for macOS
#LIBCURL_VERSION=curl-7_65_3
#LIBCURL_BRANCH="${LIBCURL_VERSION}"

pushd . > /dev/null 2>&1

SCRIPT_DIR="`dirname "${BASH_SOURCE[0]}"`"

source ${SCRIPT_DIR}/${DROP_TO_LIBROOT}/${DROP_TO_THIRDPARTY}/BuildScripts/Mac/Common/Common.sh

##
# Build zlib
#
# Note, zlib is built first, as a dependency for OpenSSL.
#
build_zlib()
{
	LIB_NAME="zlib"

	pushd "${SCRIPT_DIR}/${DROP_TO_LIBROOT}/${DROP_TO_THIRDPARTY}/${LIB_NAME}" > /dev/null 2>&1

	DEPLOYED_LIBS="${ZLIB_VERSION}/lib/Mac"
	DEPLOYED_INCS="${ZLIB_VERSION}/include/Mac"

	LIBFILES=( "${DEPLOYED_LIBS}/libz.a" "${DEPLOYED_LIBS}/libz.1.2.8.dylib" )
	INCFILES=( "${DEPLOYED_INCS}/zconf.h" "${DEPLOYED_INCS}/zlib.h" )

	SRCROOT="/tmp/${LIB_NAME}"
	DSTROOT="`pwd`"

	# Save these for later use (when building OpenSSL).
	ZLIB_LIB_ROOT="${DSTROOT}/${DEPLOYED_LIBS}"
	ZLIB_INC_ROOT="${DSTROOT}/${DEPLOYED_INCS}"

	if [ "${ENABLE_CHECKOUT_FILES}" == "1" ]; then
		checkoutFiles ${LIBFILES[@]} ${INCFILES[@]}
	fi
	saveFileStates ${LIBFILES[@]} ${INCFILES[@]}

	PREFIX_ROOT="${SRCROOT}/Deploy"

	echo "================================================================================"
	echo "Building ${LIB_NAME}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Common env.:"
	#echo "	  - DEPLOYED_LIBS: ${DEPLOYED_LIBS}"
	#echo "	  - DEPLOYED_INCS: ${DEPLOYED_INCS}"
	#echo "	  - LIBFILES     : ${LIBFILES[@]}"
	#echo "	  - INCFILES     : ${INCFILES[@]}"
	#echo "	  - SRCROOT      : ${SRCROOT}"
	#echo "	  - DSTROOT      : ${DSTROOT}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Function export env. vars.:"
	#echo "	  - ZLIB_LIB_ROOT: ${ZLIB_LIB_ROOT}"
	#echo "	  - ZLIB_INC_ROOT: ${ZLIB_INC_ROOT}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Local build env.:"
	#echo "	  - PREFIX_ROOT  : ${PREFIX_ROOT}"
	echo "================================================================================"

	rm -rf "${SRCROOT}"
	mkdir -p "${SRCROOT}"/{Deploy,Intermediate}

	pushd "${SRCROOT}" > /dev/null 2>&1

	git clone https://github.com/madler/zlib.git Source

	cd Source
	git checkout "${ZLIB_BRANCH}" -b "${ZLIB_BRANCH}"

	cd ../Intermediate
	cmake -G 'Unix Makefiles' \
		-DCMAKE_INSTALL_PREFIX:PATH="${PREFIX_ROOT}" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET="10.13" \
		-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
		"${SRCROOT}/Source"
	make -j$(get_core_count) && make install

	cd ..

	ditto Deploy/lib "${DSTROOT}/${DEPLOYED_LIBS}"
	ditto Deploy/include "${DSTROOT}/${DEPLOYED_INCS}"

	popd > /dev/null

	echo "================================================================================"
	echo "Checking built file status:"
	checkFilesWereUpdated ${LIBFILES[@]} ${INCFILES[@]}
	echo "================================================================================"
	checkFilesAreFatBinaries ${LIBFILES[@]}
	echo "================================================================================"
	echo "${LIB_NAME} was successfully built and updated."
	echo "================================================================================"
	echo ""

	popd > /dev/null
}

build_openssl()
{
	LIB_NAME="OpenSSL"

	pushd "${SCRIPT_DIR}/${DROP_TO_LIBROOT}/${DROP_TO_THIRDPARTY}/${LIB_NAME}" > /dev/null 2>&1

	DEPLOYED_LIBS="${OSSL_VERSION}/lib/Mac"
	DEPLOYED_INCS="${OSSL_VERSION}/include/Mac"

	LIBFILES=( "`find "${DEPLOYED_LIBS}" -type f -print0 | xargs -0 echo`" )
	INCFILES=( "`find "${DEPLOYED_INCS}" -type f -print0 | xargs -0 echo`" )

	SRCROOT="/tmp/${LIB_NAME}"
	DSTROOT="`pwd`"

	OSSL_ARCHS=( "x86_64" "arm64" )

	# Save these for later use (when building libcurl).
	OSSL_LIB_ROOT="${DSTROOT}/${DEPLOYED_LIBS}"
	OSSL_INC_ROOT="${DSTROOT}/${DEPLOYED_INCS}"

	if [ "${ENABLE_CHECKOUT_FILES}" == "1" ]; then
		checkoutFiles ${LIBFILES[@]} ${INCFILES[@]}
	fi
	saveFileStates ${LIBFILES[@]} ${INCFILES[@]}

	PREFIX_ROOT="${SRCROOT}/Deploy"

	echo "================================================================================"
	echo "Building ${LIB_NAME}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Common env.:"
	#echo "	  - DEPLOYED_LIBS: ${DEPLOYED_LIBS}"
	#echo "	  - DEPLOYED_INCS: ${DEPLOYED_INCS}"
	#echo "	  - LIBFILES     : ${LIBFILES[@]}"
	#echo "	  - INCFILES     : ${INCFILES[@]}"
	#echo "	  - SRCROOT      : ${SRCROOT}"
	#echo "	  - DSTROOT      : ${DSTROOT}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Imported env. vars.:"
	#echo "	  - ZLIB_LIB_ROOT: ${ZLIB_LIB_ROOT}"
	#echo "	  - ZLIB_INC_ROOT: ${ZLIB_INC_ROOT}"
	#echo "--------------------------------------------------------------------------------"
	#echo "	Local build env.:"
	#echo "	  - PREFIX_ROOT  : ${PREFIX_ROOT}"
	echo "================================================================================"

	rm -rf "${SRCROOT}"
	mkdir -p "${PREFIX_ROOT}"/Universal/{bin,lib}

	pushd "${SRCROOT}" > /dev/null 2>&1

	git clone https://github.com/openssl/openssl.git Source

	cd Source
	git checkout -b ${OSSL_TAG} tags/${OSSL_TAG}
	patch -p1 --no-backup-if-mismatch < "${DSTROOT}/Patches/darwin64-arm64-cc.patch"

	for OSSL_ARCH in "${OSSL_ARCHS[@]}"; do
		make clean > clean_${OSSL_ARCH}_log.txt 2>&1
		make distclean >> clean_${OSSL_ARCH}_log.txt 2>&1
		./Configure shared threads zlib \
			--with-zlib-lib="${ZLIB_LIB_ROOT}" \
			--with-zlib-include="${ZLIB_INC_ROOT}" \
			--prefix="${PREFIX_ROOT}/${OSSL_ARCH}" \
			--openssldir="${PREFIX_ROOT}/${OSSL_ARCH}" \
			darwin64-${OSSL_ARCH}-cc
		make -j$(get_core_count) > build_${OSSL_ARCH}_log.txt 2>&1
		make install > install_${OSSL_ARCH}_log.txt 2>&1
	done

	cd ../Deploy

	# All architectures will have the same built products, so just look at one of
	# them for the list.
	BINLIBS=$(for i in `cd "${PREFIX_ROOT}/${OSSL_ARCHS[0]}" && find {bin,lib} \( -type f -and \! -name ".DS_Store" -and \! -name "*.pc" -and \! -name "c_rehash" \)`; do echo $i; done)
	BINLNKS=$(for i in `cd "${PREFIX_ROOT}/${OSSL_ARCHS[0]}" && find {bin,lib} \( -type l \)`; do echo $i; done)

	cd Universal
	for i in ${BINLIBS}; do
		mkdir -p `dirname $i`
		lipo -create "${PREFIX_ROOT}/${OSSL_ARCHS[0]}/$i" "${PREFIX_ROOT}/${OSSL_ARCHS[1]}/$i" -output "$i"
	done
	for i in ${BINLNKS}; do
		cp -pPR "${PREFIX_ROOT}/${OSSL_ARCHS[0]}/$i" "$i"
	done

	cp -pPR "${PREFIX_ROOT}/Universal/lib/*.a" "${DSTROOT}/${DEPLOYED_LIBS}/"
	ditto "${PREFIX_ROOT}/${OSSL_ARCHS[0]}/include" "${DSTROOT}/${DEPLOYED_INCS}"

	popd > /dev/null

	echo "================================================================================"
	echo "Checking built file status:"
	checkFilesWereUpdated ${LIBFILES[@]} ${INCFILES[@]}
	echo "================================================================================"
	checkFilesAreFatBinaries ${LIBFILES[@]}
	echo "================================================================================"
	echo "${LIB_NAME} was successfully built and updated."
	echo "================================================================================"
	echo ""

	popd > /dev/null
}

build_libwebsockets()
{
	LIB_NAME="libWebSockets"
	LIB_NAME_LC="libwebsockets"

	pushd "${SCRIPT_DIR}/${DROP_TO_LIBROOT}/${DROP_TO_THIRDPARTY}/${LIB_NAME}" > /dev/null 2>&1

	DGB_LIBFILE_PATH="${LIB_NAME_LC}/lib/Mac/Debug/${LIB_NAME_LC}.a"
	REL_LIBFILE_PATH="${LIB_NAME_LC}/lib/Mac/Release/${LIB_NAME_LC}.a"

	LIBFILES=( "${DGB_LIBFILE_PATH}" "${REL_LIBFILE_PATH}" )
	INCFILES=( "${LIB_NAME_LC}/include/Mac/libwebsockets.h" "${LIB_NAME_LC}/include/Mac/lws_config.h" )

	if [ "${ENABLE_CHECKOUT_FILES}" == "1" ]; then
		checkoutFiles ${LIBFILES[@]}
		checkoutFiles ${INCFILES[@]}
	fi
	saveFileStates ${LIBFILES[@]} ${INCFILES[@]}

	echo "================================================================================"
	echo "Building ${LIB_NAME}"
	echo "================================================================================"

	SRCROOT="/tmp/${LIB_NAME}"
	DSTROOT="`pwd`"

	PREFIX_ROOT="${SRCROOT}/Deploy"

	mkdir -p "${PREFIX_ROOT}"

	pushd "${SRCROOT}" > /dev/null 2>&1

	cp "${DSTROOT}/libwebsockets/libwebsockets-${LWS_VERSION}.zip" ./
	unzip libwebsockets-${LWS_VERSION}.zip
	patch libwebsockets-${LWS_VERSION}/lib/core/private.h -i ${DSTROOT}/${LIB_NAME_LC}/NoMsgNoSignalRedefinition-v${LWS_VERSION}.patch
	cd libwebsockets-${LWS_VERSION}
	mkdir build-debug
	cd build-debug
	cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_C_FLAGS_DEBUG="-gdwarf-2" -DCMAKE_CXX_FLAGS_DEBUG="-gdwarf-2" -DCMAKE_INSTALL_PREFIX:PATH=${PREFIX_ROOT}/Debug -DOPENSSL_ROOT_DIR=/tmp/OpenSSL/Deploy/Universal -DOPENSSL_INCLUDE_DIR=/tmp/OpenSSL/Deploy/x86_64/include -DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE=/tmp/OpenSSL/Deploy/Universal -DZLIB_INCLUDE_DIR=/tmp/zlib/Deploy/include -DZLIB_LIBRARY_RELEASE=/tmp/zlib/Deploy/lib/libz.a -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
	make -j$(get_core_count) && make install
	cd ..
	mkdir build-release
	cd build-release
	cmake .. -DCMAKE_INSTALL_PREFIX:PATH=${PREFIX_ROOT}/Release -DOPENSSL_ROOT_DIR=/tmp/OpenSSL/Deploy/Universal -DOPENSSL_INCLUDE_DIR=/tmp/OpenSSL/Deploy/x86_64/include -DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE=/tmp/OpenSSL/Deploy/Universal -DZLIB_INCLUDE_DIR=/tmp/zlib/Deploy/include -DZLIB_LIBRARY_RELEASE=/tmp/zlib/Deploy/lib/libz.a -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
	make -j$(get_core_count) && make install

	popd > /dev/null

	cp -v ${PREFIX_ROOT}/Debug/lib/${LIB_NAME_LC}.a ${DGB_LIBFILE_PATH}
	cp -v ${PREFIX_ROOT}/Release/lib/${LIB_NAME_LC}.a ${REL_LIBFILE_PATH}
	cp -av ${PREFIX_ROOT}/Release/include/* "${LIB_NAME_LC}/include/Mac/"

	echo "================================================================================"
	echo "Checking built file status:"
	checkFilesWereUpdated ${LIBFILES[@]} ${INCFILES[@]}
	echo "================================================================================"
	checkFilesAreFatBinaries ${LIBFILES[@]}
	echo "================================================================================"
	echo "${LIB_NAME} was successfully built and updated."
	echo "================================================================================"
	echo ""

	popd > /dev/null
}

# This is here for reference:
build_libpython()
{
	LIB_NAME="Python3"

	pushd "${SCRIPT_DIR}/${DROP_TO_LIBROOT}/${DROP_TO_THIRDPARTY}/${LIB_NAME}" > /dev/null 2>&1

#	DGB_LIBFILE_PATH="${LIB_NAME_LC}/lib/Mac/Debug/${LIB_NAME_LC}.a"
#	REL_LIBFILE_PATH="${LIB_NAME_LC}/lib/Mac/Release/${LIB_NAME_LC}.a"

#	LIBFILES=( "${DGB_LIBFILE_PATH}" "${REL_LIBFILE_PATH}" )
#	INCFILES=( "${LIB_NAME_LC}/include/Mac/libwebsockets.h" "${LIB_NAME_LC}/include/Mac/lws_config.h" )

#	if [ "${ENABLE_CHECKOUT_FILES}" == "1" ]; then
#		checkoutFiles ${LIBFILES[@]}
#		checkoutFiles ${INCFILES[@]}
#	fi
#	saveFileStates ${LIBFILES[@]} ${INCFILES[@]}

	echo "================================================================================"
	echo "Building ${LIB_NAME}"
	echo "================================================================================"

	SRCROOT="/tmp/${LIB_NAME}"
	DSTROOT="`pwd`"

	PREFIX_ROOT="${SRCROOT}/Deploy"

	mkdir -p "${PREFIX_ROOT}"

	pushd "${SRCROOT}" > /dev/null 2>&1

	cp "${DSTROOT}/Python-${PY_VERSION}.tgz" ./
	tar -xzf Python-${PY_VERSION}.tgz
	#TODO: Apply patches here that are located at DSTROOT/Mac/Patches/Python-${PY_VERSION}/
	cd Python-${PY_VERSION}
	mkdir UEBuild
	cd UEBuild
	export MACOSX_DEPLOYMENT_TARGET=10.14
	../configure --prefix="${PREFIX_ROOT}" --enable-shared --with-openssl="/tmp/OpenSSL/Deploy/x86_64" CFLAGS="-isysroot `xcrun --sdk macosx --show-sdk-path` -mmacosx-version-min=10.14 -gdwarf-2 -I/tmp/zlib/Deploy/include" CPPFLAGS="-mmacosx-version-min=10.14 -gdwarf-2 -I/tmp/zlib/Deploy/include" LDFLAGS="/tmp/zlib/Deploy/lib/libz.a -mmacosx-version-min=10.14"
	make -j$(get_core_count) > UEBuildInstallLog.txt 2>&1
	make install >> UEBuildInstallLog.txt 2>&1

	popd > /dev/null

#	cp -v ${PREFIX_ROOT}/Debug/lib/${LIB_NAME_LC}.a ${DGB_LIBFILE_PATH}
#	cp -v ${PREFIX_ROOT}/Release/lib/${LIB_NAME_LC}.a ${REL_LIBFILE_PATH}
#	cp -av ${PREFIX_ROOT}/Release/include/* "${LIB_NAME_LC}/include/Mac/"

#	echo "================================================================================"
#	echo "Checking built file status:"
#	checkFilesWereUpdated ${LIBFILES[@]} ${INCFILES[@]}
#	echo "================================================================================"
#	checkFilesAreFatBinaries ${LIBFILES[@]}
#	echo "================================================================================"
#	echo "${LIB_NAME} was successfully built and updated."
#	echo "================================================================================"
#	echo ""

	popd > /dev/null
}
##
#TODO: Build libcurl as universal when/if needed for macOS
#build_libcurl()
#{
#}

##
#TODO: Build WebRTC as universal when/if needed for macOS
#build_webrtc()
#{
#}

build_zlib
build_openssl
#build_libwebsockets
#build_libpython

## TODO: Enable when/if needed for macOS
#build_libcurl
#build_webrtc

popd > /dev/null
