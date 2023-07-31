// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CompositePlane : ModuleRules
{
	public CompositePlane(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
				"Engine",
                "Projects",
                "Slate",
				"SlateCore",
			}
			);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
						"EditorFramework",
                        "UnrealEd",
                        "PlacementMode",
                }
            );
        }
	}
}