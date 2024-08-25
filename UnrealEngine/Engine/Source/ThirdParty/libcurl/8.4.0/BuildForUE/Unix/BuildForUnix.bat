@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set /p TARGET_LIB_SOURCE_PATH= Enter source path of libCurl...
set TARGET_LIB_SOURCE_PATH=%TARGET_LIB_SOURCE_PATH:/=/%

set LIBCURL_VERSION=8.4.0
set OPENSSL_VERSION=1.1.1t
set ZLIB_VERSION=1.3
set NGHTTP2_VERSION=1.47.0

set ENGINE_ROOT=%CD:\=/%/../../../../../..
echo %ENGINE_ROOT%
set THIRDPARTY_ROOT=%ENGINE_ROOT%/Source/ThirdParty
set CMAKE_ADDITIONAL_ARGUMENTS=-DCMAKE_C_FLAGS="-DNGHTTP2_STATICLIB=1" -DUSE_NGHTTP2=ON -DCURL_USE_OPENSSL=ON -DBUILD_CURL_EXE=OFF -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF
set CMAKE_OPENSSL_ARGUMENTS=-DOPENSSL_CRYPTO_LIBRARY=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/lib/Unix/x86_64-unknown-linux-gnu/libcrypto.a -DOPENSSL_SSL_LIBRARY=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/lib/Unix/x86_64-unknown-linux-gnu/libssl.a -DOPENSSL_INCLUDE_DIR=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/include/Unix -DOPENSSL_USE_STATIC_LIBS=ON
set CMAKE_ZLIB_ARGUMENTS=-DZLIB_LIBRARY=%THIRDPARTY_ROOT%/zlib/%ZLIB_VERSION%/lib/Unix/x86_64-unknown-linux-gnu/Release/libz.a -DZLIB_INCLUDE_DIR=%THIRDPARTY_ROOT%/zlib/%ZLIB_VERSION%/include
set CMAKE_NGHTTP2_ARGUMENTS=-DNGHTTP2_LIBRARY=%THIRDPARTY_ROOT%/nghttp2/%NGHTTP2_VERSION%/lib/Unix/x86_64-unknown-linux-gnu/Release/libnghttp2.a -DNGHTTP2_INCLUDE_DIR=%THIRDPARTY_ROOT%/nghttp2/%NGHTTP2_VERSION%/include
call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu -TargetLib=libcurl -TargetLibVersion=%LIBCURL_VERSION% -TargetLibSourcePath=%TARGET_LIB_SOURCE_PATH% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS% %CMAKE_OPENSSL_ARGUMENTS% %CMAKE_ZLIB_ARGUMENTS% %CMAKE_NGHTTP2_ARGUMENTS%" -SkipCreateChangelist || exit /b

set CMAKE_OPENSSL_ARGUMENTS=-DOPENSSL_CRYPTO_LIBRARY=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/lib/Unix/aarch64-unknown-linux-gnueabi/libcrypto.a -DOPENSSL_SSL_LIBRARY=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/lib/Unix/aarch64-unknown-linux-gnueabi/libssl.a -DOPENSSL_INCLUDE_DIR=%THIRDPARTY_ROOT%/OpenSSL/%OPENSSL_VERSION%/include/Unix -DOPENSSL_USE_STATIC_LIBS=ON
set CMAKE_ZLIB_ARGUMENTS=-DZLIB_LIBRARY=%THIRDPARTY_ROOT%/zlib/%ZLIB_VERSION%/lib/Unix/aarch64-unknown-linux-gnueabi/Release/libz.a -DZLIB_INCLUDE_DIR=%THIRDPARTY_ROOT%/zlib/%ZLIB_VERSION%/include
set CMAKE_NGHTTP2_ARGUMENTS=-DNGHTTP2_LIBRARY=%THIRDPARTY_ROOT%/nghttp2/%NGHTTP2_VERSION%/lib/Unix/aarch64-unknown-linux-gnueabi/Release/libnghttp2.a -DNGHTTP2_INCLUDE_DIR=%THIRDPARTY_ROOT%/nghttp2/%NGHTTP2_VERSION%/include
call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=libcurl -TargetLibVersion=%LIBCURL_VERSION% -TargetLibSourcePath=%TARGET_LIB_SOURCE_PATH% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS% %CMAKE_OPENSSL_ARGUMENTS% %CMAKE_ZLIB_ARGUMENTS% %CMAKE_NGHTTP2_ARGUMENTS%" -SkipCreateChangelist || exit /b
