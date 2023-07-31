#!/bin/bash

set -e

# Specifies the version of opencv to download
opencv_version=4.5.5

# Comment the line below to exclude opencv_contrib from the build
use_opencv_contrib=""

opencv_url=https://github.com/opencv/opencv/archive/"$opencv_version".zip
opencv_src=opencv-"$opencv_version"

opencv_contrib_url=https://github.com/opencv/opencv_contrib/archive/"$opencv_version".zip
opencv_contrib_src=opencv_contrib-"$opencv_version"

# Create build directory
if [ ! -d ./build ]
then
  mkdir build
fi

pushd build

# Download opencv
if [ ! -d "$opencv_src" ]
 then
    if [ ! -f "$opencv_src".zip ]
     then
        echo Downloading "$opencv_url"...
        wget -O "$opencv_src".zip "$opencv_url"
    fi

    echo Extracting "$opencv_src".zip...
    unzip "$opencv_src".zip -d .
fi

# Add our module to the path
OPENCV_ROOT_DIRECTORY=`cd $(pwd)/../; pwd`
EXTRA_MODULES_PATH=`cd $(pwd)/../UnrealModules; pwd`

if [ ${use_opencv_contrib+x} ] 
then
	# Download opencv_contrib
	if [ ! -d "$opencv_contrib_src" ]
  then
		if [ ! -f "$opencv_contrib_src".zip ]
    then
			echo Downloading "$opencv_contrib_url"...
			wget -O "$opencv_contrib_src".zip "$opencv_contrib_url"
		fi
		echo Extracting "$opencv_contrib_src".zip...
		unzip "$opencv_contrib_src".zip -d .
	fi

	# Append it to the extra modules path for opencv to compile in
  CONTRIB_MODULE_PATH=`cd $(pwd)/"$opencv_contrib_src"/modules; pwd`
	EXTRA_MODULES_PATH+=";$CONTRIB_MODULE_PATH"
fi

echo Removing install directories
rm -rf ../include/opencv*
rm -rf ../lib64

echo Deleting existing build directories...
rm -rf x64
mkdir x64

pushd x64

ARCH_NAME=x86_64-unknown-linux-gnu
UE_ENGINE_LOCATION=`cd $(pwd)/../../../../../../../..; pwd`
UE_THIRD_PARTY_LOCATION="$UE_ENGINE_LOCATION/Source/ThirdParty/"

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v19_clang-11.0.1-centos7

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME/$ARCH_NAME"

C_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang"
CXX_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang++"

echo Configuring x64 build...

CXX_FLAGS="-nostdinc++ -fvisibility=hidden -fPIC -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1"
CXX_LINKER="-nodefaultlibs -nostdlib++ -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Linux/$ARCH_NAME/ $UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Linux/$ARCH_NAME/libc++.a $UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Linux/$ARCH_NAME/libc++abi.a -lm -lc -lgcc_s -lgcc"


CMAKE_ARGS=(
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_STANDARD=11
    -DCMAKE_INSTALL_PREFIX="$OPENCV_ROOT_DIRECTORY"
    -DOPENCV_EXTRA_MODULES_PATH="$EXTRA_MODULES_PATH"
    -DCMAKE_BUILD_TYPE=RELEASE
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_SHARED_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_MODULE_LINKER_FLAGS="$CXX_LINKER"
    -DZLIB_INCLUDE_DIR="$UE_THIRD_PARTY_LOCATION/zlib/v1.2.8/include/Unix/$ARCH_NAME"
    -DZLIB_LIBRARY="$UE_THIRD_PARTY_LOCATION/zlib/v1.2.8/lib/Unix/$ARCH_NAME"
)

NUM_CPU=`grep -c ^processor /proc/cpuinfo`

pwd


echo Configuring Release build for OpenCV version "$opencv_version"...
cmake3 "${CMAKE_ARGS[@]}" -C "$OPENCV_ROOT_DIRECTORY"/cmake_options.txt -S ../"$opencv_src" 

echo Building OpenCV for Release...
cmake3 --build . --config Release --target install -- -j$NUM_CPU

# x64/..
popd

echo Moving library to destination folders...

bin_path="$OPENCV_ROOT_DIRECTORY/../../../Binaries/ThirdParty"

echo bin_path is "$bin_path"

if [ ! -d "$bin_path/Linux" ]
then
  mkdir "$bin_path/Linux"
fi


find "$OPENCV_ROOT_DIRECTORY/lib64/" -name "libopencv_world.s*" -exec mv '{}' "$bin_path/Linux" \;

mv -f "$OPENCV_ROOT_DIRECTORY/include/opencv4/opencv2" "$OPENCV_ROOT_DIRECTORY/include/"

echo Cleaning up...

rm -rf ./x64

# build/..
popd

# Remove generated .cmake files
rm -rf OpenCV*.cmake

# Remove unused installed folders
rm -rf ./lib64
rm -rf ./include/opencv4
rm -rf ./bin
rm -rf ./share

echo Done. Remember to delete the build directory and submit changed files to p4

echo Done.