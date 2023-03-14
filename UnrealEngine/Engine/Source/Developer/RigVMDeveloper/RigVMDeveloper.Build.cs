// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RigVMDeveloper : ModuleRules
{
    public RigVMDeveloper(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "RigVM",
                "VisualGraphUtils",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"EditorFramework",
                    "UnrealEd",
					"Slate",
					"SlateCore",
					
					"MessageLog",
					"BlueprintGraph"
				}
			);
        }
    }
}
