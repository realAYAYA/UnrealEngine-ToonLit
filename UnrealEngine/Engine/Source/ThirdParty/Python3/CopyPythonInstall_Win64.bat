@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

SET python_arch_name=Win64
SET python_lib_dest_name=.\%python_arch_name%
SET python_bin_dest_name=..\..\..\Binaries\ThirdParty\Python3\%python_arch_name%

SET python_src_name=Python39
SET python_src_dir=C:\Users\patrick.laflamme\AppData\Local\Programs\Python\%python_src_name%

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

ECHO Copying Python: %python_src_dir%
XCOPY /s /i /q "%python_src_dir%" "%python_bin_dest_name%"
RMDIR "%python_bin_dest_name%\Doc" /s /q
DEL "%python_bin_dest_name%\Lib\*.pyc" /s /q
REM XCOPY /q /y "..\..\..\Restricted\NoRedist\Source\ThirdParty\Python\TPS\PythonWinBin.tps" "%python_bin_dest_name%"
REM XCOPY /q /y "..\..\..\Restricted\NoRedist\Source\ThirdParty\Python\TPS\VisualStudio2008.tps" "%python_bin_dest_name%"
REM XCOPY /q /y "Python3.tps" "%python_bin_dest_name%\..\"
XCOPY /s /i /q "%python_bin_dest_name%\include" "%python_lib_dest_name%\include"
XCOPY /s /i /q "%python_bin_dest_name%\libs" "%python_lib_dest_name%\libs"
RMDIR "%python_bin_dest_name%\include" /s /q
RMDIR "%python_bin_dest_name%\libs" /s /q

:End

PAUSE
