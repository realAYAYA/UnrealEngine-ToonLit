@echo off

if not exist env (
	..\..\..\Binaries\ThirdParty\Python3\Win64\python.exe -m venv env
)

call .\env\Scripts\activate.bat

python -m pip install --upgrade pip
pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
	rmdir /s /q env
	exit %ERRORLEVEL%
)
