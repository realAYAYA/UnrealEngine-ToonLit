// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WaveFunctionCollapse : ModuleRules
{
	public WaveFunctionCollapse(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "WFC";

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EditorSubsystem",
				"EditorStyle",
				"LevelEditor",
				"EditorFramework",
				"UnrealEd",
				"Blutility",
				"UMG",
				"UMGEditor",
				"AssetTools",
				"PropertyEditor",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
			}
		);
	}
}
