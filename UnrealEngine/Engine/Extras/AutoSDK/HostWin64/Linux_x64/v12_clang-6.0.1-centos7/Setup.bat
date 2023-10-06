rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0
echo %CurrentFolder%

REM delete OutputEnvVars from the old location
del "%CurrentFolder%OutputEnvVars.txt"

set OutputEnvVarsFolder=%~dp0..\
del "%OutputEnvVarsFolder%OutputEnvVars.txt"

rem Still set LINUX_ROOT for compatibility
echo LINUX_ROOT=%CurrentFolder%x86_64-unknown-linux-gnu>"%OutputEnvVarsFolder%OutputEnvVars.txt"

rem set new LINUX_MULTIARCH_ROOT that supersedes it
echo LINUX_MULTIARCH_ROOT=%CurrentFolder%>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM support old branches that expect this file in a different location.
copy "%OutputEnvVarsFolder%OutputEnvVars.txt" "%CurrentFolder%OutputEnvVars.txt"
