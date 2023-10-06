#!/bin/bash

set -eu

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
SCRIPT_NAME=$(basename "$BASH_SOURCE")

# https://stackoverflow.com/questions/23513045/how-to-check-if-a-process-is-running-inside-docker-container
# https://unix.stackexchange.com/questions/607695/how-to-check-if-its-docker-or-host-machine-inside-bash-script
# cgroups 2 busted the first way, attempting a new way here but may break again in the future
if ! [ -f "/.dockerenv" ]; then

  ##############################################################################
  # host commands
  ##############################################################################

  ZLIB_PATH=../../../../../Engine/Source/ThirdParty/zlib/v1.2.8/

  # Need to static link zlib for being able to compress debug files
  cp -rpvf $ZLIB_PATH ./

  ImageName=build_linux_toolchain

  echo docker run -t --name ${ImageName} -v "${SCRIPT_DIR}:/src" centos:7 /src/${SCRIPT_NAME}
  docker run -t --name ${ImageName} -v "${SCRIPT_DIR}:/src" centos:7 /src/${SCRIPT_NAME}

  # Use if you want a shell when a command fails in the script
  # docker run -it --name ${ImageName} -v "${SCRIPT_DIR}:/src" centos:7 bash -c "/src/${SCRIPT_NAME}; bash"

  echo Removing ${ImageName}...
  docker rm ${ImageName}

else

  DOCKER_BUILD_DIR=/src/build

  if [ $UID -eq 0 ]; then
    ##############################################################################
    # docker root commands
    ##############################################################################
    yum install -y epel-release centos-release-scl dnf dnf-plugins-core

    # needed for mingw due to https://pagure.io/fesco/issue/2333
    dnf -y copr enable alonid/mingw-epel7

    yum install -y ncurses-devel patch llvm-toolset-7 llvm-toolset-7-llvm-devel make cmake3 tree zip \
        git wget which gcc-c++ gperf bison flex texinfo bzip2 help2man file unzip autoconf libtool \
        glibc-static libstdc++-devel libstdc++-static mingw64-gcc mingw64-gcc-c++ mingw64-winpthreads-static \
        devtoolset-7-gcc libisl-devel

    # Create non-privileged user and workspace
    adduser buildmaster
    mkdir -p ${DOCKER_BUILD_DIR}
    chown buildmaster:nobody -R ${DOCKER_BUILD_DIR}

    exec su buildmaster "$0"
  fi

  ##############################################################################
  # docker user level commands
  ##############################################################################
  cd ${DOCKER_BUILD_DIR}
  /src/build_linux_toolchain.sh

fi
