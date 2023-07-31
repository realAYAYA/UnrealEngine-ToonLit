// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BootstrapPackagedGame : ModuleRules
{
	public BootstrapPackagedGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicSystemLibraries.Add("shlwapi.lib");
		bEnableUndefinedIdentifierWarnings = false;
	}
}
