set CMAKE_BIN=%ANDROID_HOME%\cmake\3.10.2.4988404\bin

REM Build ARMv8
%CMAKE_BIN%\cmake.exe -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%CMAKE_BIN%\ninja.exe -G Ninja -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_ROOT%\build\cmake\android.toolchain.cmake -DANDROID_NATIVE_API_LEVEL=23 -H%CD%\include\ -Bbuild\arm64-v8a\

%CMAKE_BIN%\ninja.exe -C build\arm64-v8a\

copy build\arm64-v8a\libhwcpipe.so lib\arm64-v8a\libhwcpipe.so