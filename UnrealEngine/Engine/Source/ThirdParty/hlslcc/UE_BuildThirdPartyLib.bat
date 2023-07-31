@echo off
REM hlslcc
pushd hlslcc\projects

	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2017 x64
	pushd vs2017
	REM FYI Files are still generated into libs/2015...
	msbuild hlslcc.sln /target:Clean,hlslcc_lib /p:Platform=x64;Configuration="Debug"
	msbuild hlslcc.sln /target:Clean,hlslcc_lib /p:Platform=x64;Configuration="Release"
	popd

	REM Linux (only if LINUX_ROOT is defined)
	set CheckLINUX_ROOT=%LINUX_ROOT%
	if "%CheckLINUX_ROOT%"=="" goto SkipLinux

	pushd Linux
	call CrossCompile.bat
	popd

:SkipLinux

popd
