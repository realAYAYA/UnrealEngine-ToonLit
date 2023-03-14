REM get libwebsockets-1.7.3 from https://github.com/EpicGames/ThirdParty
REM put this file in libwebsockets-1.7.3/build
REM modify UEENGINEDIR to point to engine directory
REM run this batch file

@echo off

set UEENGINEDIR=D:\ue4_4.25\Engine
set UEOPENSSL=%UEENGINEDIR%\Source\ThirdParty\OpenSSL\1_0_1s
set UELIBWEBSOCKET=%UEENGINEDIR%\Source\ThirdParty\libWebSockets\libwebsockets
set UELIBWEBSOCKETINCLUDE=%UELIBWEBSOCKET%\include\Android
set UELIBWEBSOCKETLIB=%UELIBWEBSOCKET%\lib\Android

mkdir %UELIBWEBSOCKETINCLUDE%
mkdir %UELIBWEBSOCKETLIB%

set OPENSSL_LIBRARIES=%UEENGINEDIR%\Source\ThirdParty\libcurl\Android

rem NOTE: do not need difference includes for each architecture since identical config

rem ARMv7
mkdir tempwork
cd tempwork
%UEENGINEDIR%\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe ..\..  -DOPENSSL_INCLUDE_DIR=%UEOPENSSL%\include\Android -DLWS_OPENSSL_LIBRARIES=%OPENSSL_LIBRARIES% -DOPENSSL_CRYPTO_LIBRARY=%OPENSSL_LIBRARIES%\ARMv7\libcrypto.a -DOPENSSL_SSL_LIBRARY="%OPENSSL_LIBRARIES%\ARMv7libssl.a" -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=OFF -DCMAKE_TOOLCHAIN_FILE=%UEENGINEDIR%\Source\ThirdParty\PhysX\Externals\CMakeModules\Android\android.toolchain.cmake -G "MinGW Makefiles" -DTARGET_BUILD_PLATFORM=Android -DANDROID_NDK=%NDKROOT% -DCMAKE_MAKE_PROGRAM=%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe -DANDROID_NATIVE_API_LEVEL="android-19" -DANDROID_ABI="armeabi-v7a" -DANDROID_STL=libc++_shared
%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe
mkdir %UELIBWEBSOCKETLIB%\ARMv7
copy lib\libwebsockets.a %UELIBWEBSOCKETLIB%\ARMv7
copy ..\..\lib\libwebsockets.h %UELIBWEBSOCKETINCLUDE%
copy lws_config.h %UELIBWEBSOCKETINCLUDE%
cd ..
rmdir /S /Q tempwork

rem ARM64
mkdir tempwork
cd tempwork
%UEENGINEDIR%\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe ..\..  -DOPENSSL_INCLUDE_DIR=%UEOPENSSL%\include\Android -DLWS_OPENSSL_LIBRARIES=%OPENSSL_LIBRARIES% -DOPENSSL_CRYPTO_LIBRARY=%OPENSSL_LIBRARIES%\ARMv7\libcrypto.a -DOPENSSL_SSL_LIBRARY="%OPENSSL_LIBRARIES%\ARMv7libssl.a" -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=OFF -DCMAKE_TOOLCHAIN_FILE=%UEENGINEDIR%\Source\ThirdParty\PhysX\Externals\CMakeModules\Android\android.toolchain.cmake -G "MinGW Makefiles" -DTARGET_BUILD_PLATFORM=Android -DANDROID_NDK=%NDKROOT% -DCMAKE_MAKE_PROGRAM=%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe -DANDROID_NATIVE_API_LEVEL="android-21" -DANDROID_ABI="arm64-v8a" -DANDROID_STL=libc++_shared
%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe
mkdir %UELIBWEBSOCKETLIB%\ARM64
copy lib\libwebsockets.a %UELIBWEBSOCKETLIB%\ARM64
cd ..
rmdir /S /Q tempwork

rem x86
mkdir tempwork
cd tempwork
%UEENGINEDIR%\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe ..\..  -DOPENSSL_INCLUDE_DIR=%UEOPENSSL%\include\Android -DLWS_OPENSSL_LIBRARIES=%OPENSSL_LIBRARIES% -DOPENSSL_CRYPTO_LIBRARY=%OPENSSL_LIBRARIES%\ARMv7\libcrypto.a -DOPENSSL_SSL_LIBRARY="%OPENSSL_LIBRARIES%\ARMv7libssl.a" -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=OFF -DCMAKE_TOOLCHAIN_FILE=%UEENGINEDIR%\Source\ThirdParty\PhysX\Externals\CMakeModules\Android\android.toolchain.cmake -G "MinGW Makefiles" -DTARGET_BUILD_PLATFORM=Android -DANDROID_NDK=%NDKROOT% -DCMAKE_MAKE_PROGRAM=%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe -DANDROID_NATIVE_API_LEVEL="android-19" -DANDROID_ABI="x86" -DANDROID_STL=libc++_shared
%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe
mkdir %UELIBWEBSOCKETLIB%\x86
copy lib\libwebsockets.a %UELIBWEBSOCKETLIB%\x86
cd ..
rmdir /S /Q tempwork

rem x64
mkdir tempwork
cd tempwork
%UEENGINEDIR%\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe ..\..  -DOPENSSL_INCLUDE_DIR=%UEOPENSSL%\include\Android -DLWS_OPENSSL_LIBRARIES=%OPENSSL_LIBRARIES% -DOPENSSL_CRYPTO_LIBRARY=%OPENSSL_LIBRARIES%\ARMv7\libcrypto.a -DOPENSSL_SSL_LIBRARY="%OPENSSL_LIBRARIES%\ARMv7libssl.a" -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITH_SHARED=OFF -DCMAKE_TOOLCHAIN_FILE=%UEENGINEDIR%\Source\ThirdParty\PhysX\Externals\CMakeModules\Android\android.toolchain.cmake -G "MinGW Makefiles" -DTARGET_BUILD_PLATFORM=Android -DANDROID_NDK=%NDKROOT% -DCMAKE_MAKE_PROGRAM=%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe -DANDROID_NATIVE_API_LEVEL="android-21" -DANDROID_ABI="x86_64" -DANDROID_STL=libc++_shared
%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe
mkdir %UELIBWEBSOCKETLIB%\x64
copy lib\libwebsockets.a %UELIBWEBSOCKETLIB%\x64
cd ..
rmdir /S /Q tempwork
