// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ColorCorrectRegionsEditor : ModuleRules
{
	public ColorCorrectRegionsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"UnrealEd",
				"ColorCorrectRegions",
				"Slate",
				"SlateCore",
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"ColorCorrectRegions",
				"PlacementMode",
			}
		);
	}
}
