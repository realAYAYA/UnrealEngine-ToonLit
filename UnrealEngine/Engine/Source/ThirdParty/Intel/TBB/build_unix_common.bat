REM This must be called from other batch files to properly setup required variables

REM Generate a unique workspace ID using our machine name and current workspace
(hostname && pwd) | md5sum | cut -d " " -f 1 > workspace.id
set /p WORKSPACE_ID=<workspace.id

set WORKDIR=~/remote_build/%WORKSPACE_ID%
set SSHCMD=ssh %USERHOST%

%SSHCMD% "mkdir -p %WORKDIR%"

REM We could filter out sending local binaries as we don't need them for remote builds
rsync -av -e "ssh" ./%INTELTBB%/ %USERHOST%:%WORKDIR%/

%SSHCMD% "cd %WORKDIR% && make work_dir=build/tmp clean && make work_dir=build/tmp && make work_dir=build/tmp extra_inc=big_iron.inc"

p4 edit ./%INTELTBB%/lib/%PLATFORM%/libtbb*

REM Fetch binaries generated from remote build
scp %USERHOST%:%WORKDIR%/build/tmp_release/libtbb*.dylib* ./%INTELTBB%/lib/%PLATFORM%
scp %USERHOST%:%WORKDIR%/build/tmp_release/libtbb*.so*    ./%INTELTBB%/lib/%PLATFORM%
scp %USERHOST%:%WORKDIR%/build/tmp_release/libtbb*.a*     ./%INTELTBB%/lib/%PLATFORM%
scp %USERHOST%:%WORKDIR%/build/tmp_debug/libtbb*.dylib*   ./%INTELTBB%/lib/%PLATFORM%
scp %USERHOST%:%WORKDIR%/build/tmp_debug/libtbb*.so*      ./%INTELTBB%/lib/%PLATFORM%
scp %USERHOST%:%WORKDIR%/build/tmp_debug/libtbb*.a*       ./%INTELTBB%/lib/%PLATFORM%

