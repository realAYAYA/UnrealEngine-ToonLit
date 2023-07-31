#!/bin/bash

# Blog post for setting up arm multiarch docker images:
#   Cross Building and Running Multi-Arch Docker Images
#   https://www.ecliptik.com/Cross-Building-and-Running-Multi-Arch-Docker-Images/
#
# TL;DR:
#   apt-get install qemu-user-static
#   docker run --rm --privileged multiarch/qemu-user-static:register
#
# To test docker images, run something like this:
#   docker run -v /epic:/epic -it --rm multiarch/centos:aarch64-clean /bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
EXPAT_DIR=$(readlink -f "${SCRIPT_DIR}/../expat-2.2.10")

BuildExpatWithDocker()
{
	local Arch=$1
	local Image=$2
	local ImageName=temp_build_linux_expat
	local LibDir=${EXPAT_DIR}/Unix/${Arch}

	echo Building ${Arch}...
	echo docker run -t --name ${ImageName} -v ${EXPAT_DIR}:/expat -v ${SCRIPT_DIR}:/src ${Image} /src/docker-build-expat.sh
	docker run -t --name ${ImageName} -v ${EXPAT_DIR}:/expat -v ${SCRIPT_DIR}:/src ${Image} /src/docker-build-expat.sh

	echo Copying files...
	mkdir -p ${LibDir}/{Debug,Release}
	rm -rf ${LibDir}/{Debug,Release}/libexpat.a

	docker cp ${ImageName}:/build/Debug/libexpat.a ${LibDir}/Debug
	docker cp ${ImageName}:/build/Release/libexpat.a ${LibDir}/Release

	echo Cleaning up...
	docker rm ${ImageName}
}

BuildExpatWithDocker x86_64-unknown-linux-gnu      centos:7
BuildExpatWithDocker aarch64-unknown-linux-gnueabi multiarch/centos:aarch64-clean

