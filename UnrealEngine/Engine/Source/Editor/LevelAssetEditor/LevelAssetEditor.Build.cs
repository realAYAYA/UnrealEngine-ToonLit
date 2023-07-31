// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelAssetEditor : ModuleRules
{
	public LevelAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EditorInteractiveToolsFramework",
				"InteractiveToolsFramework",
				"ToolMenus",
				"TypedElementFramework",
				// ... add other public dependencies that you statically link with here ...
			}
			);			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
