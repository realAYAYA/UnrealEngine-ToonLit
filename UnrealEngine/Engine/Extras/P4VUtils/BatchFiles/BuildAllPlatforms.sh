#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

cd "`dirname "$0"`/.."

if [ "$(uname)" = "Darwin" ]; then
	source ../../Build/BatchFiles/Mac/SetupDotnet.sh
else
	source ../../Build/BatchFiles/Linux/SetupDotnet.sh
fi

rm -rf bin
rm -rf obj

# Compile with defualt WithRestricted value, which may be true
dotnet publish P4VUtils.csproj -p:IsWindows=true -p:IsOSX=false -p:IsLinux=false -p:EnableWindowsTargeting=true
dotnet publish P4VUtils.csproj -p:IsWindows=false -p:IsOSX=true -p:IsLinux=false
dotnet publish P4VUtils.csproj -p:IsWindows=false -p:IsOSX=false -p:IsLinux=true

# Now compile with WithRestricted of false, which may compile the same as above, or differently
dotnet publish P4VUtils.csproj -p:IsWindows=true -p:IsOSX=false -p:IsLinux=false -p:WithRestricted=false -p:EnableWindowsTargeting=true
dotnet publish P4VUtils.csproj -p:IsWindows=false -p:IsOSX=true -p:IsLinux=false -p:WithRestricted=false
dotnet publish P4VUtils.csproj -p:IsWindows=false -p:IsOSX=false -p:IsLinux=true -p:WithRestricted=false
