// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class Itoo : ModuleRules
{
    public Itoo(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(ModuleDirectory);
	}
}

