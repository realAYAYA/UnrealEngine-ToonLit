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
SDL_DIR=${SCRIPT_DIR}/../SDL-gui-backend

BuildSDL2WithDocker()
{
	local Arch=$1
	local Image=$2
	local ImageName=temp_build_linux_sdl2
	local LibDir=${SDL_DIR}/lib/Unix/${Arch}

	echo Building ${Arch}...
	echo docker run -t --name ${ImageName} -v ${SCRIPT_DIR}/../../Vulkan:/Vulkan -v ${SDL_DIR}:/SDL-gui-backend -v ${SCRIPT_DIR}:/src ${Image} /src/docker-build-sdl2.sh
	docker run -t --name ${ImageName} -v ${SCRIPT_DIR}/../../Vulkan:/Vulkan -v ${SDL_DIR}:/SDL-gui-backend -v ${SCRIPT_DIR}:/src ${Image} /src/docker-build-sdl2.sh

	echo Copying files...
	mkdir -p ${LibDir}
	rm -rf ${LibDir}/libSDL2*.a

	docker cp ${ImageName}:/build/libSDL2_fPIC_Debug.a ${LibDir}/
	docker cp ${ImageName}:/build/libSDL2.a ${LibDir}/
	docker cp ${ImageName}:/build/libSDL2_fPIC.a ${LibDir}/

	echo Cleaning up...
	docker rm ${ImageName}
}

BuildSDL2WithDocker x86_64-unknown-linux-gnu      centos:7
BuildSDL2WithDocker aarch64-unknown-linux-gnueabi multiarch/centos:aarch64-clean

