// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DistributedBuildInterface : ModuleRules
{
	public DistributedBuildInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Public"));
	}
}
