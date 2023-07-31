#!/usr/bin/env bash

PLATFORMS_VERSION=${1:-}
BUILDTOOLS_VERSION=${2:-}
CMAKE_VERSION=${3:-}
NDK_VERSION=${4:-}

ARG5=${5:-}
pausefunc()
{
    read -rsp $'Press any key to continue...\n' -n1 key
}

# $1 parameter should be the profile file: .bash_profile as it is read by UE and .zprofile if default shell is zsh
updateProfileFileFunc()
{
	if ! grep -q "export ANDROID_HOME=\"$STUDIO_SDK_PATH\"" "$1" 
	then
		echo >> "$1"
		echo "export ANDROID_HOME=\"$STUDIO_SDK_PATH\"" >> "$1"
	fi
 
	if ! grep -q "export ANDROID_SDK_HOME=\"$STUDIO_SDK_PATH\"" "$1" 
	then
		echo >> "$1"
		echo "export ANDROID_SDK_HOME=\"$STUDIO_SDK_PATH\"" >> "$1"
	fi
 
	if ! grep -q "export JAVA_HOME=\"$JAVA_HOME\"" "$1"
	then
		echo >> $1
		echo "export JAVA_HOME=\"$JAVA_HOME\"" >> "$1"
	fi
 
	if [ "$adbPath" == "" ]; then
		if ! grep -q "export PATH=\"\$PATH:$PLATFORMTOOLS\"" "$1"
		then
			echo >> "$1"
			echo export PATH="\"\$PATH:$PLATFORMTOOLS\"" >> "$1"
			echo Added $PLATFORMTOOLS to PATH in $1 
		fi
	fi
 
	if ! grep -q "export NDKROOT=\"$NDKINSTALLPATH\"" "$1"
	then
		echo >> "$1"
		echo "export NDKROOT=\"$NDKINSTALLPATH\"" >> "$1"
		echo "export NDK_ROOT=\"$NDKINSTALLPATH\"" >> "$1"
	fi
}

if [[ ${ARG5} == "-noninteractive" ]]; then
    PAUSE=
else
    PAUSE="pausefunc"
fi

# hardcoded versions for compatibility with non-Turnkey manual running
if [[ -z "${PLATFORMS_VERSION}" ]]; then
    PLATFORMS_VERSION="android-32"
fi
if [[ -z "${BUILDTOOLS_VERSION}" ]]; then
    BUILDTOOLS_VERSION="30.0.3"
fi
if [[ -z "${CMAKE_VERSION}" ]]; then
    CMAKE_VERSION="3.10.2.4988404"
fi
if [[ -z "${NDK_VERSION}" ]]; then
    NDK_VERSION="25.1.8937393"
fi


STUDIO_PATH="$HOME/Applications/Android Studio.app"
if [ ! -x "$STUDIO_PATH" ]; then
	STUDIO_PATH="/Applications/Android Studio.app"
	if [ ! -x "$STUDIO_PATH" ]; then
		echo Android Studio not installed, please download Android Studio 4.0.2 from https://developer.android.com/studio
		${PAUSE}
		exit 1
	fi
fi
echo Android Studio Path: $STUDIO_PATH

if [ "$STUDIO_SDK_PATH" == "" ]; then
	STUDIO_SDK_PATH=~/Library/Android/sdk
fi
if [ "$1" != "" ]; then
	STUDIO_SDK_PATH=$1
fi
if [ ! -d "$STUDIO_SDK_PATH" ]; then
	echo Android SDK not found at: $STUDIO_SDK_PATH
	echo Unable to locate local Android SDK location. Did you run Android Studio after installing?
	echo If Android Studio is installed, please run again with SDK path as parameter, otherwise download Android Studio 4.0.2 from https://developer.android.com/studio
	${PAUSE}
	exit 1
fi
echo Android Studio SDK Path: $STUDIO_SDK_PATH

export JAVA_HOME="$STUDIO_PATH/Contents/jre/jdk/Contents/Home"
NDKINSTALLPATH="$STUDIO_SDK_PATH/ndk/${NDK_VERSION}"
PLATFORMTOOLS="$STUDIO_SDK_PATH/platform-tools:$STUDIO_SDK_PATH/build-tools/${BUILDTOOLS_VERSION}:$STUDIO_SDK_PATH/tools/bin"
adbPath=$(type -P "adb")
SDKMANAGERPATH="$STUDIO_SDK_PATH/cmdline-tools/latest/bin"
if [ ! -d "$SDKMANAGERPATH" ]; then
	SDKMANAGERPATH="$STUDIO_SDK_PATH/tools/bin"
	if [ ! -d "$SDKMANAGERPATH" ]; then
		echo Unable to locate sdkmanager. Did you run Android Studio and install cmdline-tools after installing?
		${PAUSE}
		exit 1
	fi
fi

"$SDKMANAGERPATH/sdkmanager" "platform-tools" "platforms;${PLATFORMS_VERSION}" "build-tools;${BUILDTOOLS_VERSION}" "cmake;${CMAKE_VERSION}" "ndk;${NDK_VERSION}"

retVal=$?
if [ $retVal -ne 0 ]; then
	echo Update failed. Please check the Android Studio install.
	${PAUSE}
	exit $retVal
fi

if [ ! -d "$STUDIO_SDK_PATH/platform-tools" ]; then
	retVal=1
fi
if [ ! -d "$STUDIO_SDK_PATH/platforms/${PLATFORMS_VERSION}" ]; then
	retVal=1
fi
if [ ! -d "$STUDIO_SDK_PATH/build-tools/${BUILDTOOLS_VERSION}" ]; then
	retVal=1
fi
if [ ! -d "$NDKINSTALLPATH" ]; then
	retVal=1
fi

if [ $retVal -ne 0 ]; then
	echo Update failed. Did you accept the license agreement?
	${PAUSE}
	exit $retVal
fi

# on zsh, ~/.bash_profile may not exist
if [ ! -f ~/.bash_profile ]; then
	echo > ~/.bash_profile
fi
 
updateProfileFileFunc ~/.bash_profile
# on zsh, update .zprofile and create .bash_profile (as UE reads it)
if [ $SHELL == "/bin/zsh" ]; then
	updateProfileFileFunc ~/.zprofile
	echo Adding UE Android dev env variables to .zprofile
fi

echo Success!

exit 0