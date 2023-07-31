@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

SET ROOT=%~dp0..

rmdir /s /q %ROOT%\include
robocopy %ROOT%\src\google %ROOT%\include\google *.h *.inc *.proto /MIR /XD testdata testing internal /XF *unittest* test_*
