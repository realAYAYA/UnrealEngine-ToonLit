@echo off
setlocal 

pushd "%~dp0"

set config=Shipping

if "%~1"=="" goto blank

set config=%1

:blank

echo.
echo === Building Targets ===
pushd "../../../.."
call Engine/Build/BatchFiles/RunUBT.bat -NoUba -NoUbaLocal -NoSNDBS -NoXGE -Architectures=x64+arm64 ^
	-Target="UbaAgent Win64 %config%" ^
	-Target="UbaCli Win64 %config%" ^
	-Target="UbaDetours Win64 %config%" ^
	-Target="UbaHost Win64 %config%" ^
	-Target="UbaStorageProxy Win64 %config%" ^
	-Target="UbaVisualizer Win64 %config%"

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
popd

echo.
echo === Reconciling artifacts ===
p4 reconcile ../../../Binaries/Win64/UnrealBuildAccelerator/...

popd

endlocal

exit /b 0
