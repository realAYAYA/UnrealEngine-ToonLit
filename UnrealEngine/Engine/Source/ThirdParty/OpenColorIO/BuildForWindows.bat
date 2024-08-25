@echo off
setlocal
rem Copyright Epic Games, Inc. All Rights Reserved.

rem Setup part
setlocal

if [%1]==[] goto usage

set OCIO_VERSION=2.3.1
set OCIO_LIB_NAME=OpenColorIO-%OCIO_VERSION%
set ENGINE_ROOT=%~dp0..\..\..
set ARCH_NAME=%1

rem Remove previously extracted build library folder
if exist .\%OCIO_LIB_NAME% (
    rd /S /Q .\%OCIO_LIB_NAME%
)
    
git clone --depth 1 --branch v%OCIO_VERSION% https://github.com/AcademySoftwareFoundation/OpenColorIO.git %OCIO_LIB_NAME%

cd /d .\%OCIO_LIB_NAME%
set DEPLOY_FOLDER=..\Deploy\OpenColorIO

git apply ../ue_ocio_v23.patch

rem Configure OCIO cmake and launch a release build
echo Configuring %ARCH_NAME% build...
cmake -S . -B build -G "Visual Studio 17 2022"^
    -A %ARCH_NAME%^
    -DBUILD_SHARED_LIBS=ON^
    -DOCIO_BUILD_STATIC=OFF^
    -DOCIO_BUILD_TRUELIGHT=OFF^
    -DOCIO_BUILD_APPS=OFF^
    -DOCIO_BUILD_GPU_TESTS=OFF^
    -DOCIO_BUILD_NUKE=OFF^
    -DOCIO_BUILD_DOCS=OFF^
    -DOCIO_BUILD_TESTS=OFF^
    -DOCIO_BUILD_PYGLUE=OFF^
    -DOCIO_BUILD_JNIGLUE=OFF^
    -DOCIO_STATIC_JNIGLUE=OFF^
    -DOCIO_USE_BOOST_PTR=OFF^
    -DOCIO_PYGLUE_LINK=OFF^
    -DOCIO_BUILD_PYTHON=OFF^
    -DCMAKE_INSTALL_PREFIX:PATH=.\install

echo Building %ARCH_NAME% Release build...
cmake --build build --config Release --target INSTALL

echo Copying deploy files...
xcopy .\build\install\bin\OpenColorIO_2_3.dll %ENGINE_ROOT%\Binaries\ThirdParty\OpenColorIO\Win64\%ARCH_NAME%\* /Y
xcopy .\build\install\include\OpenColorIO\* %DEPLOY_FOLDER%\include\OpenColorIO\* /Y
xcopy .\build\install\lib\OpenColorIO.lib %DEPLOY_FOLDER%\lib\Win64\%ARCH_NAME%\* /Y

endlocal
pause

echo Done.
exit /B 0

:usage
echo Arch: x64 or ARM64
exit /B 1

endlocal