// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureAlignMode : ModuleRules
{
	public TextureAlignMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"Slate",
				"EditorFramework",
				"UnrealEd",
				"RenderCore",
				"LevelEditor",
				"GeometryMode",
                "BspMode",
            }
		);
	}
}
