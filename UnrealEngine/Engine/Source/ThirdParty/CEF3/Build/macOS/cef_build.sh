#!/bin/bash
set -e
export GN_DEFINES="is_component_build=false enable_precompiled_headers=false is_official_build=true use_allocator=none symbol_level=1 use_thin_lto=false enable_remoting=false use_allocator_shim=false"

if [ -z "$CEF_BRANCH" ]; then
    CEF_BRANCH=4430
fi
if [ -z "$GN_DEFINES" ]; then
    echo "Missing GN_DEFINES variable, make sure it was set above"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "Missing valid cmake install."
    echo "Install it via https://cmake.org/install/ including the command line install (sudo \"/Applications/CMake.app/Contents/bin/cmake-gui\" --install)."
    exit 1
fi

if [ -z "$CEF_BUILD_DIR" ]; then
    CEF_BUILD_DIR=`pwd`/build/cef_$CEF_BRANCH
fi
mkdir -p $CEF_BUILD_DIR
CURRENT_DIR=`pwd`
# CEF 4430+ needs the 11.0 SDK as a min to build on macOS but the Chrome sync only expresses a need for 10.15
export FORCE_MAC_SDK_MIN="11.1"


# make the automate subfolder 
if [ "$1" != "skipsync" ] && [ "$1" != "skipbuild" ]; then
    mkdir -p $CEF_BUILD_DIR/automate/
    #wget https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py -O $CEF_BUILD_DIR/automate/automate-git.py
    curl https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py --output $CEF_BUILD_DIR/automate/automate-git.py
    export CEF_USE_GN=1
    echo "## Building CEF branch $CEF_BRANCH"
    python $CEF_BUILD_DIR/automate/automate-git.py --branch=$CEF_BRANCH --download-dir=$CEF_BUILD_DIR --no-distrib --no-build --force-update --x64-build
    rm $CEF_BUILD_DIR/chromium/src/cef/epic.patched || true
else
    if [ ! -d $CEF_BUILD_DIR/chromium/src/cef ]; then
        echo "Can't find CEF sync yet skipsync specified"
        exit 1
    fi
fi


PATH=$PATH:$CEF_BUILD_DIR/depot_tools
cd  $CEF_BUILD_DIR/chromium/src/cef

if [ "$1" != "skipbuild" ]; then
    if [ ! -f $CEF_BUILD_DIR/chromium/src/cef/epic.patched ]; then
        echo "### Applying local patches to build"
        git apply $CURRENT_DIR/patches/*.diff && touch $CEF_BUILD_DIR/chromium/src/cef/epic.patched
    fi

    echo "### Running cef_create_projects"
    bash cef_create_projects.sh

    cd $CEF_BUILD_DIR/chromium/src
    echo "### Starting debug build"
    ninja -C out/Debug_GN_x64 cef
    echo "### Starting release build"
    ninja -C out/Release_GN_x64 cef

    echo "### Packaging release"
    cd $CEF_BUILD_DIR/chromium/src/cef/tools
    ./make_distrib.sh --ninja-build --x64-build
fi

echo "### Building CEF wrapper library"
cd $CEF_BUILD_DIR/chromium/src/cef/
BINARY_DIST_DIR="`find $CEF_BUILD_DIR/chromium/src/cef/binary_distrib -iname README.txt -exec dirname {} \;`"
mkdir -p wrapper_build/release
pushd wrapper_build/release
cmake -G "Xcode" -DPROJECT_ARCH="x86_64" -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF $BINARY_DIST_DIR
xcodebuild -scheme libcef_dll_wrapper -configuration Release
popd

mkdir -p wrapper_build/debug
pushd wrapper_build/debug
cmake -G "Xcode" -DPROJECT_ARCH="x86_64" -DCMAKE_BUILD_TYPE=Debug -DUSE_SANDBOX=OFF $BINARY_DIST_DIR
xcodebuild -scheme libcef_dll_wrapper -configuration Debug
popd

mkdir -p $BINARY_DIST_DIR/build_release/libcef_dll
cp wrapper_build/release/libcef_dll_wrapper/Release/libcef_dll_wrapper.a $BINARY_DIST_DIR/Release
mkdir -p $BINARY_DIST_DIR/build_debug/libcef_dll
cp wrapper_build/debug/libcef_dll_wrapper/Debug/libcef_dll_wrapper.a $BINARY_DIST_DIR/Debug

echo "###"
echo "### Build complete."
echo "### Copy binaries from \"$BINARY_DIST_DIR\" to your Engine/Source/ThirdParty/CEF3 folder."
echo "###"
echo "###"
