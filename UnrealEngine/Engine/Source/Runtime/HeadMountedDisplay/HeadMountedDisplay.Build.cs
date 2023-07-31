// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class HeadMountedDisplay : ModuleRules
{
    public HeadMountedDisplay(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Renderer/Private"
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "RHI",
                "Renderer",
                "RenderCore",
                "Analytics",
                "EngineSettings",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "AugmentedReality",
            }
        );

        if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"UnrealEd"
				});
		}
	}
}
