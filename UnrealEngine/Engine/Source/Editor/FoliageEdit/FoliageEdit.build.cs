// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FoliageEdit : ModuleRules
{
	public FoliageEdit(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"EditorFramework",
				"EditorSubsystem",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"RenderCore",
				"LevelEditor",
				"SceneOutliner",
				"Landscape",
                "PropertyEditor",
                "DetailCustomizations",
                "AssetTools",
                "Foliage",
				"DataLayerEditor",
				"EditorWidgets",
				"ToolWidgets",
			}
		);

	}
}
