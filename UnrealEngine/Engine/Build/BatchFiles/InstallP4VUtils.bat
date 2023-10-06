@rem Copyright Epic Games, Inc. All Rights Reserved.

@echo off
setlocal

cd %~dp0%

set DESTDIR=%LOCALAPPDATA%\Unreal Engine\P4VUtils
set SOURCEDIR=..\..\..\Extras\P4VUtils\Binaries\Win64
set MESSAGE=Installing P4VUtils into p4v...

dir
dir ..\..\Restricted\NotForLicensees\Extras\P4VUtils
if EXIST ..\..\Restricted\NotForLicensees\Extras\P4VUtils (
	set MESSAGE=Installing P4VUtils [with Epic extensions] into p4v...
	set SOURCEDIR=..\..\Restricted\NotForLicensees\Extras\P4VUtils\Binaries\Win64
)

rmdir /s /q "%DESTDIR%" 2> NUL
xcopy /s /y /i "%SOURCEDIR%" "%DESTDIR%" 2> NUL

echo %MESSAGE%
"%DESTDIR%\P4VUtils" install
