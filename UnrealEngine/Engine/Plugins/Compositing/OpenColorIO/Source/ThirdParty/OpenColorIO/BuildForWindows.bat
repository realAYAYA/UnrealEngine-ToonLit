@echo off

rem Copyright Epic Games, Inc. All Rights Reserved.

rem Setup part
setlocal

set OCIO_VERSION=2.2.0
set OCIO_LIB_NAME=OpenColorIO-%OCIO_VERSION%
set DEPLOY_FOLDER=..\Deploy\%OCIO_LIB_NAME%
set WITH_PYTHON=false

@REM rem Download library source if not present
@REM if not exist %OCIO_LIB_NAME%.zip (
@REM     powershell -Command "Invoke-WebRequest https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.0.zip -OutFile %OCIO_LIB_NAME%.zip"
@REM )

rem Remove previously extracted build library folder
if exist .\%OCIO_LIB_NAME% (
    rd /S /Q .\%OCIO_LIB_NAME%
)

@REM echo Extracting %OCIO_LIB_NAME%.zip...
@REM powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%OCIO_LIB_NAME%.zip', '.')"
    
git clone --depth 1 --branch v2.2.0 https://github.com/AcademySoftwareFoundation/OpenColorIO.git %OCIO_LIB_NAME%

cd /d .\%OCIO_LIB_NAME%

rem Configure OCIO cmake and launch a release build
echo Configuring x64 build...
if %WITH_PYTHON% equ true (
    rem NOTE: Assumes a local python installation matching the engine's version is available (currently 3.7.7).
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON -DOCIO_BUILD_STATIC=OFF -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_GPU_TESTS=OFF -DOCIO_BUILD_NUKE=OFF -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_PYGLUE=OFF -DOCIO_BUILD_JNIGLUE=OFF -DOCIO_STATIC_JNIGLUE=OFF -DOCIO_USE_BOOST_PTR=OFF -DOCIO_PYGLUE_LINK=OFF -DCMAKE_INSTALL_PREFIX:PATH=.\install
) else (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON -DOCIO_BUILD_STATIC=OFF -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_GPU_TESTS=OFF -DOCIO_BUILD_NUKE=OFF -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_PYGLUE=OFF -DOCIO_BUILD_JNIGLUE=OFF -DOCIO_STATIC_JNIGLUE=OFF -DOCIO_USE_BOOST_PTR=OFF -DOCIO_PYGLUE_LINK=OFF -DOCIO_BUILD_PYTHON=OFF -DCMAKE_INSTALL_PREFIX:PATH=.\install
)
echo Building x64 Release build...
cmake --build build --config Release --target INSTALL

rem Remove previous deployment file
if exist %DEPLOY_FOLDER% (
    rd /S /Q %DEPLOY_FOLDER%
)

echo Copying deploy files...
xcopy .\build\install\bin\OpenColorIO_2_2.dll %DEPLOY_FOLDER%\..\..\..\..\..\Binaries\ThirdParty\Win64\* /Y
xcopy .\build\install\include\OpenColorIO\* %DEPLOY_FOLDER%\include\OpenColorIO\* /Y
xcopy .\build\install\lib\OpenColorIO.lib %DEPLOY_FOLDER%\lib\Win64\* /Y
if %WITH_PYTHON% equ true (
    xcopy .\build\install\lib\site-packages\* %DEPLOY_FOLDER%\..\..\..\..\..\Content\Python\Lib\Win64\site-packages\* /Y
)

endlocal
pause

