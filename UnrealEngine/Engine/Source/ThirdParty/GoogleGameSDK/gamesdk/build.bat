set ANDROID_HOME=C:\win_build_extras\sdk
set ANDROID_NDK_HOME=C:\win_build_extras\ndk\r21
set BUILDBOT_SCRIPT=true
set BUILDBOT_CMAKE="%CD%\..\prebuilts\cmake\windows-x86"

if not defined DIST_DIR set DIST_DIR="%CD%\..\package"

gradlew packageLocalZip -Plibraries=swappy,tuningfork -PincludeSampleArtifacts -PdistPath="%DIST_DIR%" -Pndk=21.3.6528147 --no-daemon
