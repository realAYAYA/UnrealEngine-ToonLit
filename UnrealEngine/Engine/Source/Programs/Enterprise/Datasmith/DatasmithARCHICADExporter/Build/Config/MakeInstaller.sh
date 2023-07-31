#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

# Get directory of the command file
ScriptPath="`dirname "$0"`"

UE4RootPath=$ScriptPath/../../../../../../../..
UE4RootPath=`python -c "import os; print(os.path.realpath('$UE4RootPath'))"`

echo "Build DatasmithUE4ArchiCAD"
xcodebuild -workspace "$UE4RootPath/UE4.xcworkspace" -scheme DatasmithUE4ArchiCAD -configuration Development build -quiet
"$ScriptPath/setupdylibs.sh"

echo "Build DatasmithARCHICADExporter"
xcodebuild -project "$ScriptPath/../DatasmithARCHICADExporter.xcodeproj" -scheme BuildAllAddOns -configuration Release build -quiet

DeveloperId=$1
 if [ "$DeveloperId" = "" ]; then
	# Use user environment variable to get signing id
	DeveloperId=$UE_DatasmithArchicadDeveloperId
fi

"$ScriptPath/../../Installer/MacOS/SignAndBuild.sh" "$DeveloperId"

echo "Done"
