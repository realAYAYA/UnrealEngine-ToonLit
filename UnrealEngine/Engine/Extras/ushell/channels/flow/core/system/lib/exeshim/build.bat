:: Copyright Epic Games, Inc. All Rights Reserved.

@echo off
setlocal

cl.exe 1>nul 2>nul
if errorlevel 9009 (
    for /f "delims=" %%d in ('vswhere -latest -find VC/Auxiliary/Build/vcvars64.bat') do (
        call "%%d"
    )
)

pushd %~dp0
1>nul 2>nul mkdir _build
cd _build

if "%1"=="debug" (
    set _cl=/Od /DDEBUG
    set _link=/debug
)
call:build 1 py
call:build 2 shim
call:build 3 dump

dump.exe
if errorlevel 1 (
    echo !! dump failed!
)

popd
goto:eof

:build
echo.
echo shim.cpp / %2 / %1
cl ../shim.cpp /Fo%2.obj /DSHIM_BUILD=%1 /nologo /FC /Zi /Zl /EHs-c- /O1 /c %_cl%
link %_link% /nologo /nodefaultlib /align:16 /entry:%2 /out:%2.exe /subsystem:console %2.obj kernel32.lib user32.lib shell32.lib
goto:eof
