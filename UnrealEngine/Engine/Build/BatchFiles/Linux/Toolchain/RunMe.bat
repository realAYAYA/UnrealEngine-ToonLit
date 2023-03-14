@REM @echo off

set TOOLCHAIN_VERSION=v20
set LLVM_VERSION=13.0.1

set SVN_BINARY=%CD%\..\..\..\..\Binaries\ThirdParty\svn\Win64\svn.exe
set CMAKE_BINARY=%CD%\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe
set PYTHON_BINARY=%CD%\..\..\..\..\Binaries\ThirdParty\Python\Win64\python.exe
set NSIS_BINARY=C:\Program Files (x86)\NSIS\Bin\makensis.exe

for %%i in (python.exe) do set PYTHON_BINARY="%%~$PATH:i"
for %%i in (cmake.exe) do set CMAKE_BINARY="%%~$PATH:i"
for %%i in (svn.exe) do set SVN_BINARY="%%~$PATH:i"

set FILENAME=%TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7

echo Building %FILENAME%.exe...

echo.
echo Using SVN: %SVN_BINARY%
echo Using CMake: %CMAKE_BINARY%
echo Using Python: %PYTHON_BINARY%

@REM We need to build in a directory with shorter path, so we avoid hitting path max limit.
set ROOT_DIR=%CD%

rm -rf %TEMP:\=/%\clang-build-%LLVM_VERSION%
mkdir %TEMP%\clang-build-%LLVM_VERSION%
pushd %TEMP%\clang-build-%LLVM_VERSION%


unzip -o %ROOT_DIR:\=/%/%FILENAME%-windows.zip -d OUTPUT

set GIT_LLVM_RELEASE_HASH=43ff75f2c3feef64f9d73328230d34dac8832a91

git clone https://github.com/llvm/llvm-project source
pushd source
git checkout %GIT_LLVM_RELEASE_HASH%
popd

mkdir build_llvm
pushd build_llvm

%CMAKE_BINARY% -G "Visual Studio 16 2019" -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON -DCMAKE_INSTALL_PREFIX="..\install" -DPYTHON_EXECUTABLE="%PYTHON_BINARY%" "..\source\llvm"
%CMAKE_BINARY% --build . --target install --config MinSizeRel

popd

mkdir build_lld
pushd build_lld

%CMAKE_BINARY% -G "Visual Studio 16 2019" -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON -DCMAKE_INSTALL_PREFIX="..\install" -DPYTHON_EXECUTABLE="%PYTHON_BINARY%" -DLLVM_CONFIG_PATH="..\install\bin\llvm-config.exe" "..\source\lld"
%CMAKE_BINARY% -G "Visual Studio 16 2019" -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON -DCMAKE_INSTALL_PREFIX="..\install" -DPYTHON_EXECUTABLE="%PYTHON_BINARY%" -DLLVM_CONFIG_PATH="..\install\bin\llvm-config.exe" "..\source\lld"
%CMAKE_BINARY% --build . --target install --config MinSizeRel

popd

mkdir build_clang
pushd build_clang

%CMAKE_BINARY% -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="..\install" -DPYTHON_EXECUTABLE="%PYTHON_BINARY%" "..\source\clang"
%CMAKE_BINARY% --build . --target install --config MinSizeRel

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
rm -rf %ROOT_DIR:\=/%/%FILENAME%.zip
zip %ROOT_DIR:\=/%/%FILENAME%.zip *
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
