@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
set THIRDPARTY_ROOT=%ENGINE_ROOT%\Source\ThirdParty
set CMAKE_ADDITIONAL_ARGUMENTS=-DBUILD_SHARED_LIBS=OFF -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF -DBUILD_WITH_LIBCXX=ON
set CMAKE_OPENSSL_ARGUMENTS=-DOPENSSL_CRYPTO_LIBRARY=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\lib\Unix\x86_64-unknown-linux-gnu\libcrypto.a -DOPENSSL_SSL_LIBRARY=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\lib\Unix\x86_64-unknown-linux-gnu\libssl.a -DOPENSSL_INCLUDE_DIR=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\include\Unix\x86_64-unknown-linux-gnu -DOPENSSL_USE_STATIC_LIBS=ON
set CMAKE_ZLIB_ARGUMENTS=-DZLIB_LIBRARY=%THIRDPARTY_ROOT%\zlib\1.2.12\lib\Unix\x86_64-unknown-linux-gnu\Release\libz.a -DZLIB_INCLUDE_DIR=%THIRDPARTY_ROOT%\zlib\1.2.12\include
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu -TargetLib=libzip -TargetLibVersion=1.9.2 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS% %CMAKE_OPENSSL_ARGUMENTS% %CMAKE_ZLIB_ARGUMENTS%" -SkipCreateChangelist || exit /b

set CMAKE_OPENSSL_ARGUMENTS=-DBUILD_SHARED_LIBS=OFF -DOPENSSL_CRYPTO_LIBRARY=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\lib\Unix\aarch64-unknown-linux-gnueabi\libcrypto.a -DOPENSSL_SSL_LIBRARY=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\lib\Unix\aarch64-unknown-linux-gnueabi\libssl.a -DOPENSSL_INCLUDE_DIR=%THIRDPARTY_ROOT%\OpenSSL\1.1.1n\include\Unix\aarch64-unknown-linux-gnueabi -DOPENSSL_USE_STATIC_LIBS=ON
set CMAKE_ZLIB_ARGUMENTS=-DZLIB_LIBRARY=%THIRDPARTY_ROOT%\zlib\1.2.12\lib\Unix\aarch64-unknown-linux-gnueabi\Release\libz.a -DZLIB_INCLUDE_DIR=%THIRDPARTY_ROOT%\zlib\1.2.12\include
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=libzip -TargetLibVersion=1.9.2 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS% %CMAKE_OPENSSL_ARGUMENTS% %CMAKE_ZLIB_ARGUMENTS%" -SkipCreateChangelist || exit /b
