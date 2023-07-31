@echo off
REM hlslcc
pushd Projects
echo ----
echo %cd%
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2017 x64
	pushd vs2017\SPIRV-Reflect
echo ----
echo %cd%
	msbuild SPIRV-Reflect.sln /target:Clean,SPIRV-Reflect /p:Platform=x64;Configuration="Debug"
	msbuild SPIRV-Reflect.sln /target:Clean,SPIRV-Reflect /p:Platform=x64;Configuration="Release"
	popd

	REM Linux (only if LINUX_ROOT is defined)
	set CheckLINUX_ROOT=%LINUX_ROOT%
	if "%CheckLINUX_ROOT%"=="" goto SkipLinux

	REM pushd Linux
	REM call CrossCompile.bat
	REM popd

:SkipLinux

popd
