// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ColorCorrectRegionsEditor : ModuleRules
{
	public ColorCorrectRegionsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ActorPickerMode",
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"ColorCorrectRegions",
				"PlacementMode",
				"SceneOutliner",
			}
		);
	}
}
