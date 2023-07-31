#!/bin/bash
set -e
if [ -z "$CEF_BRANCH" ]; then
    CEF_BRANCH=4577
fi
if [ -z "$GN_DEFINES" ]; then
    echo "Missing GN_DEFINES variable, make sure it was set via your Docker build args."
    exit 1
fi

wget https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py -O /code/automate/automate-git.py
CEF_USE_GN=1
echo "## Building CEF branch $CEF_BRANCH"
python /code/automate/automate-git.py --branch=$CEF_BRANCH --download-dir=/code --no-distrib --no-build --force-update

PATH=$PATH:/code/depot_tools
echo "### Applying local patches to build"
cd  /code/chromium/src/cef && git apply /code/patches/*.diff
echo "### Running cef_create_projects"
bash cef_create_projects.sh

cd /code/chromium/src
echo "### Starting debug build"
ninja -C out/Debug_GN_x64 cefsimple chrome_sandbox
echo "### Starting release build"
ninja -C out/Release_GN_x64 cefsimple chrome_sandbox

echo "### Packaging release"
cd /code/chromium/src/cef/tools
./make_distrib.sh --ninja-build --x64-build

echo "### Building CEF wrapper library"
cd /code/chromium/src/cef/
BINARY_DIST_DIR="`find /code/chromium/src/cef/binary_distrib -iname README.txt -exec dirname {} \;`"
mkdir -p wrapper_build/release
pushd wrapper_build/release
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF -DCMAKE_TOOLCHAIN_FILE=/code/Toolchain-clang.cmake  $BINARY_DIST_DIR
make -j 8 libcef_dll_wrapper
popd

mkdir -p wrapper_build/debug
pushd wrapper_build/debug
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DUSE_SANDBOX=OFF -DCMAKE_TOOLCHAIN_FILE=/code/Toolchain-clang.cmake  $BINARY_DIST_DIR
make -j 8 libcef_dll_wrapper
popd

mkdir -p $BINARY_DIST_DIR/build_release/libcef_dll
cp wrapper_build/release/libcef_dll_wrapper/libcef_dll_wrapper.a $BINARY_DIST_DIR/build_release/libcef_dll
mkdir -p $BINARY_DIST_DIR/build_debug/libcef_dll
cp wrapper_build/debug/libcef_dll_wrapper/libcef_dll_wrapper.a $BINARY_DIST_DIR/build_debug/libcef_dll

echo "###"
echo "### Build complete. Quit this shell to allow the build.bat file to copy the result locally."
echo "###"
echo "### You can also run \"docker cp cef3_build:/code/chromium/src/cef/binary_distrib/ .\" to extract the build locally"
echo "###"
echo "###"
