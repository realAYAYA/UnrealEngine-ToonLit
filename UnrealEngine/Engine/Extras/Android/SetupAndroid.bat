@echo off
setlocal ENABLEEXTENSIONS
setlocal ENABLEDELAYEDEXPANSION
set KEY_NAME=HKLM\SOFTWARE\Android Studio
set VALUE_NAME=Path
set STUDIO_PATH=

IF "%5" == "-noninteractive" (
	set PAUSE=
) ELSE (
	set PAUSE=pause
)

SET PLATFORMS_VERSION=%1
SET BUILDTOOLS_VERSION=%2
SET CMAKE_VERSION=%3
SET NDK_VERSION=%4

rem hardcoded versions for compatibility with non-Turnkey manual running
if "%PLATFORMS_VERSION%" == "" SET PLATFORMS_VERSION=android-32
if "%BUILDTOOLS_VERSION%" == "" SET BUILDTOOLS_VERSION=30.0.3
if "%CMAKE_VERSION%" == "" SET CMAKE_VERSION=3.10.2.4988404
if "%NDK_VERSION%" == "" SET NDK_VERSION=25.1.8937393


FOR /F "tokens=2*" %%A IN ('REG.exe query "%KEY_NAME%" /v "%VALUE_NAME%"') DO (set STUDIO_PATH=%%B)

IF EXIST "%STUDIO_PATH%" (
	echo.
	) ELSE (
	echo Android Studio not installed, please download Android Studio 3.5.3 from https://developer.android.com/studio
	%PAUSE%
	exit /b 1
)
echo Android Studio Path: %STUDIO_PATH%

set VALUE_NAME=SdkPath
set STUDIO_SDK_PATH=
FOR /F "tokens=2*" %%A IN ('REG.exe query "%KEY_NAME%" /v "%VALUE_NAME%"') DO (set STUDIO_SDK_PATH=%%B)

set ANDROID_LOCAL=%LOCALAPPDATA%\Android\Sdk

if "%STUDIO_SDK_PATH%" == "" (
	IF EXIST "%ANDROID_LOCAL%" (
		set STUDIO_SDK_PATH=%ANDROID_LOCAL%
	) ELSE (
		IF EXIST "%ANDROID_HOME%" (
			set STUDIO_SDK_PATH=%ANDROID_HOME%
		) ELSE (
			echo Unable to locate local Android SDK location. Did you run Android Studio after installing?
			%PAUSE%
			exit /b 2
		)
	)
)
echo Android Studio SDK Path: %STUDIO_SDK_PATH%

if DEFINED ANDROID_HOME (set a=1) ELSE (
	set ANDROID_HOME=%STUDIO_SDK_PATH%
	setx ANDROID_HOME "%STUDIO_SDK_PATH%"
)
if DEFINED ANDROID_SDK_HOME (set a=1) ELSE (
	set ANDROID_SDK_HOME=%STUDIO_SDK_PATH%
	setx ANDROID_SDK_HOME "%STUDIO_SDK_PATH%"
)
if DEFINED JAVA_HOME (set a=1) ELSE (
	set JAVA_HOME=%STUDIO_PATH%\jre
	setx JAVA_HOME "%STUDIO_PATH%\jre"
)
set NDKINSTALLPATH=%STUDIO_SDK_PATH%\ndk\%NDK_VERSION%
set PLATFORMTOOLS=%STUDIO_SDK_PATH%\platform-tools;%STUDIO_SDK_PATH%\tools

set KEY_NAME=HKCU\Environment
set VALUE_NAME=Path
set USERPATH=

FOR /F "tokens=2*" %%A IN ('REG.exe query "%KEY_NAME%" /v "%VALUE_NAME%"') DO (set USERPATH=%%B)

where.exe /Q adb.exe
IF /I "%ERRORLEVEL%" NEQ "0" (
	echo Current user path: "%USERPATH%"
	setx PATH "%USERPATH%;%PLATFORMTOOLS%"
	echo Added %PLATFORMTOOLS% to path
)

set SDKMANAGER=%STUDIO_SDK_PATH%\cmdline-tools\latest\bin\sdkmanager.bat
IF EXIST "%SDKMANAGER%" (
	echo Using sdkmanager: %SDKMANAGER%
) ELSE (
	set SDKMANAGER=%STUDIO_SDK_PATH%\tools\bin\sdkmanager.bat
	IF EXIST "!SDKMANAGER!" (
		echo Using sdkmanager: !SDKMANAGER!
	) ELSE (
		echo Unable to locate sdkmanager.bat. Did you run Android Studio and install cmdline-tools after installing?
		%PAUSE%
		exit /b 3
	)
)

call "%SDKMANAGER%" "platform-tools" "platforms;%PLATFORMS_VERSION%" "build-tools;%BUILDTOOLS_VERSION%" "cmake;%CMAKE_VERSION%" "ndk;%NDK_VERSION%"

IF /I "%ERRORLEVEL%" NEQ "0" (
	echo Update failed. Please check the Android Studio install.
	%PAUSE%
	exit /b 4
)

if EXIST "%NDKINSTALLPATH%" (
	echo Success!
	setx NDKROOT "%NDKINSTALLPATH%"
	setx NDK_ROOT "%NDKINSTALLPATH%"
) ELSE (
	echo Update failed. Did you accept the license agreement?
	%PAUSE%
	exit /b 5
)

%PAUSE%
exit /b 0
