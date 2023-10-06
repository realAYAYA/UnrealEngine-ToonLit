#!/bin/bash

# Run via something like this:
#   script -c "./BUILD.EPIC.CentOS-7.sh && ./BUILD.EPIC.CentOS-7aarch64.sh" log.txt

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

set -x

# the following is needed for Ubuntu 16.04 and up
# - https://robinwinslow.uk/2016/06/23/fix-docker-networking-dns/
if [ $(uname -o) = "Msys" ]; then
DNS=""
else
DNS_IP4=`nmcli device list | grep IP4.DNS | head -1 | awk '{print $2}'`
DNS="--dns $DNS_IP4"
fi


if [ -z "${ARCH}" ]; then
	ARCH=
	IMAGE="centos:7"
fi

# UE4 supports CentOS 7.x Linux for some vendors
# this build script targets that specific platform.
# - will setup a build environment
# - and will run the [ ./BUILD.EPIC.sh ] build script (that's also used for
#   all of the other major platforms)
# this is primarily for automated builds on newer Linux distros...
#
# if you're just looking to build the libraries for your Linux distribution
# - just run the ./BUILD.EPIC.sh script by itself

# ------------------------------------------------------------
docker $DNS pull ${IMAGE}
# can test image with:
# docker $DNS run -it --rm --name test centos${ARCH}:7 bash

# ------------------------------------------------------------
docker $DNS build -t centos${ARCH}:7_buildtools -f Dockerfile.CentOS-7-build_tools${ARCH} .
# can test image with:
# docker $DNS run -it --rm --name test centos${ARCH}:7_buildtools bash

# ------------------------------------------------------------
docker $DNS build -t centos${ARCH}:7_z_ssl_curl_lws -f Dockerfile.CentOS-7-z_ssl_curl_websockets${ARCH} .
# can test image with:
# docker $DNS run -it --rm --name test centos${ARCH}:7_z_ssl_curl_lws bash


# ------------------------------------------------------------
# copy the compiled library files
docker $DNS run --name tmp_z_ssl_curl_lws${ARCH} centos${ARCH}:7_z_ssl_curl_lws
FOLDER=$(docker $DNS run --rm centos${ARCH}:7_z_ssl_curl_lws \
	find . -type d -name "INSTALL.*" -print); \
	for i in $FOLDER; do mkdir -p ./OUTPUT${ARCH}/$i && docker cp tmp_z_ssl_curl_lws${ARCH}:/home/work/$i/. ./OUTPUT${ARCH}/$i ; done
docker rm tmp_z_ssl_curl_lws${ARCH}

# ------------------------------------------------------------
# handy commands to know:

# docker ps -a    # all
# docker ps -aq
# docker ps -l    # latest
# docker rm <containerID>

# docker images
# docker rmi <image>

