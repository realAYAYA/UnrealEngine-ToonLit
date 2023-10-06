:: Copyright Epic Games, Inc. All Rights Reserved.

@echo off
pushd %~dp0

cl.exe 1>nul 2>nul
if errorlevel 9009 (
    for /f "delims=" %%d in ('vswhere -latest -find VC/Auxiliary/Build/vcvars64.bat') do (
        call "%%d"
    )
)

setlocal

set pysdk=%~f1
if "%pysdk%"=="" goto:usage

if not "%3"=="release" (
    set _cl=/Od
    set _link=/debug
) else (
    set _cl=/O1 /Os
)

set _cl="/I%pysdk%/include" /nologo /Zl /Zi /EHs-c- /c %_cl%
set _link=%_link% /nologo /dll /machine:x64^
    /nodefaultlib /align:16^
    /incremental:no /opt:ref,icf^
    "/libpath:%pysdk%/libs" python3.lib kernel32.lib

mkdir 1>nul 2>nul _build
cd _build

call:go cl.exe ../native.cpp %_cl%
if errorlevel 1 exit /b 1

call:go link.exe %_link% /out:native.pyd native.obj
if errorlevel 1 exit /b 1

move /y native.pyd ..\__init__.pyd

popd
goto:eof

:go
echo.
echo [96m%*[0m
%*
goto:eof

:usage
echo usage: %~nx0 pysdk [release]
goto:eof
