rem TODO: check if this is under UE_SDKS_ROOT ?
set CurrentFolder=%~dp0
echo %CurrentFolder%

REM delete OutputEnvVars from the old location
del "%CurrentFolder%OutputEnvVars.txt"

set OutputEnvVarsFolder=%~dp0..\
del "%OutputEnvVarsFolder%OutputEnvVars.txt"



REM set android sdk environment variables
echo ANDROID_HOME=%CurrentFolder%android-sdk-windows>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo ANDROID_SDK_HOME=%CurrentFolder%android-sdk-windows>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM set ndk environment variables
echo ANDROID_NDK_ROOT=%CurrentFolder%android-ndk-r9c>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo NDK_ROOT=%CurrentFolder%android-ndk-r9c>>"%OutputEnvVarsFolder%OutputEnvVars.txt"
echo NDKROOT=%CurrentFolder%android-ndk-r9c>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM set ANT directory
echo ANT_HOME=%CurrentFolder%apache-ant-1.8.2>>"%OutputEnvVarsFolder%OutputEnvVars.txt"


REM set CYGWIN home directory
echo CYGWIN_HOME=%CurrentFolder%cygwin>>"%OutputEnvVarsFolder%OutputEnvVars.txt"


REM set JDK directory
echo JAVA_HOME=%CurrentFolder%jdk1.6.0_45>>"%OutputEnvVarsFolder%OutputEnvVars.txt"

REM support old branches that expect this file in a different location.
copy "%OutputEnvVarsFolder%OutputEnvVars.txt" "%CurrentFolder%OutputEnvVars.txt"
