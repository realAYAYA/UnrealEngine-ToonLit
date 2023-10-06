#!/bin/bash

set -x
set -eu

ToolChainVersion=v22
LLVM_VERSION_MAJOR=16
LLVM_VERSION=${LLVM_VERSION_MAJOR}.0.6
LLVM_BRANCH=release/${LLVM_VERSION_MAJOR}.x
LLVM_TAG=llvmorg-${LLVM_VERSION}

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

if [ ! -d "/src/build/crosstool-ng" ]; then
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
fi

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

echo "Cloning LLVM (tag $LLVM_TAG only)"
# clone -b can also accept tag names
git clone https://github.com/llvm/llvm-project llvm-src -b ${LLVM_TAG} --single-branch --depth 1 -c advice.detachedHead=false
pushd llvm-src
git -c advice.detachedHead=false checkout tags/${LLVM_TAG} -b ${LLVM_BRANCH}
popd


# this fixes an issue where AT_HWCAP2 is just not defined correctly in our sysroot. This is likely due to
# AT_HWCAP2 being around since glibc 2.18 offically, while we are still stuck on 2.17 glibc.
patch -d llvm-src -p 1 < /src/patches/compiler-rt/manually-define-AT_HWCAP2.diff

# this fixes lack of HWCAP_CRC32 in the old glibc (similar issue)
patch -d llvm-src -p 1 < /src/patches/compiler-rt/cpu_model_define_HWCAP_CRC32.diff

# move back to defaulting to dwarf 4, as if we leave to dwarf 5 libs built with dwarf5 will force everything to dwarf5
# even if you request dwarf 4. dwarf 5 currently causes issues with dump_syms and gdb/lldb earlier versions
patch -d llvm-src -p 1 < /src/patches/clang/default-dwarf-4.patch

# add a patch to disable auto-upgrade of debug info. It missed clang 16.x, so this patch shouldn't be needed for clang 17.x going forward
# See https://reviews.llvm.org/D143229 for context
patch -d llvm-src -p 1 < /src/patches/llvm/disable-auto-upgrade-debug-info.patch

# LLVM has just failed to support stand-alone LLD build, cheat by moving a required header into a location it can be found easily
# if you fulling include this you end up breaking other things. https://github.com/llvm/llvm-project/issues/48572
cp -rf llvm-src/libunwind/include/mach-o/ llvm-src/llvm/include

mkdir -p build-clang
pushd build-clang
	# CMake Error at cmake/modules/CheckCompilerVersion.cmake:40 (message):
	#   Host GCC version should be at least 5.1 because LLVM will soon use new C++
	#   features which your toolchain version doesn't support.  Your version is
	#   4.8.5.  You can temporarily opt out using
	#   LLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN, but very soon your toolchain won't be
	#   supported.
	cmake3 -G "Unix Makefiles" ../llvm-src/llvm \
		-DLLVM_ENABLE_PROJECTS=llvm\;clang\;lld\;compiler-rt \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_TERMINFO=OFF \
		-DLLVM_ENABLE_LIBXML2=OFF \
		-DLLVM_ENABLE_ZLIB=FORCE_ON \
		-DZLIB_LIBRARY="$ZLIB_PATH/lib/Unix/x86_64-unknown-linux-gnu/libz_fPIC.a" \
		-DZLIB_INCLUDE_DIR="$ZLIB_PATH/include/Unix/x86_64-unknown-linux-gnu" \
		-DLLVM_ENABLE_LIBCXX=1 \
		-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON \
		-DCMAKE_INSTALL_PREFIX=${InstallClangDir} \
		-DLLVM_INCLUDE_BENCHMARKS=OFF \
		-DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
		-DCLANG_REPOSITORY_STRING="github.com/llvm/llvm-project"

	make -j$CORES && make install
popd

# Copy files
for arch in $TARGETS; do
	echo "Copying ${arch} toolchain..."

	pushd ${OutputDirLinux}/$arch/
		chmod -R +w .

		if [ -d "$arch" ]; then
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
		fi
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

	mkdir -p ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION_MAJOR}/{lib,share,include}

	# copy share + include files (same as x86_64)
	cp -r ${OutputDirLinux}/x86_64-unknown-linux-gnu/lib/clang/${LLVM_VERSION_MAJOR}/share/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION_MAJOR}/share/
	cp -r ${OutputDirLinux}/x86_64-unknown-linux-gnu/lib/clang/${LLVM_VERSION_MAJOR}/include/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION_MAJOR}/include/

	mkdir -p build-rt-$arch
	pushd build-rt-$arch

		cmake3 -G "Unix Makefiles" ../llvm-src/compiler-rt \
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
			-DCOMPILER_RT_BUILD_ORC=OFF \
			-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
			-DCMAKE_INSTALL_PREFIX=../install-rt-$arch \
			-DSANITIZER_COMMON_LINK_FLAGS="-fuse-ld=lld" \
			-DSCUDO_LINK_FLAGS="-fuse-ld=lld" \
			-DLLVM_CONFIG_PATH=${InstallClangDir}/bin/llvm-config

		make -j$CORES && make install

	popd

	echo "Copying compiler rt..."
	cp -r install-rt-$arch/lib/* ${OutputDirLinux}/$arch/lib/clang/${LLVM_VERSION_MAJOR}/lib/
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
	tar czfvh /src/build/llvm-${LLVM_VERSION}-github-snapshot.src.tar.gz --hard-dereference /src/build/llvm-src
	cp /src/build/*.src.tar.gz build/src
	cp /src/*.{config,sh,nsi,bat} build/scripts

	# copy the toolchain in the directory named its version as per convention
	mkdir ${OutputDirLinux}/${ToolChainVersionName}
	cp -r x86_64-unknown-linux-gnu ${OutputDirLinux}/${ToolChainVersionName}
	cp -r build ${OutputDirLinux}/${ToolChainVersionName}
	cp -r aarch64-unknown-linux-gnueabi ${OutputDirLinux}/${ToolChainVersionName}
	cp -r ToolchainVersion.txt ${OutputDirLinux}/${ToolChainVersionName}

	# delete libraries in x86_64's lib folder or bundled binares with crash
	find ${OutputDirLinux}/${ToolChainVersionName}/x86_64-unknown-linux-gnu/lib/ -maxdepth 1 -type f -delete

	tar czfhv native-linux-${ToolChainVersionName}.tar.gz --hard-dereference ${ToolChainVersionName}
popd

# Pack Windows files
pushd ${OutputDirWindows}
	mkdir -p build/{src,scripts}
	cp /src/build/build-windows-x86_64-unknown-linux-gnu/.build/tarballs/* build/src
	cp /src/build/build-windows-aarch64-unknown-linux-gnueabi/.build/tarballs/* build/src
	zip -r /src/build/llvm-${LLVM_VERSION}-github-snapshot.src.zip /src/build/llvm-src
	cp /src/build/*.src.zip build/src
	cp /src/*.{config,sh,nsi,bat} build/scripts

	zip -r /src/build/${ToolChainVersionName}-windows.zip *
popd

echo done.
