@ECHO OFF

:: Specifies the version of opencv to download
set opencv_version=4.5.5

:: Comment the line below to exclude opencv_contrib from the build
set use_opencv_contrib=""

set opencv_url=https://github.com/opencv/opencv/archive/%opencv_version%.zip
set opencv_src=opencv-%opencv_version%

set opencv_contrib_url=https://github.com/opencv/opencv_contrib/archive/%opencv_version%.zip
set opencv_contrib_src=opencv_contrib-%opencv_version%

:: Create build directory
if not exist build md build

pushd build

:: Download opencv
if not exist %opencv_src% (
    if not exist opencv.zip (
        echo Downloading %opencv_url%...
        powershell -Command "(New-Object Net.WebClient).DownloadFile('%opencv_url%', '%opencv_src%.zip')"
    )
    echo Extracting %opencv_src%.zip...
    powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%opencv_src%.zip', '.')"
)

set EXTRA_MODULES_PATH="%~dp0UnrealModules"

if defined use_opencv_contrib (
	:: Download opencv_contrib
	if not exist %opencv_contrib_src% (
		if not exist opencv_contrib.zip (
			echo Downloading %opencv_contrib_url%...
			powershell -Command "(New-Object Net.WebClient).DownloadFile('%opencv_contrib_url%', '%opencv_contrib_src%.zip')"
		)
		echo Extracting %opencv_contrib_src%.zip...
		powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%opencv_contrib_src%.zip', '.')"
	)

	:: Append it to the extra modules path for opencv to compile in
	set EXTRA_MODULES_PATH=%EXTRA_MODULES_PATH%;"%~dp0build\%opencv_contrib_src%\modules"
)

echo Deleting existing build directories...
if exist x64 rd /s /q x64

:: Create x64 directory
IF NOT EXIST x64 (
	md x64
)

pushd x64

echo Configuring x64 build...

cmake^
 -G "Visual Studio 16 2019"^
 -A x64^
 -C "%~dp0\cmake_options.txt"^
 -DCMAKE_INSTALL_PREFIX=%~dp0^
 -DOPENCV_EXTRA_MODULES_PATH=%EXTRA_MODULES_PATH%^
 "..\%opencv_src%"

echo Building x64 Release build...
cmake.exe --build . --config Release --target INSTALL -- /m:4

echo Building x64 Debug build...
cmake.exe --build . --config Debug --target INSTALL -- /m:4

:: x64/..
popd

echo Moving outputs to destination folders...

set bin_path="%~dp0\..\..\..\Binaries\ThirdParty"
set lib_path="%~dp0\lib"

echo bin_path is %bin_path%
echo lib_path is %lib_path%

echo %bin_path%\Win64
echo %lib_path%\Win64

IF NOT EXIST %bin_path%\Win64 (
	md %bin_path%\Win64
)

IF NOT EXIST %lib_path%\Win64 (
	md %lib_path%\Win64
)

move /y x64\bin\Release\opencv_*.*   %bin_path%\Win64
move /y x64\bin\Debug\opencv_*.*     %bin_path%\Win64
move /y x64\lib\Release\opencv_*.lib %lib_path%\Win64
move /y x64\lib\Debug\opencv_*.lib   %lib_path%\Win64

echo Cleaning up...

rd /s /q x64

:: build/..
popd

:: Remove generated .cmake files
del /f OpenCV*.cmake

echo Done. Remember to delete the build directory and submit changed files to p4
