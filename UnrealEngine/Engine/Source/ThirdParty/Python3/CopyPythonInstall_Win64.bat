@ECHO OFF
REM Run from root of engine installation as CWD. First argument is path to the python installation
SETLOCAL ENABLEEXTENSIONS

SET python_arch_name=Win64
SET python_lib_dest_name=.\Engine\Source\ThirdParty\Python3\%python_arch_name%
SET python_bin_dest_name=.\Engine\Binaries\ThirdParty\Python3\%python_arch_name%

SET python_src_dir="%~1"

IF NOT EXIST "%python_src_dir%" (
	ECHO Python Source Directory Missing: %python_src_dir%
	GOTO End
)

IF EXIST "%python_lib_dest_name%" (
	ECHO Removing Existing Target Directory: %python_lib_dest_name%
	RMDIR "%python_lib_dest_name%" /s /q
)

IF EXIST "%python_bin_dest_name%" (
	ECHO Removing Existing Target Directory: %python_bin_dest_name%
	RMDIR "%python_bin_dest_name%" /s /q
)
pause

ECHO Copying Python: %python_src_dir%
robocopy "%python_src_dir%" "%python_bin_dest_name%" /S /MIR /XD Doc /XD include /XD libs /XF *.pyc
robocopy "%python_src_dir%\include" "%python_lib_dest_name%\include" /S /MIR /XF *.pyc
robocopy "%python_src_dir%\libs" "%python_lib_dest_name%\libs" /S /MIR /XF *.pyc
REM cleans empty subfolders resulting from filters
robocopy "%python_bin_dest_name%" "%python_bin_dest_name%" /S /MOVE
robocopy "%python_lib_dest_name%\include" "%python_lib_dest_name%\include" /S /MOVE
robocopy "%python_lib_dest_name%\libs" "%python_lib_dest_name%\libs" /S /MOVE

:End

PAUSE
