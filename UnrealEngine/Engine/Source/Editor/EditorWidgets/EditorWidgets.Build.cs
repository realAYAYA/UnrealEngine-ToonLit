// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorWidgets : ModuleRules
{
	public EditorWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("AssetRegistry");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"CoreUObject",
				"ToolWidgets",
				"EditorConfig",
				"AssetDefinition",
			}
		);
		
		DynamicallyLoadedModuleNames.Add("AssetRegistry");
	}
}
