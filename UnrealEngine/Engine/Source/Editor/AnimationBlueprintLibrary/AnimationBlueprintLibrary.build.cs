// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationBlueprintLibrary : ModuleRules
{
	public AnimationBlueprintLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"Core",
				"CoreUObject",
				"Kismet",
				"AnimGraph",
				"UnrealEd",
				"TimeManagement"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimGraph",
				"KismetCompiler",
				"Engine",
				"BlueprintGraph",
				"UnrealEd",
			}
		);
	}
}
