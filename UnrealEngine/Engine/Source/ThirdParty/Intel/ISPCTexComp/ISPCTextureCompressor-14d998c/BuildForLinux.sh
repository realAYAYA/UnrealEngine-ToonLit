#!/bin/bash

set -eu
set -v

export UE_THIRD_PARTY_DIR="$(shell cd ../../.. && pwd)"

CC=${UE_SDKS_ROOT}/HostLinux/Linux_x64/v20_clang-13.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang

ISPC/linux/ispc -O2 --arch=x86-64 --target=sse2,avx --opt=fast-math --pic -o ispc_texcomp/kernel_astc_ispc.o -h ispc_texcomp/kernel_astc_ispc.h ispc_texcomp/kernel_astc.ispc
ISPC/linux/ispc -O2 --arch=x86-64 --target=sse2,avx --opt=fast-math --pic -o ispc_texcomp/kernel_ispc.o -h ispc_texcomp/kernel_ispc.h ispc_texcomp/kernel.ispc

${CC} -I${UE_THIRD_PARTY_DIR}/Unix/LibCxx/include/c++/v1 -std=c++11 -stdlib=libc++ -O2 -msse2 -fPIC -I. -c ispc_texcomp/ispc_texcomp_astc.cpp -o ispc_texcomp/ispc_texcomp_astc.o

${CC} -I${UE_THIRD_PARTY_DIR}/Unix/LibCxx/include/c++/v1 -std=c++11 -stdlib=libc++ -O2 -msse2 -fPIC -I. -c ispc_texcomp/ispc_texcomp.cpp -o ispc_texcomp/ispc_texcomp.o

mkdir -p build

${CC} -nodefaultlibs -std=c++11 -stdlib=libc++ -shared -rdynamic -o build/libispc_texcomp.so -L${UE_THIRD_PARTY_DIR}/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu ${UE_THIRD_PARTY_DIR}/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu/libc++abi.a ispc_texcomp/kernel_astc_ispc.o ispc_texcomp/kernel_astc_ispc_sse2.o ispc_texcomp/kernel_astc_ispc_avx.o ispc_texcomp/kernel_ispc.o ispc_texcomp/kernel_ispc_sse2.o ispc_texcomp/kernel_ispc_avx.o ispc_texcomp/ispc_texcomp_astc.o ispc_texcomp/ispc_texcomp.o
