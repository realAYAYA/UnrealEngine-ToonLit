// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEEditor : ModuleRules
{
	public NNEEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetTools",
				"NNE"
			}
		);
	}
}
