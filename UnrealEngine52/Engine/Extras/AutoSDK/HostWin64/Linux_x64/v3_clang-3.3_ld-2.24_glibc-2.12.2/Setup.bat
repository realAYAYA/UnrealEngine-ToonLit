rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0
echo %CurrentFolder%

REM delete OutputEnvVars from the old location
del "%CurrentFolder%OutputEnvVars.txt"

set OutputEnvVarsFolder=%~dp0..\
del "%OutputEnvVarsFolder%OutputEnvVars.txt"

echo LINUX_ROOT=%CurrentFolder%toolchain>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM support old branches that expect this file in a different location.
copy "%OutputEnvVarsFolder%OutputEnvVars.txt" "%CurrentFolder%OutputEnvVars.txt"
