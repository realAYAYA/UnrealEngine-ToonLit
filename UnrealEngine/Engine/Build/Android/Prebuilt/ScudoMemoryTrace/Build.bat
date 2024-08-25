set CMAKEPATH=%ANDROID_HOME%\cmake\3.22.1\bin

call :build_arch arm64-v8a
call :build_arch x86_64

exit /b

:build_arch

mkdir .build_%1
pushd .build_%1

%CMAKEPATH%\cmake.exe -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Android -DCMAKE_SYSTEM_VERSION=28 -DCMAKE_ANDROID_ARCH_ABI=%1 -DCMAKE_ANDROID_STL_TYPE=c++_static -DCMAKE_MAKE_PROGRAM=%CMAKEPATH%\ninja.exe ..
%CMAKEPATH%\ninja.exe

popd

mkdir %1
move /y .build_%1\libScudoMemoryTrace.so %1
rmdir .build_%1 /s /q

exit /b
