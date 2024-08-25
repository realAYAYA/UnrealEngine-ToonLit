To build a Linux library, use a more modern version than you will get by default on CentOS 7.
You should obtain and build a toolchain before. 

Script what can do it you can find in Engine/Build/BatchFiles/Linux/SetupToolchain.sh. 
You can set up the environment by running script Engine/Build/BatchFiles/Linux/Setup.sh.

An easier way to build is to use script build-libvpx-linux.sh in a dedicated Linux machine.
if you want use Epic toolchain, you can set UE_ROOT_DIR environment variablr to path with toolchain or 
build the library in thr docker container.

If you want to build in docker, you need get Epic toolchain and use build-libvpx-for-linux-in-docker.sh script
Minimum requered folders to build:
/* - Root files
/Engine/Build
/Engine/Source/ThirdParty/LibCxx

Need install Linux toolchain by run script "Engine/Build/BatchFiles/Linux/SetupToolchain.sh" from shell (Git Bash, etc.)

It will install toolchain /Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/

Script build-libvpx-for-linux-in-docker.sh and file centos7_build_libvpx.dockerfile can be used to build the library using Epic toolchain.
Use comamnd line option -t fot it. Another options can see by run the script with -h option.
