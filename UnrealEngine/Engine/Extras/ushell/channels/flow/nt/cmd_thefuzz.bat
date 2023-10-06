:: Copyright Epic Games, Inc. All Rights Reserved.

@echo off

setlocal
set _bat="%temp%\thefuzz_%flow_sid%.bat"
set _fzf=fzf --layout=reverse --color=16 --height=20

<nul >%_bat% (
    if "%~1"=="history" call:history
    if "%~1"=="chdir"   call:chdir %2
    if "%~1"=="explore" call:explore %2
)

endlocal & if not errorlevel 1 (
    call %_bat%
)

goto:eof

::------------------------------------------------------------------------------
:history
set _hist=
for /f "tokens=1,2 delims==" %%d in ('doskey /macros') do (
    if "%%d"=="history" (
        set _hist=%%e
        goto:_hist_set_break
    )
)
:_hist_set_break
if defined _hist (
    call:history_impl %_hist%
)
goto:eof

:history_impl
for /f "delims=" %%d in ('""%~f1" %2 | rg "^^[0-9 ]+^(.+^)$" -r $1 | %_fzf% --scheme=history --history=%temp%/thefuzz_hist"') do (
    echo set _hist_result="%%d"
    echo set _hist_result=%%_hist_result:^|=^^^|%%
    echo set _hist_result=%%_hist_result:^&=^^^&%%
    echo set _hist_result=%%_hist_result:^<=^^^<%%
    echo set _hist_result=%%_hist_result:^>=^^^>%%
    echo echo %%_hist_result:~1,-1%%^>%temp%\thefuzz_%flow_sid%_hist_tmp
    echo clip ^<%temp%\thefuzz_%flow_sid%_hist_tmp
    echo echo Copied to clipboard; [92m%%_hist_result:~1,-1%%[0m
    echo set _hist_result=
)
goto:eof

::------------------------------------------------------------------------------
:chdir
set _base="%~f1"
if "%~1"=="" set _base=\
echo cd /d ^^
fd -d8 -td . %_base:\"="% | %_fzf% --scheme=default --history=%temp%/thefuzz_chdir
goto:eof

::------------------------------------------------------------------------------
:explore
set _base="%~f1"
if "%~1"=="" set _base=\
echo explorer.exe ^^
fd -d8 -td . %_base:\"="% | %_fzf% --scheme=default --history=%temp%/thefuzz_chdir
goto:eof
