// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PackagesDialog : ModuleRules
{
	public PackagesDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("AssetTools");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"EditorFramework",
				"Engine", 
                "InputCore",
				"Slate", 
				"SlateCore",
				"UnrealEd",
				"SourceControl",
				"EditorWidgets",
				"AssetRegistry",
				"ToolWidgets",
			}
		);

		DynamicallyLoadedModuleNames.Add("AssetTools");
	}
}
