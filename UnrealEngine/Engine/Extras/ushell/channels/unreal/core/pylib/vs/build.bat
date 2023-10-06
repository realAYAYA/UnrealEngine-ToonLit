@echo off
pushd %~dp0

cl.exe 1>nul 2>nul
if errorlevel 9009 (
    for /f "delims=" %%d in ('vswhere -latest -find VC/Auxiliary/Build/vcvars64.bat') do (
        call "%%d"
    )
)

setlocal

set vssdk=%~f1
if "%vssdk%"=="" goto:usage

set pysdk=%~f2
if "%pysdk%"=="" goto:usage

if not "%3"=="release" (
    set _cl=/Od
    set _link=/debug
) else (
    set _cl=/O1 /Os
)

set _cl="/I%vssdk%/Inc" "/I%pysdk%/include" /nologo /Zl /Zi /EHs-c- /c %_cl%
set _link=%_link% /nologo /dll /machine:x64^
    /nodefaultlib /align:16^
    /incremental:no /opt:ref,icf^
    "/libpath:%pysdk%/libs" python3.lib ole32.lib oleaut32.lib kernel32.lib

mkdir 1>nul 2>nul _build
cd _build

call:go cl.exe ../dte.cpp %_cl%
if errorlevel 1 exit /b 1

call:go link.exe %_link% /out:dte.pyd dte.obj
if errorlevel 1 exit /b 1

move /y dte.pyd ..

popd
goto:eof

:go
echo.
echo [96m%*[0m
%*
goto:eof

:usage
echo usage: %~nx0 vssdk pysdk [release]
goto:eof
