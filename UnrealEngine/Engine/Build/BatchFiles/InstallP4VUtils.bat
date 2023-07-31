setlocal

set LIBPATH=%LOCALAPPDATA%\Unreal Engine\P4VUtils

echo Copying P4VUtils files to %LIBPATH%...
mkdir "%LIBPATH%" 2> NUL

copy ..\..\Extras\P4VUtils\Binaries\Win64\* "%LIBPATH%"
copy ..\..\Extras\P4VUtils\P4VUtils.ini "%LIBPATH%"
xcopy /s /y ..\..\Restricted\NotForLicensees\Extras\P4VUtils\* "%LIBPATH%\NotForLicensees\" 2> NUL

echo Installing P4VUtils into p4v...
dotnet "%LIBPATH%\P4VUtils.dll" install
