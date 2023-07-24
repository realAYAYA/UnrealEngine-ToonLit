# AutoSDK

## Introduction

The AutoSDK system provides a mechanism for distributing target platform SDKs and configuring them for use by the engine on demand.

It was designed for build machines to serve multiple branches with different SDK requirements without needing to manually manage installed packages, but may also be used by developers that don't require a full SDK install. Only a minimal toolset is typically available by default (compiler, deployment software, etc..).

UnrealBuildTool, AutomationTool and the Unreal Editor are all designed to work seamlessly with AutoSDK. The switching between SDKs is handled by UnrealBuildTool, which is invoked by the other tools. 

For any engine verison, UnrealBuildTool has a preferred SDK version that it will attempt to use. These versions are typically defined by classes derived from UEBuildPlatform in the UBT source code.

## Setup

Epic is not able to distribute SDKs for legal reasons, so this directory tree serves a template showing how to structure it. We recommend you start with an empty AutoSDK folder and copy files from here as you add a particular SDK version.

**Do not configure the AutoSDK system to use this folder. It just a template, and the system will not work without adding additional files.**

The AutoSDK folder should be submitted to source control separately to the game or engine code, so that multiple branches on a single machine can share it. 

Any developer wishing to use AutoSDK can sync it and set the `UE_SDKS_ROOT` environment variable to point to the path on their local machine for UBT which contains it.

## Layout

The AutoSDK directory structure is loosely defined and generally left to the discretion of each platform, but generally follows this pattern.

    /HostPlatform/                                 --- Host platform that the SDKs can be used on.
    /HostPlatform/TargetPlatform/                  --- Directory containing the "TargetPlatform" SDKs for "HostPlatform" (eg. "/HostWin64/Android/").
	/HostPlatform/TargetPlatform/1.0/              --- Directory containing the "TargetPlatform" 1.0 SDK for "HostPlatform".
	/HostPlatform/TargetPlatform/1.0/Setup.bat     --- Optional batch file that is run when setting up this SDK. Named "setup.sh" on Mac/Linux.
	/HostPlatform/TargetPlatform/1.0/Unsetup.bat   --- Optional batch file that is run when removing this SDK. Named "unsetup.sh" on Mac/Linux.

When run, Setup.bat will output a text file to the same directory called OutputEnvVars.txt, containing a list of environment variables to set in the form NAME=VALUE, as well as special directives to modify the PATH environment variable such as ADDPATH=Foo and STRIPPATH=Foo.

Setup scripts written by Epic for supported SDK versions are included under this directory.

## Platforms

More information about adding SDKs for each platform is given by README.md files in HostPlatform/TargetPlatform subfolders. For PlatformExtension platforms, you will need to merge AutoSDKs files from Engine/Platforms/<Platform>/Extras/AutoSDK into your UE_SDKS_ROOT