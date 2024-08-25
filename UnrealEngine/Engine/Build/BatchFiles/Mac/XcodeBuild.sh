#!/bin/sh

# This script gets called every time Xcode does a build or clean operation. It is similar to Build.sh
# (and can take the same arguments) but performs some interpretation of arguments that come from Xcode
# Values for $ACTION: "" = building, "clean" = cleaning

if [ ["$UE_SKIP_UBT"] == ["1"] ]; then
	echo Skipping UBT per request
	exit 0
fi

echo Running Xcodebuild $@

# Setup Environment
source  Engine/Build/BatchFiles/Mac/SetupEnvironment.sh -dotnet Engine/Build/BatchFiles/Mac

#echo "Raw Args: $*"

BUILD_UBT=1

case $1 in 
	"clean")
		ACTION="clean"
	;;

	"install")
		ACTION="install"
	;;	

	"-nobuildubt")
		BUILD_UBT=0
	;;
esac


# If this is a source drop of the engine make sure that the UnrealBuildTool is up-to-date
if [ $BUILD_UBT == 1 ]; then
	# remove environment variable passed from xcode which also has meaning to dotnet, breaking the build
	unset TARGETNAME
	
	if [ ! -f Engine/Build/InstalledBuild.txt ]; then
		dotnet build Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj -c Development -v quiet

		if [ $? -ne 0 ]; then
			echo "Failed to build the build tool (UnrealBuildTool)"
			exit 1
		fi
	fi
fi



if [ ["$ACTION"] == [""] ]; then
	ACTION="build"
	TARGET=$1
	PLATFORM=$2
	CONFIGURATION=$3
	TRAILINGARGS=${@:4}
else
	# non build actions are all shifted by one
	TARGET=$2
	PLATFORM=$3
	CONFIGURATION=$4
	TRAILINGARGS=${@:5}
fi

if [[ $ARCHS ]]; then
	# convert the space in xcode's multiple architecture (arm64 x86_64) argument into the standard + that UBT expects (arm64+x86_64)
	UBT_ARCHFLAG="-architecture=${ARCHS/ /+}" 
else
	UBT_ARCHFLAG=""
fi

# Convert platform to UBT terms
case $PLATFORM in
	"iphoneos"|"IOS")
		PLATFORM="IOS"
	;;
	"iphonesimulator"|"iossimulator")
		PLATFORM="IOS"
		UBT_ARCHFLAG="-architecture=iossimulator" 
	;;
	"appletvos")
		PLATFORM="TVOS"
	;;
	"xros")
		PLATFORM="VisionOS"
	;;
	"xrsimulator")
		PLATFORM="VisionOS"
		UBT_ARCHFLAG="-architecture=iossimulator" 
	;;
	"macosx")
		PLATFORM="Mac"
	;;
esac

echo "Processing $ACTION for Target=$TARGET Platform=$PLATFORM Configuration=$CONFIGURATION $UBT_ARCHFLAG $TRAILINGARGS "

# Add additional flags based on actions, arguments, and env properties
AdditionalFlags=""

if [ "$ACTION" == "build" ]; then

	# flags based on platform
	case $PLATFORM in 
		"IOS")
			AdditionalFlags="${AdditionalFlags} -deploy"
		;;

		"TVOS")
			AdditionalFlags="${AdditionalFlags} -deploy"
		;;
	esac

	case $CLANG_STATIC_ANALYZER_MODE in
		"deep")
			AdditionalFlags="${AdditionalFlags} -SkipActionHistory"
			;;
		"shallow")
			AdditionalFlags="${AdditionalFlags} -SkipActionHistory"
			;;
	esac

	case $ENABLE_THREAD_SANITIZER in
		"YES"|"1")
			# Disable TSAN atomic->non-atomic race reporting as we aren't C++11 memory-model conformant so UHT will fail
			export TSAN_OPTIONS="suppress_equal_stacks=true suppress_equal_addresses=true report_atomic_races=false"
		;;
	esac

	# Build SCW if this is an editor target
	if [[ "$TARGET" == *"Editor" ]]; then
		dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll ShaderCompileWorker Mac Development $UBT_ARCHFLAG
	fi

elif [ $ACTION == "clean" ]; then
	AdditionalFlags="-clean"
	
elif [ $ACTION == "metadata" ]; then
	AdditionalFlags="-mode=ExportXcodeMetadata"
fi

echo Running dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll $TARGET $PLATFORM $CONFIGURATION "$TRAILINGARGS" $UBT_ARCHFLAG $AdditionalFlags
# set an envvar to let UBT know that it's being run from xcode (envvar allows children to get the setting if needed)
UE_BUILD_FROM_XCODE=1 dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll $TARGET $PLATFORM $CONFIGURATION "$TRAILINGARGS" $UBT_ARCHFLAG $AdditionalFlags

ExitCode=$?
if [ $ExitCode -eq 254 ] || [ $ExitCode -eq 255 ] || [ $ExitCode -eq 2 ]; then
	exit 0
fi
exit $ExitCode
