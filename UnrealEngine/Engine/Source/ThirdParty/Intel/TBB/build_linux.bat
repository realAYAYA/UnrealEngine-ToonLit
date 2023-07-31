@echo off
REM Requirements for this batch file to work
REM   Define LINUXBUILDHOST as your target machine IP/DNS
REM   Define LINUXBUILDUSER as the user to use for SSH login
REM   Install https://git-scm.com/download/win tools and put /usr/bin in your PATH
REM   Add rsync into your /usr/bin directory
REM      Download from http://repo.msys2.org/msys/x86_64/rsync-3.1.3-1-x86_64.pkg.tar.xz
REM   Generate ssh keys if not already available for your user
REM     ssh-keygen
REM   Install your ssh keys on the host
REM     bash ssh-copy-id %LINUXBUILDUSER%@%LINUXBUILDHOST%
REM     
set USERHOST=%LINUXBUILDUSER%@%LINUXBUILDHOST%
set INTELTBB=IntelTBB-2019u8
set PLATFORM=Linux

CALL build_unix_common.bat