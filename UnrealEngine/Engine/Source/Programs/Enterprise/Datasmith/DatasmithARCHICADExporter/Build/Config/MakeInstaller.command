#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

# Get directory of the command file
ScriptPath="`dirname "$0"`"

"$ScriptPath/MakeInstaller.sh"

open "$ScriptPath/../../../../../../../../Engine/Binaries/Mac/DatasmithArchiCADExporter"
