#!/bin/sh
# This file generates a Windows batch file that can be then used by builders for cross-compiling without relying on Cygwin

export TARGET_ARCH=x86_64-unknown-linux-gnu
make clean
make -n > CrossCompile.bat

sed -i s@clang\+\+@\%LINUX_MULTIARCH_ROOT\%/\%LINUX_ARCH\%\/bin\/clang\+\+.exe@g CrossCompile.bat
sed -i s@-Wno-switch@-Wno-switch\ -target\ \%LINUX_ARCH\%\ --sysroot=\%LINUX_MULTIARCH_ROOT\%/\%LINUX_ARCH\%@g CrossCompile.bat
sed -i s@\\bar\\b@\%LINUX_MULTIARCH_ROOT\%/\%LINUX_ARCH\%\/bin\/%LINUX_ARCH\%-ar.exe@g CrossCompile.bat
