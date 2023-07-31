// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebAPIOpenAPI : ModuleRules
{
	public WebAPIOpenAPI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"AssetTools",
				"Core",
				"CoreUObject",
				"EditorStyle",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"WebAPI",
				"WebAPIEditor"
			});
    }
}
