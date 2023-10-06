// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ToolPresetAsset: ModuleRules
{
	public ToolPresetAsset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",				
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetDefinition",
				"CoreUObject",
				"EditorConfig",
				"EditorSubsystem",
				"Engine",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",
				"UnrealEd",
			});
	}
}
