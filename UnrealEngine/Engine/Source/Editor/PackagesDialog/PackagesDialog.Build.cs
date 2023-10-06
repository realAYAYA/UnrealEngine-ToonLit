// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PackagesDialog : ModuleRules
{
	public PackagesDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("AssetTools");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Slate", 
				"SlateCore",
				"UnrealEd",
				"SourceControl",
				"AssetRegistry",
				"ToolWidgets",
			}
		);

		DynamicallyLoadedModuleNames.Add("AssetTools");
	}
}
