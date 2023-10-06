@REM @echo off

set TOOLCHAIN_VERSION=v22
set LLVM_VERSION=16.0.6
set LLVM_BRANCH=release/16.x
set LLVM_TAG=llvmorg-16.0.6

set CMAKE_BINARY=%CD%\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe
set PYTHON_BINARY=%CD%\..\..\..\..\Binaries\ThirdParty\Python\Win64\python.exe
set ZLIB_PATH=%CD%\..\..\..\..\..\Engine\Source\ThirdParty\zlib\v1.2.8
set "ZLIB_PATH=%ZLIB_PATH:\=/%"
set NSIS_BINARY=C:\Program Files (x86)\NSIS\Bin\makensis.exe
set PATCH_BINARY=C:\Program Files\Git\usr\bin\patch.exe
set VS_VERSION="Visual Studio 17 2022"

for %%i in (python.exe) do set PYTHON_BINARY="%%~$PATH:i"
for %%i in (cmake.exe) do set CMAKE_BINARY="%%~$PATH:i"

set FILENAME=%TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7

echo Building %FILENAME%.exe...

echo.
echo Using CMake: %CMAKE_BINARY%
echo Using Python: %PYTHON_BINARY%
echo Using VisualStudio: %VS_VERSION%

@REM We need to build in a directory with shorter path, so we avoid hitting path max limit.
set ROOT_DIR=%CD%

rmdir /S /Q %TEMP%\clang-build-%LLVM_VERSION%
mkdir %TEMP%\clang-build-%LLVM_VERSION%
pushd %TEMP%\clang-build-%LLVM_VERSION%


unzip -q -o %ROOT_DIR%\%FILENAME%-windows.zip -d OUTPUT

echo Cloning rpmalloc to speed up clang/lld runtime operation
git clone https://github.com/mjansson/rpmalloc rpmalloc --depth 1


echo Cloning LLVM (tag %LLVM_TAG% only)
rem clone -b can also accept tag names
git clone https://github.com/llvm/llvm-project source -b %LLVM_TAG% --single-branch --depth 1 -c advice.detachedHead=false
pushd source
git -c advice.detachedHead=false checkout tags/%LLVM_TAG% -b %LLVM_BRANCH%
popd

echo Applying patches
pushd "source"
rem set DRY_RUN=--dry-run
set DRY_RUN=
"%PATCH_BINARY%" %DRY_RUN% -p 1 -i %ROOT_DIR%\patches\clang\default-dwarf-4.patch 
"%PATCH_BINARY%" %DRY_RUN% -p 1 -i %ROOT_DIR%\patches\compiler-rt\manually-define-AT_HWCAP2.diff 
"%PATCH_BINARY%" %DRY_RUN% -p 1 -i %ROOT_DIR%\patches\llvm\disable-auto-upgrade-debug-info.patch 
popd


echo Building LLVM, clang, lld, bolt
mkdir build_all
pushd build_all
%CMAKE_BINARY% -G %VS_VERSION% -DLLVM_ENABLE_PROJECTS=llvm;clang;lld -DLLVM_INTEGRATED_CRT_ALLOC=../rpmalloc -DLLVM_USE_CRT_RELEASE=MT -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="/O2 /Ob3 /DNDEBUG /Zi /Gy" -DCMAKE_CXX_FLAGS_RELEASE="/O2 /Ob3 /DNDEBUG /Zi /Gy" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF" -DCMAKE_INSTALL_PREFIX="..\install" -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_TARGETS_TO_BUILD="AArch64;X86" -DLLVM_ENABLE_ZLIB=FORCE_ON -DZLIB_LIBRARY="%ZLIB_PATH%/lib/Win64-llvm/Release/zlibstatic.lib" -DZLIB_INCLUDE_DIR="%ZLIB_PATH%/include/Win64/VS2015" -DCLANG_REPOSITORY_STRING="github.com/llvm/llvm-project" "..\source\llvm"
%CMAKE_BINARY% --build . --target install --config Release
popd

for %%G in (aarch64-unknown-linux-gnueabi x86_64-unknown-linux-gnu) do (
    mkdir OUTPUT\%%G
    mkdir OUTPUT\%%G\bin
    mkdir OUTPUT\%%G\lib
    mkdir OUTPUT\%%G\lib\clang
    copy "install\bin\clang.exe" OUTPUT\%%G\bin
    copy "install\bin\clang++.exe" OUTPUT\%%G\bin
    copy "install\bin\ld.lld.exe" OUTPUT\%%G\bin
    copy "install\bin\lld.exe" OUTPUT\%%G\bin
    copy "install\bin\llvm-ar.exe" OUTPUT\%%G\bin
    copy "install\bin\llvm-profdata.exe" OUTPUT\%%G\bin
    copy "install\bin\llvm-symbolizer.exe" OUTPUT\%%G\bin
    copy "install\bin\llvm-objcopy.exe" OUTPUT\%%G\bin
    copy "install\bin\LTO.dll" OUTPUT\%%G\bin
    xcopy "install\lib\clang" OUTPUT\%%G\lib\clang /s /e /y
)

@REM Create version file
echo %TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7> OUTPUT\ToolchainVersion.txt

echo Packing final toolchain...

pushd OUTPUT
del /S /Q %ROOT_DIR%\%FILENAME%.zip
zip -r %ROOT_DIR%\%FILENAME%.zip *
popd

if exist "%NSIS_BINARY%" (
    echo Creating %FILENAME%.exe...
    copy %ROOT_DIR%\InstallerScript.nsi .
    "%NSIS_BINARY%" /V4 InstallerScript.nsi
    move %FILENAME%.exe %ROOT_DIR%
) else (
    echo Skipping installer creation, because makensis.exe was not found.
    echo Install Nullsoft.
)

popd

echo.
echo Done.
