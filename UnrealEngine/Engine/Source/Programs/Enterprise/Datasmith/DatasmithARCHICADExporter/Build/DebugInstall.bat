@echo off
:: DebugInstall.bat
::  Copyright Epic Games, Inc. All Rights Reserved.

:: Get addon path, name and version

set BatPath=%~dp0
set BatPath=%BatPath:~0,-1%

set ACVersion=%1

if "%ACVersion%" EQU "" goto:InvalidACVersion
if %ACVersion% LSS 23 goto:InvalidACVersion
if %ACVersion% GTR 30 goto:InvalidACVersion

set DLLName=DatasmithUE4ArchiCAD
set AddOnName=DatasmithARCHICAD%ACVersion%Exporter
set OtherAddOnName=%AddOnName%-Debug

if "%2"=="-Debug" (
    set DLLName=%DLLName%-Win64-Debug
    set OtherAddOnName=%AddOnName%
    set AddOnName=%AddOnName%-Debug
)

set UEWin64=%BatPath%\..\..\..\..\..\..\..\Engine\Binaries\Win64

set DatasmithUE4ArchicadAddonFolder=%UEWin64%\DatasmithARCHICADExporter\ArchiCAD%ACVersion%

set DatasmithUE4ArchicadFolder=%UEWin64%\DatasmithUE4ArchiCAD

if exist "%DatasmithUE4ArchicadFolder%" (
    call:ProcessACVersion %ACVersion%
) else (
    echo DLL Folder missing: "%DatasmithUE4ArchicadFolder%" >> "%temp%\result.txt"
)

goto:eof

:ProcessACVersion
    for /F "tokens=*" %%i in ('reg.exe query HKCU\Software\GRAPHISOFT\ARCHICAD /f "%1.0.0" ^| FIND "ARCHICAD\"') do call:ProcessACVersionKey "%%i"
goto:eof

:ProcessACVersionKey
    set ACKey=%1
    :: Get the application's path
    for /F "tokens=3*" %%A in ('reg.exe query %ACKey% /ve ^| FIND ":\"') do call:ProcessOneACPath "%%B"
goto:eof

:ProcessOneACPath
    set ACAppPath=%1
    if exist "%1" (
        :: Get folder path of the application
        for %%M in (%ACAppPath%) do call:FindAPXPath "%%~dpM"
    )
goto:eof

:FindAPXPath
    set ACFolderPath=%1
    :: Remove the "
    set ACFolderPath=%ACFolderPath:~1,-1%
    :: We search the apx folder equivalent to english "Add-Ons/Import-Export"
    set Search="%ACFolderPath%*Collada In-Out.apx"
    set APXPath=
    for /F "tokens=1* delims=:" %%i in ('dir %Search% /s 2^>NUL ^| FIND "%ACFolderPath%"') do set APXPath=%%j
    if "%APXPath%" NEQ "" (
        set APXPath=%ACFolderPath:~0,2%%APXPath%
        echo Installing "%AddOnName%" in "%APXPath%" >> "%temp%\result.txt"
        
        :: Copy the dll and debug files
        xcopy /Y "%DatasmithUE4ArchicadFolder%\%DLLName%.dll" "%ACFolderPath%"
        xcopy /Y "%DatasmithUE4ArchicadFolder%\%DLLName%.pdb" "%ACFolderPath%"

        :: Copy the addon
        :: Delete previous addon if installed
        rd /S /Q "%APXPath%\%OtherAddOnName%"
        rd /S /Q "%APXPath%\%AddOnName%"

        :: Copy the new Addon
        mkdir "%APXPath%\%AddOnName%"
        xcopy /Y "%DatasmithUE4ArchicadAddonFolder%\%AddOnName%.apx" "%APXPath%\%AddOnName%"
        xcopy /Y "%DatasmithUE4ArchicadAddonFolder%\%AddOnName%.pdb" "%APXPath%\%AddOnName%"
 
       echo Finish "%AddOnName%" in "%APXPath%" >> "%temp%\result.txt"
    )
goto:eof

:InvalidACVersion
    echo ARCHICAD version must be 23 or higher >> "%temp%\result.txt"
goto:eof
