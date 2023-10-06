// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetPlacementEdMode : ModuleRules
{
	public AssetPlacementEdMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                
				"EditorFramework",
				"EditorSubsystem",
				"UnrealEd",
				"EditorInteractiveToolsFramework",
				"InteractiveToolsFramework",
				"Foliage",
				"Landscape",
				"TypedElementFramework",
				"TypedElementRuntime",
				"PropertyEditor",
				"EditorWidgets",
			}
		);
	}
}
