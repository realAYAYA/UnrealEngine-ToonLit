#!/bin/bash

## Unreal Engine Container Build script for Third Party Libs
## Copyright Epic Games, Inc. All Rights Reserved.

## This script is expecting to exist in the UE4/Engine/Build/BatchFiles/Linux directory.  It will not work correctly
## if you copy it to a different location and run it.

# The purpose of this script is to re-build the third party libraries in either
# an Ubuntu 14.04 or CentOS 6 container/chroot. This is to maintain compatiblity with older glibc
# versions and system libraries.
#
# It has been tested only on CentOS 7 and Ubuntu 17.10 with the default firewall
# and currently only works for matching guest arches. If there are issues with
# downloading packages inside the container then this is likely a DNS or 
# firewall setup problem.
#
# It uses lxc 1 (lxc net and lxd do not seem available on CentOS 7).

echo
echo Running Container Build...
echo

# put ourselves into Source Root directory (two up from location of this script)
pushd "`dirname "$0"`/../../../../"

ARGS=$*
BUILD_ROOT="/mnt/build"
INIT_VERSION=1

### Install Container packages ###
InstallPackages()
{
	if [ "$EUID" -ne 0 ]; then
		echo "Currently unprivileged containers not supported please re-run as root or with sudo."
		echo "e.g. sudo ./ContainerBuildThridParty.sh $ARGS"
		exit 1
	fi

	source /etc/os-release

	# As this script is not generally used for running the editor it is not intended this section would be extended to support
	# multiple distros. If you are not using a debian or rhel based distro then you must install lxc manually.
	if [[ $ID_LIKE == *"debian"* ]] || [[ $ID == *"debian"* ]]; then
		echo "Installing LXC via apt."
		apt-get install -y lxc qemu-user-static net-tools
	elif [[ $ID_LIKE == *"fedora"* ]] || [[ $ID == *"fedora"* ]]; then
		echo "Installing LXC via yum."
		yum install -y lxc lxc-templates lxc-extra debootstrap libvirt bridge-utils perl qemu-user
		systemctl start libvirtd
	fi

	if ! which lxc-info 1>/dev/null; then
		echo "Unsupported build environment, please install LXC."; exit 1;
	fi
}

### Set config value
SetConfig()
{
	local PARAM=$1
	local VALUE=$2

	CONFIG_FILE=`GetContainerRoot $CONTAINER_NAME`/../config
	#echo "Config File: $CONFIG_FILE"

        if ! grep -l "$PARAM" $CONFIG_FILE 1>/dev/null; then
       		echo "$PARAM = $VALUE" | tee --append $CONFIG_FILE 1>/dev/null
        else
        	sed -i -e "s/$PARAM[[:blank:]]*=[[:blank:]]*[^\n]*/$PARAM = $VALUE/g" $CONFIG_FILE
        fi
}

### Set network config
SetNetworkConfigUbuntuGuest()
{
	local IF=`GetDefaultInterface`
	local ADDR=$1
	local GATEWAY=$2

	CONFIG_FILE=`GetContainerRoot $CONTAINER_NAME`/etc/network/interfaces
	echo "Network Interface File: $CONFIG_FILE"

	# This is currently specific to an ubuntu trusty guest
	sed -i -e "s/eth0 inet dhcp/eth0 inet static/g" $CONFIG_FILE
	echo " address $ADDR" | tee --append $CONFIG_FILE	
	echo " netmask 255.255.255.0" | tee --append $CONFIG_FILE	
	echo " gateway $GATEWAY" | tee --append $CONFIG_FILE	

	CONFIG_FILE=`GetContainerRoot $CONTAINER_NAME`/etc/resolvconf/resolv.conf.d/head
	echo "Resolv conf File: $CONFIG_FILE"

	for NAMESERVER in `nmcli device show $IF | grep IP4.DNS | awk '/ /{ print $2 }'`; do
		echo "nameserver $NAMESERVER" | tee --append $CONFIG_FILE
	done
}

### Set network config
SetNetworkConfigCentOSGuest()
{
	local IF=`GetDefaultInterface`
	local ADDR=$1
	local GATEWAY=$2

	CONFIG_FILE=`GetContainerRoot $CONTAINER_NAME`/etc/sysconfig/network-scripts/ifcfg-eth0
	echo "Network Interface File: $CONFIG_FILE"

	# This is currently specific to a centos 6 guest
	echo "DEVICE=eth0" | tee $CONFIG_FILE
	echo " TYPE=Ethernet" |tee --append $CONFIG_FILE
	echo " ONBOOT=yes" | tee --append $CONFIG_FILE
	echo " BOOTPROTO=none" | tee --append $CONFIG_FILE
	echo " IPADDR=$ADDR" | tee --append $CONFIG_FILE
	echo " PREFIX=24" | tee --append $CONFIG_FILE
	echo " GATEWAY=$GATEWAY" | tee --append $CONFIG_FILE	
	echo " IPV4_FAILURE_FATAL=yes" | tee --append $CONFIG_FILE
	echo " NAME=\"System eth0\"" | tee --append $CONFIG_FILE	

	CONFIG_FILE=`GetContainerRoot $CONTAINER_NAME`/etc/resolv.conf
	echo "Resolv conf File: $CONFIG_FILE"

	for NAMESERVER in `nmcli device show $IF | grep IP4.DNS | awk '/ /{ print $2 }'`; do
		echo "nameserver $NAMESERVER" | tee --append $CONFIG_FILE
	done
}

### Run command as user inside Container
RunCommand()
{
	local CONTAINER_NAME=$1
	local TARGET_ARCH=$2
	shift; shift

	lxc-attach -n $CONTAINER_NAME -a $TARGET_ARCH --clear-env --keep-var http_proxy --keep-var https_proxy --set-var CONTAINER_BUILD=1 --set-var TARGET_ARCH=$TARGET_ARCH -- su user -c "$*"
}

### Run root command inside Container
RunRootCommand()
{
	local CONTAINER_NAME=$1
	local TARGET_ARCH=$2
	shift; shift

	lxc-attach -n $CONTAINER_NAME -a $TARGET_ARCH --clear-env --keep-var http_proxy --keep-var https_proxy --set-var CONTAINER_BUILD=1 --set-var TARGET_ARCH=$TARGET_ARCH -- $*
}

### Get Container Root###
GetContainerRoot()
{
	local CONTAINER_NAME=$1
	local CONTAINER_ROOT=""

	CONTAINER_ROOT=`lxc-config lxc.lxcpath`/$CONTAINER_NAME/rootfs

	echo $CONTAINER_ROOT
}

### GetMajorVersion ###
GetMajorVersion()
{
	local VERSION=`lxc-info --version`
	local VERSION_SPLIT=(${VERSION//./ })
	echo "${VERSION_SPLIT[0]}"
}

### GetMinorVersion ###
GetMinorVersion()
{
	local VERSION=`lxc-info --version`
	local VERSION_SPLIT=(${VERSION//./ })
	echo "${VERSION_SPLIT[1]}"
}

### GetDefaultInterface ###
GetDefaultInterface()
{
	local DEFAULT=`route | grep -m 1 '^default'`
	local DEFAULT_SPLIT=($DEFAULT)
	local LENGTH=${#DEFAULT_SPLIT[@]}
	echo "${DEFAULT_SPLIT[$LENGTH-1]}"
}

### Init Network ###
InitNetwork()
{
	local IF=`GetDefaultInterface`
	local LINK_NAME=$1
	local IP_ADDR=$2
	IP_FORWARD=`cat /proc/sys/net/ipv4/ip_forward`

	if [ -z $IF ]; then
		echo "Cannot find default interface"; return
	fi
	
	brctl addbr $LINK_NAME
	ip addr add $IP_ADDR/24 dev $LINK_NAME
	ip link set up dev $LINK_NAME
	echo 1 | tee /proc/sys/net/ipv4/ip_forward > /dev/null

	iptables -t nat -A POSTROUTING -o $IF -j MASQUERADE
	iptables -A FORWARD -i $IF -o $LINK_NAME -j ACCEPT
	iptables -A FORWARD -i $LINK_NAME -o $IF -j ACCEPT

	# For centos 7
	if which firewall-cmd 1>/dev/null; then
		firewall-cmd --direct --add-rule ipv4 nat POSTROUTING 0 -o $IF -j MASQUERADE > /dev/null
		firewall-cmd --direct --add-rule ipv4 filter FORWARD 0 -i $IF -o $LINK_NAME -j ACCEPT > /dev/null
		firewall-cmd --direct --add-rule ipv4 filter FORWARD 0 -i $LINK_NAME -o $IF -j ACCEPT > /dev/null
	fi
}

### Cleanup Network ###
DestroyNetwork()
{
	local IF=`GetDefaultInterface`
	local LINK_NAME=$1
	local IP_ADDR=$2
	local IP_ADDR2=$3

	ip addr del $IP_ADDR/24 dev $LINK_NAME
	ip link set dev $LINK_NAME down
	brctl delbr $LINK_NAME
	echo $IP_FORWARD | tee /proc/sys/net/ipv4/ip_forward > /dev/null
	iptables -t nat -D POSTROUTING -o $IF -j MASQUERADE
	iptables -D FORWARD -i $IF -o $LINK_NAME -j ACCEPT
	iptables -D FORWARD -i $LINK_NAME -o $IF -j ACCEPT

	# For centos 7
	if which firewall-cmd 1>/dev/null; then
		firewall-cmd --direct --remove-rule ipv4 nat POSTROUTING 0 -o $IF -j MASQUERADE > /dev/null
		firewall-cmd --direct --remove-rule ipv4 filter FORWARD 0 -i $IF -o $LINK_NAME -j ACCEPT > /dev/null
		firewall-cmd --direct --remove-rule ipv4 filter FORWARD 0 -i $LINK_NAME -o $IF -j ACCEPT > /dev/null
	fi
}

### Init Container ###
InitContainer()
{
	local CONTAINER_NAME=$1
	local CONTAINER_ROOT=""
	local CONTAINER_DISTRO=$2
	local CONTAINER_VERSION=$3
	local TARGET_ARCH=$4
	shift; shift; shift; shift
	local INIT_COMMAND=$*

	if ! lxc-info -n $CONTAINER_NAME 1>/dev/null; then
		echo "Creating container"
		lxc-create -n $CONTAINER_NAME -t download -- -d $CONTAINER_DISTRO -r $CONTAINER_VERSION -a $TARGET_ARCH 

		# Setup lxc networking
		local NET_KEY="net.0"
		local ADDR_KEY=".address"
		if [ `GetMajorVersion` -eq "1" ]; then
			NET_KEY="network"
			ADDR_KEY=""
		fi
		SetConfig "lxc.$NET_KEY.type" veth
		SetConfig "lxc.$NET_KEY.flags" down 
		SetConfig "lxc.$NET_KEY.name" "eth0"
		SetConfig "lxc.$NET_KEY.link" ue4-lxc-br0 
		SetConfig "lxc.$NET_KEY.ipv4$ADDR_KEY" "172.16.255.2"
		SetConfig "lxc.$NET_KEY.ipv4.gateway" "172.16.255.1"

		if [ $CONTAINER_DISTRO == "ubuntu" ]; then
			SetNetworkConfigUbuntuGuest "172.16.255.2" "172.16.255.1"
		else
			SetNetworkConfigCentOSGuest "172.16.255.2" "172.16.255.1"
		fi

		local QEMU_ARCH=$TARGET_ARCH
		if [ $TARGET_ARCH == "armhf" ]; then
			QEMU_ARCH="arm"
		elif [ $TARGET_ARCH == "arm64" ]; then
			QEMU_ARCH="aarch64"
		fi
		cp /usr/bin/qemu-$QEMU_ARCH /var/lib/lxc/$CONTAINER_NAME/rootfs/usr/bin/qemu-$QEMU_ARCH-static 2>/dev/null
		cp /usr/bin/qemu-$QEMU_ARCH-static /var/lib/lxc/$CONTAINER_NAME/rootfs/usr/bin/qemu-$QEMU_ARCH-static 2>/dev/null
	else
		echo "Container exists"
	fi

	InitNetwork ue4-lxc-br0 172.16.255.1
	
	lxc-start -n $CONTAINER_NAME -d --logfile=$(pwd)/Engine/Build/BatchFiles/Linux/$CONTAINER_NAME.log

	CONTAINER_ROOT=`GetContainerRoot $CONTAINER_NAME`
	echo "Container Root: $CONTAINER_ROOT"

	MountSource $CONTAINER_ROOT
	echo "Running Container Init..."
	echo $INIT_COMMAND | tee $CONTAINER_ROOT/var/init.sh 1>/dev/null
	RunRootCommand $CONTAINER_NAME $TARGET_ARCH sh /var/init.sh 
	if [ -f $CONTAINER_ROOT/var/.uecontainerinit-$INIT_VERSION ]; then
		echo "Container Init Complete"
	else
		echo "Container Init Failed"
		CloseContainer $CONTAINER_NAME
		exit 1;
	fi

	# Uncomment this if you want to jump in to shell
	#lxc-attach -n $CONTAINER_NAME -- su user
}

### Close Container ###
CloseContainer()
{
	local CONTAINER_NAME=$1

	CONTAINER_ROOT=`GetContainerRoot $CONTAINER_NAME`

	UnmountSource $CONTAINER_ROOT

	lxc-stop -n $CONTAINER_NAME
	DestroyNetwork ue4-lxc-br0 172.16.255.1
}

### Mount Engine directory into Container ###
MountSource()
{
	local CONTAINER_ROOT=$1
	mkdir -p $CONTAINER_ROOT/$BUILD_ROOT/
	mount --bind `pwd` $CONTAINER_ROOT/$BUILD_ROOT

	# LXC with SELinux enabled needs this
	if [ -d /sys/fs/selinux ]; then
		mkdir -p $CONTAINER_ROOT/sys/fs/selinux
		mount --bind /sys/fs/selinux $CONTAINER_ROOT/sys/fs/selinux
	fi
}

### Unmount Engine directory ###
UnmountSource()
{
	local CONTAINER_ROOT=$1
	umount $CONTAINER_ROOT/$BUILD_ROOT

	if [ -d /sys/fs/selinux ]; then
		umount $CONTAINER_ROOT/sys/fs/selinux
	fi

	mount -o remount,rw /dev/pts
}

### Build Third Party libs inside a container for the specified arch ###
BuildThirdParty()
{
	local CONTAINER_ROOT=""
	local CONTAINER_NAME=""
	local CONTAINER_DISTRO=$1
	local CONTAINER_VERSION=$2
	local TARGET_ARCH=$3
	shift; shift; shift
	local INIT_COMMAND=$*

	CONTAINER_NAME=$CONTAINER_DISTRO-$CONTAINER_VERSION-$TARGET_ARCH
	
	InitContainer $CONTAINER_NAME $CONTAINER_DISTRO $CONTAINER_VERSION $TARGET_ARCH $INIT_COMMAND

	### Exec BuildThirdParty.sh script in Container ###
	RunCommand $CONTAINER_NAME $TARGET_ARCH $BUILD_ROOT/Engine/Build/BatchFiles/Linux/BuildThirdParty.sh $ARGS

	CloseContainer $CONTAINER_NAME
}

# CentOS 6 does not have Wayland included so this needs to be manually updated
WAYLAND_BUILD_SCRIPT="cd /tmp; wget https://wayland.freedesktop.org/releases/wayland-1.14.0.tar.xz 1>/dev/null; tar -xvf wayland-1.14.0.tar.xz 1>/dev/null; cd wayland-1.14.0; ./configure --disable-documentation 1>/dev/null; make 1>/dev/null; make install 1>/dev/null; cd ..; wget https://wayland.freedesktop.org/releases/wayland-protocols-1.12.tar.xz 1>/dev/null; tar -xvf wayland-protocols-1.12.tar.xz 1>/dev/null; cd wayland-protocols-1.12; ./configure 1>/dev/null; make 1>/dev/null; make install 1>/dev/null; cd ..; wget http://xkbcommon.org/download/libxkbcommon-0.8.0.tar.xz 1>/dev/null; tar -xvf libxkbcommon-0.8.0.tar.xz 1>/dev/null; cd libxkbcommon-0.8.0; ./configure --disable-x11 1>/dev/null; make 1>/dev/null; make install 1>/dev/null; cd ..; wget https://mesa.freedesktop.org/archive/mesa-17.2.4.tar.xz 1>/dev/null; tar -xvf mesa-17.2.4.tar.xz 1>/dev/null; cd mesa-17.2.4; export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:\$PKG_CONFIG_PATH; sed -i -e 's/2\.4\.\(75\|71\|66\)/2\.4\.65/g' configure.ac; sed -i -e 's/3\.9/3\.3/g' configure.ac; ./autogen.sh --with-platforms=wayland --disable-glx --disable-gbm --with-dri-drivers=swrast; cd src/egl/wayland/wayland-egl; make; make install; cd ..; wget https://www.khronos.org/registry/EGL/api/EGL/egl.h 1>/dev/null; wget https://www.khronos.org/registry/EGL/api/EGL/eglext.h 1>/dev/null; wget https://www.khronos.org/registry/EGL/api/EGL/eglplatform.h 1>/dev/null; mkdir -p /usr/local/include/EGL; cp -r egl*.h /usr/local/include/EGL"

# This is to add the current user to the container - does not take into account facl and other permission systems.
REAL_UID=`id -u $SUDO_USER`
REAL_GID=`id -g $SUDO_USER`
if [ $REAL_UID -ne 0 ]; then
	USER_INIT_COMMAND="set +e; USER_UID=\`id -u user 2>/dev/null\`; USER_GID=\`id -g user 2>/dev/null\`; GROUP_GID=\`getent group user | awk -F: '{print \$3}' 2>/dev/null\`; if [ -z \$USER_UID ]; then useradd -u $REAL_UID user 1>/dev/null; fi; if [ -z \$USER_GID ]; then groupadd -g $REAL_GID user; elif [ ! \$GROUP_GID == $REAL_GID ]; then groupmod -g $REAL_GID user 1>/dev/null; fi; if [ ! \$USER_UID == $REAL_UID ]; then usermod -u $REAL_UID; fi; if [ ! \$USER_GID == $REAL_GID ]; then usermod -g $REAL_GID user 1>/dev/null; fi; set -e";
else
	USER_INIT_COMMAND="echo stub"
fi

# Initial container setup for Ubuntu 14.04
UBUNTU_INIT_COMMAND="set -e; if [ -d /sys/fs/selinux ]; then mount -o remount,ro,bind /sys/fs/selinux; fi; if [ ! -f /var/.uecontainerinit-$INIT_VERSION ]; then apt-get install -y bison build-essential cmake git libasound2-dev libegl1-mesa-dev libexpat1-dev libffi-dev libgbm-dev libgles2-mesa-dev libpulse-dev libtool libxcursor-dev libxi-dev libxinerama-dev libxkbcommon-dev libxml2-dev libtool libxrandr-dev libxss-dev llvm llvm-dev mesa-common-dev pkg-config wget x11proto-dri3-dev x11proto-gl-dev x11proto-present-dev x11proto-scrnsaver-dev xutils-dev xz-utils 1>/dev/null; $WAYLAND_BUILD_SCRIPT; fi; touch /var/.uecontainerinit-$INIT_VERSION; $USER_INIT_COMMAND set +e;"

# Initial container setup for CentOS 6
CENTOS_INIT_COMMAND="set -e; if [ ! -f /var/.uecontainerinit-$INIT_VERSION ]; then yum install -y wget 1>/dev/null; cd /tmp; wget http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm 1>/dev/null; rpm -ivh --replacepkgs epel-release-6-8.noarch.rpm 1>/dev/null; yum install -y alsa-lib-devel autoconf bison bzip2 clang cmake coreutils elfutils-libelf-devel expat-devel file flex gcc-c++ gettext git glibc-static gperf help2man libffi-devel libtool libX11-devel libXcursor-devel libXext-devel libXft-devel libXi-devel libXinerama-devel libxml2-devel libXmu-devel libXpm-devel libXrandr-devel libXScrnSaver-devel libxshmfence-devel llvm llvm-devel mesa-libEGL-devel mesa-libGL-devel ncurses-devel patch pkgconfig pulseaudio-libs-devel tar texinfo which xorg-x11-proto-devel xorg-x11-util-macros xz zlib-devel; $WAYLAND_BUILD_SCRIPT; fi; touch /var/.uecontainerinit-$INIT_VERSION; $USER_INIT_COMMAND; set +e;"

# Initial container setup for CentOS 7
CENTOS7_INIT_COMMAND="set -e; if [ ! -f /var/.uecontainerinit-$INIT_VERSION ]; then yum install -y epel-release centos-release-scl; yum install -y alsa-lib-devel autoconf bison bzip2  clang cmake3 coreutils elfutils-libelf-devel expat-devel file flex gcc-c++ gettext git glibc-static gperf help2man libffi-devel libtool libX11-devel libXcursor-devel libXext-devel libXft-devel libXi-devel libXinerama-devel libxml2-devel libXmu-devel libXpm-devel libXrandr-devel libXScrnSaver-devel libxshmfence-devel llvm-toolset-7 llvm-toolset-7-llvm-devel make mesa-libEGL-devel mesa-libGL-devel ncurses-devel patch pkgconfig pulseaudio-libs-devel svn tar texinfo wget which xorg-x11-proto-devel xorg-x11-util-macros xz zlib-devel; fi; touch /var/.uecontainerinit-$INIT_VERSION; $USER_INIT_COMMAND; set +e;"

InstallPackages

# Default - only this builds toolchain
BuildThirdParty centos 6 amd64 $CENTOS_INIT_COMMAND

# Newer centos for building compiler-rt
# BuildThirdParty centos 7 amd64 $CENTOS7_INIT_COMMAND

# Others...
#BuildThirdParty centos 6 i386 $CENTOS_INIT_COMMAND
#BuildThirdParty ubuntu trusty arm64 $UBUNTU_INIT_COMMAND
#BuildThirdParty ubuntu trusty armhf $UBUNTU_INIT_COMMAND
