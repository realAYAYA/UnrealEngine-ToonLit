#!/bin/bash

set -x
set -eu

ToolChainVersion=v20
LLVM_VERSION=13.0.1
LLVM_URL=https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}
ZLIB_PATH=/src/v1.2.8

ToolChainVersionName="${ToolChainVersion}_clang-${LLVM_VERSION}-centos7"

TARGETS="x86_64-unknown-linux-gnu aarch64-unknown-linux-gnueabi"

OutputDirLinux=/src/build/OUTPUT-linux
OutputDirWindows=/src/build/OUTPUT-windows
InstallClangDir=/src/build/install-clang

# Default permissions
umask 0022

# Get num of cores
CORES=$(getconf _NPROCESSORS_ONLN)
echo Using $CORES cores for building

echo "check_certificate=off" > "$HOME/.wgetrc"
# Get crosstool-ng
git clone http://github.com/BrandonSchaefer/crosstool-ng -b 1.22

# Build crosstool-ng
pushd crosstool-ng
./bootstrap && ./configure --enable-local && make
popd

# Build linux toolchain to OUTPUT-linux
for arch in $TARGETS; do
	mkdir -p build-linux-$arch
	pushd build-linux-$arch
	cp /src/$arch.linux.config .config
	../crosstool-ng/ct-ng build.$CORES
	popd
done

# Build windows toolchain to OUTPUT-windows
for arch in $TARGETS; do
	mkdir -p build-windows-$arch
	pushd build-windows-$arch
	cp /src/$arch.windows.config .config
	../crosstool-ng/ct-ng build.$CORES
	popd
done

# since we are -u in the bash script and this ENV is not set it complains when source the devtoolset-7
export MANPATH=""

# cannot use this normal method as it creates its own bash, so lets just sorce the env
# scl enable devtoolset-7 bash
source /opt/rh/devtoolset-7/enable

# need to unset this or crosstools complains
unset LD_LIBRARY_PATH

#
# Linux
#

# Build clang
LLVM=llvm-${LLVM_VERSION}
CLANG=clang-${LLVM_VERSION}
UNWIND=libunwind-${LLVM_VERSION}
LLD=lld-${LLVM_VERSION}
COMPILER_RT=compiler-rt-${LLVM_VERSION}

wget ${LLVM_URL}/${LLVM}.src.tar.xz
wget ${LLVM_URL}/${CLANG}.src.tar.xz
wget ${LLVM_URL}/${UNWIND}.src.tar.xz
wget ${LLVM_URL}/${LLD}.src.tar.xz
wget ${LLVM_URL}/${COMPILER_RT}.src.tar.xz

mkdir -p llvm
tar -xf $LLVM.src.tar.xz --strip-components 1 -C llvm
mkdir -p llvm/tools/clang
tar -xf $CLANG.src.tar.xz --strip-components 1 -C llvm/tools/clang/
mkdir -p llvm/libunwind
tar -xf $UNWIND.src.tar.xz --strip-components 1 -C llvm/libunwind/
mkdir -p llvm/tools/lld
tar -xf $LLD.src.tar.xz --strip-components 1 -C llvm/tools/lld/
mkdir -p llvm/projects/compiler-rt
tar -xf $COMPILER_RT.src.tar.xz --strip-components 1 -C llvm/projects/compiler-rt/

# this fixes an issue where AT_HWCAP2 is just not defined correctly in our sysroot. This is likely due to
# AT_HWCAP2 being around since glibc 2.18 offically, while we are still stuck on 2.17 glibc.
patch -d llvm/projects/compiler-rt/ -p 1 < /src/patches/compiler-rt/manually-define-AT_HWCAP2.diff

# LLVM has just failed to support stand-alone LLD build, cheat by moving a required header into a location it can be found easily
# if you fulling include this you end up breaking other things. https://github.com/llvm/llvm-project/issues/48572
cp -rf llvm/libunwind/include/mach-o/ llvm/include

mkdir build-clang
pushd build-clang
	# CMake Error at cmake/modules/CheckCompilerVersion.cmake:40 (message):
	#   Host GCC version should be at least 5.1 because LLVM will soon use new C++
	#   features which your toolchain version doesn't support.  Your version is
	#   4.8.5.  You can temporarily opt out using
	#   LLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN, but very soon your toolchain won't be
	#   supported.
	cmake3 -G "Unix Makefiles" ../llvm \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_TERMINFO=OFF \
		-DLLVM_ENABLE_LIBXML2=OFF \
		-DLLVM_ENABLE_ZLIB=FORCE_ON \
		-DZLIB_LIBRARY="$ZLIB_PATH/lib/Unix/x86_64-unknown-linux-gnu/libz_fPIC.a" \
		-DZLIB_INCLUDE_DIR="$ZLIB_PATH/include/Unix/x86_64-unknown-linux-gnu" \
		-DLLVM_ENABLE_LIBCXX=1 \
		-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON \
		-DCMAKE_INSTALL_PREFIX=${InstallClangDir} \
		-DLLVM_TARGETS_TO_BUILD="AArch64;X86"

	make -j$CORES && make install
popd

# Copy files
for arch in $TARGETS; do
	echo "Copying ${arch} toolchain..."

	pushd ${OutputDirLinux}/$arch/
		chmod -R +w .

		# copy $arch/include/c++ to include/c++
		cp -r -L $arch/include .

		# copy usr lib64 and include dirs
		mkdir -p usr
		cp -r -L $arch/sysroot/usr/include usr
		cp -r -L $arch/sysroot/usr/lib64 usr
		cp -r -L $arch/sysroot/usr/lib usr

		cp -r -L $arch/lib64 .
		cp -r -L $arch/lib .

		[[ -f build.log.bz2 ]] && mv build.log.bz2 ../../build-linux-$arch.log.bz2

		rm -rf $arch
	popd

	echo "Copying clang..."
	cp -L ${InstallClangDir}/bin/clang           ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/clang++         ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/lld             ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/ld.lld          ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/llvm-ar         ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/llvm-profdata   ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/llvm-objcopy    ${OutputDirLinux}/$arch/bin/
	cp -L ${InstallClangDir}/bin/llvm-symbolizer ${OutputDirLinux}/$arch/bin/

	if [ "$arch" == "x86_64-unknown-linux-gnu" ]; then
		cp -r -L ${InstallClangDir}/lib/clang ${OutputDirLinux}/$arch/lib/
	fi
done

# Build compiler-rt
for arch in $TARGETS; do
	if [ "$arch" == "x86_64-unknown-linux-gnu" ]; then
		# We already built it with clang
		continue
	fi

	mkdir -p ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION}/{lib,share,include}

	# copy share + include files (same as x86_64)
	cp -r ${OutputDirLinux}/x86_64-unknown-linux-gnu/lib/clang/${LLVM_VERSION}/share/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION}/share/
	cp -r ${OutputDirLinux}/x86_64-unknown-linux-gnu/lib/clang/${LLVM_VERSION}/include/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION}/include/

	mkdir build-rt-$arch
	pushd build-rt-$arch

		cmake3 -G "Unix Makefiles" ../llvm/projects/compiler-rt \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_SYSTEM_NAME="Linux" \
			-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
			-DCMAKE_C_COMPILER_TARGET="$arch" \
			-DCMAKE_C_COMPILER=${InstallClangDir}/bin/clang \
			-DCMAKE_CXX_COMPILER=${InstallClangDir}/bin/clang++ \
			-DCMAKE_AR=${InstallClangDir}/bin/llvm-ar \
			-DCMAKE_NM=${InstallClangDir}/bin/llvm-nm \
			-DCMAKE_RANLIB=${InstallClangDir}/bin/llvm-ranlib \
			-DLLVM_ENABLE_ZLIB=FORCE_ON \
			-DZLIB_LIBRARY="$ZLIB_PATH/lib/Unix/x86_64-unknown-linux-gnu/libz_fPIC.a" \
			-DZLIB_INCLUDE_DIR="$ZLIB_PATH/include/Unix/x86_64-unknown-linux-gnu" \
			-DCMAKE_EXE_LINKER_FLAGS="--target=$arch -L${OutputDirLinux}/$arch/lib64 --sysroot=${OutputDirLinux}/$arch -fuse-ld=lld" \
			-DCMAKE_C_FLAGS="--target=$arch --sysroot=${OutputDirLinux}/$arch" \
			-DCMAKE_CXX_FLAGS="--target=$arch --sysroot=${OutputDirLinux}/$arch" \
			-DCMAKE_ASM_FLAGS="--target=$arch --sysroot=${OutputDirLinux}/$arch" \
			-DCMAKE_INSTALL_PREFIX=../install-rt-$arch \
			-DSANITIZER_COMMON_LINK_FLAGS="-fuse-ld=lld" \
			-DSCUDO_LINK_FLAGS="-fuse-ld=lld" \
			-DLLVM_CONFIG_PATH=${InstallClangDir}/bin/llvm-config

		make -j$CORES && make install

	popd

	echo "Copying compiler rt..."
	cp -r install-rt-$arch/lib/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION}/lib/
done

# Create version file
echo "${ToolChainVersionName}" > ${OutputDirLinux}/ToolchainVersion.txt

#
# Windows
#

for arch in $TARGETS; do
	echo "Copying Windows $arch toolchain..."

	pushd ${OutputDirWindows}/$arch/
		chmod -R +w .

		# copy $arch/include/c++ to include/c++
		cp -r -L $arch/include .

		# copy usr lib64 and include dirs
		mkdir -p usr
		cp -r -L $arch/sysroot/usr/include usr
		cp -r -L $arch/sysroot/usr/lib64 usr
		cp -r -L $arch/sysroot/usr/lib usr

		cp -r -L $arch/lib64 .
		cp -r -L $arch/lib .

		# Copy compiler-rt
		cp -r -L ${OutputDirLinux}/$arch/lib/clang lib/

		[[ -f build.log.bz2 ]] && mv build.log.bz2 ../../build-windows-$arch.log.bz2

		rm -rf $arch
	popd
done

# Pack Linux files
pushd ${OutputDirLinux}
	mkdir -p build/{src,scripts}
	cp /src/build/build-linux-x86_64-unknown-linux-gnu/.build/tarballs/* build/src
	cp /src/build/build-linux-aarch64-unknown-linux-gnueabi/.build/tarballs/* build/src
	cp /src/build/*.src.tar.xz build/src
	cp /src/*.{config,sh,nsi,bat} build/scripts

	echo tar czfhv /src/build/${ToolChainVersionName}.tar.gz --hard-dereference *
	tar czfhv /src/build/${ToolChainVersionName}.tar.gz --hard-dereference *
popd

# Pack Windows files
pushd ${OutputDirWindows}
	mkdir -p build/{src,scripts}
	cp /src/build/build-windows-x86_64-unknown-linux-gnu/.build/tarballs/* build/src
	cp /src/build/build-windows-aarch64-unknown-linux-gnueabi/.build/tarballs/* build/src
	cp /src/build/*.src.tar.xz build/src
	cp /src/*.{config,sh,nsi,bat} build/scripts

	zip -r /src/build/${ToolChainVersionName}-windows.zip *
popd

echo done.
