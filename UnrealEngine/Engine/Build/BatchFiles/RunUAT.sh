#!/bin/bash
## Unreal Engine AutomationTool setup script	
## Copyright Epic Games, Inc. All Rights Reserved.
##
## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work
## correctly if you copy it to a different location and run it.

echo
echo Running AutomationTool...
echo

GetAllChildProcesses() {
	local Children=$(ps -o pid= --ppid "$1")

	for PID in $Children
	do
		GetAllChildProcesses "$PID"
	done

	echo "$Children"
}

# Gather all the descendant children of this process, and first kill -TERM. If any child process
# is still alive finally send a -KILL
TermHandler() {
	MaxWait=30
	CurrentWait=0

	ProcessesToKill=$(GetAllChildProcesses $$)
	kill -s TERM $ProcessesToKill 2> /dev/null

	ProcessesStillAllive=$(ps -o pid= -p $ProcessesToKill)

	# Wait until all the processes have been gracefully killed, or max Wait time
	while [ -n "$ProcessesStillAllive" ] && [ "$CurrentWait" -lt "$MaxWait" ]
	do
		CurrentWait=$((CurrentWait + 1))
		sleep 1

		ProcessesStillAllive=$(ps -o pid= -p $ProcessesToKill)
	done

	# If some processes are still alive after MaxWait, lets just force kill them
	if [ -n "$ProcessesStillAllive" ]; then
		kill -s KILL $ProcessesStillAllive 2> /dev/null
	fi
}

# put ourselves into Engine directory (two up from location of this script)
SCRIPT_DIR=$(cd "`dirname "$0"`" && pwd)
cd "$SCRIPT_DIR/../.."

UATCompileArg=-compile

if [ ! -f Build/BatchFiles/RunUAT.sh ]; then
	echo "RunUAT ERROR: The script does not appear to be located in the "
	echo "Engine/Build/BatchFiles directory.  This script must be run from within that directory."
	exit 1
fi

# see if we have the no compile arg
if echo "$@" | grep -q -w -i "\-nocompile"; then
	UATCompileArg=
else
	UATCompileArg=-compile
fi

# control toggling of msbuild verbosity for easier debugging
if echo "$@" | grep -q -w -i "\-msbuild-verbose"; then
	MSBuild_Verbosity=normal
else
	MSBuild_Verbosity=quiet
fi

if [ -f Build/InstalledBuild.txt ]; then
	UATCompileArg=
fi

EnvironmentType=-dotnet
UATDirectory=Binaries/DotNET/AutomationTool

if [ "$(uname)" = "Darwin" ]; then
	# Setup Environment
	source "$SCRIPT_DIR/Mac/SetupEnvironment.sh" $EnvironmentType "$SCRIPT_DIR/Mac"
fi

if [ "$(uname)" = "Linux" ]; then
	# Setup Environment
	source "$SCRIPT_DIR/Linux/SetupEnvironment.sh" $EnvironmentType "$SCRIPT_DIR/Linux"
fi

if [ "$UATCompileArg" = "-compile" ]; then
  # see if the .csproj exists to be compiled
	if [ ! -f Source/Programs/AutomationTool/AutomationTool.csproj ]; then
		echo No project to compile, attempting to use precompiled AutomationTool
		UATCompileArg=
	else
		"$SCRIPT_DIR"/BuildUAT.sh $MSBuild_Verbosity $FORCECOMPILE_UAT
		if [ $? -ne 0 ]; then
			echo RunUAT ERROR: AutomationTool failed to compile.
			exit 1
		fi
	fi
fi

## Run AutomationTool

#run UAT
cd $UATDirectory
if [ -z "$uebp_LogFolder" ]; then
	LogDir="$HOME/Library/Logs/Unreal Engine/LocalBuildLogs"
else
	LogDir="$uebp_LogFolder"
fi

# if we are running under UE, we need to run this with the term handler (otherwise canceling a UAT job from the editor
# can leave dotnet, etc running in the background, which means we need the PID so we 
# run it in the background
if [ "$UE_DesktopUnrealProcess" = "1" ]; then
	# you can't set a dotted env var nicely in sh, but env will run a command with
	# a list of env vars set, including dotted ones
	echo Start UAT Non-Interactively: dotnet AutomationTool.dll "$@"
	trap TermHandler SIGTERM SIGINT
	env uebp_LogFolder="$LogDir" dotnet AutomationTool.dll "$@" &
	UATPid=$!
	wait $UATPid
else
	# you can't set a dotted env var nicely in sh, but env will run a command with
	# a list of env vars set, including dotted ones
	echo Start UAT Interactively: dotnet AutomationTool.dll "$@"
	which dotnet
	env uebp_LogFolder="$LogDir" dotnet AutomationTool.dll "$@"
fi

UATReturn=$?

# @todo: Copy log files to somewhere useful
# if not "%uebp_LogFolder%" == "" copy log*.txt %uebp_LogFolder%\UAT_*.*
# if "%uebp_LogFolder%" == "" copy log*.txt c:\LocalBuildLogs\UAT_*.*
#cp log*.txt /var/log

if [ $UATReturn -ne 0 ]; then
	echo RunUAT ERROR: AutomationTool was unable to run successfully. Exited with code: $UATReturn
	exit $UATReturn
fi
