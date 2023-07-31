You need msys and yasm to build libvpx on Windows.

Msys can be downloaded from here:
http://www.mingw.org/wiki/Getting_Started

and Yasm from here:
https://yasm.tortall.net/

You don't need to install any compiler, just base environment.

1) From the msys shell run GenerateVSSolution.sh which will generate Visual Studio 2015 solution (compatible with 2017) inside Temp\libvpx-x.x.x directory.
2) Copy downloaded Yasm binary to Temp\libvpx-x.x.x and rename it to "yasm.exe"
3) Finally, build vpx project in Release configuration from the solution, which will generate vpxmd.lib inside Temp\libvpx-x.x.x\x64\Release directory.
