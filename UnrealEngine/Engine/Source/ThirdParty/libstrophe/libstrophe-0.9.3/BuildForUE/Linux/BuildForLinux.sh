#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

ARCH=x86_64-unknown-linux-gnu
#ARCH=aarch64-unknown-linux-gnueabi

# In ThirdParty directory:
#   docker run -it --name centos_x86_64  -v /$(pwd)/../../:/strophe //d/path/to/your/ue4/Engine/Source/ThirdParty:/ThirdParty centos:7 /bin/bash
#   docker run -it --name centos_aarch64 -v /$(pwd)/../../:/strophe //d/path/to/your/ue4/Engine/Source/ThirdParty:/ThirdParty multiarch/centos:aarch64-clean /bin/bash
#
# Install centos build tools:
#   yum install -y epel-release
#   yum install -y cmake3 make gcc-c++

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

for BUILD_CONFIG in Debug Release; do
	BUILD_DIR="${SCRIPT_DIR}/../../Linux/Build-${BUILD_CONFIG}"

	if [ -d "${BUILD_DIR}" ]; then
		rm -rf "${BUILD_DIR}"
	fi
	mkdir -pv "${BUILD_DIR}"

	pushd "${BUILD_DIR}"
	cmake3 -DSOCKET_IMPL=../../src/sock.c -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" -DDISABLE_TLS=0 -DOPENSSL_PATH=/ThirdParty/OpenSSL/1.1.1c/include/Linux/${ARCH} -DEXPAT_PATH=/ThirdParty/Expat/expat-2.2.10/lib -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" "${SCRIPT_DIR}/../../BuildForUE"

	make -j8

	OUTPUT_DIR="${SCRIPT_DIR}/../../Linux/${ARCH}/${BUILD_CONFIG}"
	mkdir -p "${OUTPUT_DIR}"
	mv "${SCRIPT_DIR}/../../Linux/libstrophe.a" "${OUTPUT_DIR}"

	popd

	rm -rf "${BUILD_DIR}"
	
done

# -fPIC version
for BUILD_CONFIG in Debug Release; do
	BUILD_DIR="${SCRIPT_DIR}/../../Linux/Build-${BUILD_CONFIG}"

	if [ -d "${BUILD_DIR}" ]; then
		rm -rf "${BUILD_DIR}"
	fi
	mkdir -pv "${BUILD_DIR}"

	pushd "${BUILD_DIR}"
	cmake3 -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true -DSOCKET_IMPL=../../src/sock.c -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" -DDISABLE_TLS=0 -DOPENSSL_PATH=/ThirdParty/OpenSSL/1.1.1c/include/Linux/${ARCH} -DEXPAT_PATH=/ThirdParty/Expat/expat-2.2.10/lib -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" "${SCRIPT_DIR}/../../BuildForUE"

	make -j8

	OUTPUT_DIR="${SCRIPT_DIR}/../../Linux/${ARCH}/${BUILD_CONFIG}"
	mkdir -p "${OUTPUT_DIR}"
	mv "${SCRIPT_DIR}/../../Linux/libstrophe.a" "${OUTPUT_DIR}/libstrophe_fPIC.a"

	popd

	rm -rf "${BUILD_DIR}"
	
done
