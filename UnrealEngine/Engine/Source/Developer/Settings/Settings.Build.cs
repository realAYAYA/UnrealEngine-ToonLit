// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Settings : ModuleRules
{
	public Settings(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
			});

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}
