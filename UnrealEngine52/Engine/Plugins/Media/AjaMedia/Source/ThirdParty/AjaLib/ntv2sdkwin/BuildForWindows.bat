rem Copyright Epic Games, Inc. All Rights Reserved.

rem Setup part
setlocal
set AJANTV2_VERSION=16.2-bugfix5
set AJANTV2_LIB_NAME=AjaNTV2-%AJANTV2_VERSION%
set AJANTV2_LIB_FOLDER=ntv2
set AJANTV2_FOLDER=ntv2-%AJANTV2_VERSION%
set DEPLOY_FOLDER=..\..\..\..\ntv2lib-deploy
set AJANTV2_BUILD_FOLDER=cmake-build


rem Download library source if not present
if not exist %AJANTV2_LIB_NAME%.zip (
	powershell -Command "Invoke-WebRequest https://github.com/aja-video/ntv2/archive/refs/tags/v16.2-bugfix5.zip -OutFile %AJANTV2_LIB_NAME%.zip"
)
rem Remove previously extracted build library folder
if exist .\%AJANTV2_LIB_FOLDER% (
   rd /S /Q .\%AJANTV2_LIB_FOLDER%
)

echo Extracting %AJANTV2_LIB_NAME%.zip...
powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%AJANTV2_LIB_NAME%.zip', '%AJANTV2_LIB_FOLDER%')"
cd /d .\%AJANTV2_LIB_FOLDER%\%AJANTV2_FOLDER%
mkdir %AJANTV2_BUILD_FOLDER%
cd %AJANTV2_BUILD_FOLDER%

rem Configure AjaNTV2 cmake and launch a release build
echo Configuring x64 build environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" 

echo Configuring x64 debug cmake config...
cmake -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 16 2019" -A x64 ..
 
echo Building x64 Debug build...
cmake --build . --config Debug

echo Configuring x64 Release cmake config...
cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 16 2019" -A x64 ..

echo Building x64 Release build...
cmake --build . --config Release

rem Remove previous deployment file
if exist %DEPLOY_FOLDER% (
    rd /S /Q %DEPLOY_FOLDER%
)

echo Copying library for deployment ...
xcopy .\ajalibraries\ajantv2\Debug\ajantv2d.lib %DEPLOY_FOLDER%\lib\libajantv2d.lib* /Y
xcopy .\ajalibraries\ajantv2\Release\ajantv2.lib %DEPLOY_FOLDER%\lib\libajantv2.lib* /Y

echo Copying all headers for deployment ...
xcopy "..\ajalibraries\*.h" "%DEPLOY_FOLDER%\includes\" /S /C /Y
xcopy "..\ajalibraries\*.hh" "%DEPLOY_FOLDER%\includes\" /S /C /Y

endlocal
pause