// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandmassEditor : ModuleRules
{
	public LandmassEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateIncludePaths.Add("Editor/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
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