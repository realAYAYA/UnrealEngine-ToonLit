@echo off
:: DebugInstallAccess.bat
::  Copyright Epic Games, Inc. All Rights Reserved.

:: Get addon path, name and version
set BatFilePath=%~f0
set InstallFileDir=%BatFilePath%\..\DebugInstall.bat

echo "%~f0" %* > "%temp%\result.txt"

:: Check if we have admin access
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"

if '%errorlevel%' NEQ '0' (
    echo Requesting Admin access...
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "cmd.exe", "/c ""%InstallFileDir%"" %*", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    del "%temp%\getadmin.vbs"
    :: Timeout of 1 second need to get "%temp%\result.txt" flushed
    ping 192.0.2.1 -n 1 -w 1000 > NUL
) else (
    echo Admin access already granted...
    "%InstallFileDir%"
)

type "%temp%\result.txt"

echo Done
