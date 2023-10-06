// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorLayerUtilitiesEditor : ModuleRules
{
	public ActorLayerUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorLayerUtilities",
				"Core",
				"CoreUObject",
				"EditorWidgets",
				"Engine",
				"Layers",
				"LevelEditor",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"UnrealEd",
				"EditorStyle",
			}
		);
	}
}
