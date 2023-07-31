SET LINUX_MULTIARCH_ROOT=%UE_SDKS_ROOT%\HostWin64\Linux_x64\v9_clang-4.0.0-centos7

rem SET LINUX_ARCH=arm-unknown-linux-gnueabihf
rem call CrossCompile.bat

SET LINUX_ARCH=x86_64-unknown-linux-gnu
call CrossCompile.bat