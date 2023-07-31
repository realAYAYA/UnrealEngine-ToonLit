rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0
echo %CurrentFolder%

REM delete OutputEnvVars from the old location
del "%CurrentFolder%OutputEnvVars.txt"

set OutputEnvVarsFolder=%~dp0..\
del "%OutputEnvVarsFolder%OutputEnvVars.txt"



REM set android sdk environment variables
echo ANDROID_HOME=%CurrentFolder%>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo ANDROID_SDK_HOME=%CurrentFolder%>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM set ndk environment variables
echo ANDROID_NDK_ROOT=%CurrentFolder%ndk\21.0.6113669>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo NDK_ROOT=%CurrentFolder%ndk\21.0.6113669>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo NDKROOT=%CurrentFolder%ndk\21.0.6113669>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM set JDK directory
echo JAVA_HOME=%CurrentFolder%jre>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM Set the Andriod SWT path 
echo ANDROID_SWT=%CurrentFolder%\tools\lib\x86_64>>"%OutputEnvVarsFolder%OutputEnvVars.txt"




REM support old branches that expect this file in a different location.
copy "%OutputEnvVarsFolder%OutputEnvVars.txt" "%CurrentFolder%OutputEnvVars.txt"

