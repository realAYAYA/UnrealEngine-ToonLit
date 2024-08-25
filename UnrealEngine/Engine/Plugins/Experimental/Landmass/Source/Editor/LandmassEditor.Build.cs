// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandmassEditor : ModuleRules
{
	public LandmassEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Landscape",
				"Engine",
				"LevelEditor",
				"UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
			}
		);
	}
}