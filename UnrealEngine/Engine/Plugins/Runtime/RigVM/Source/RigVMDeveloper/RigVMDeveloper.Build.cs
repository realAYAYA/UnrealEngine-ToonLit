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
                "KismetCompiler",
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
					"BlueprintGraph",
					"GraphEditor",
					"Kismet",
                }
			);
            
            PrivateIncludePathModuleNames.Add("RigVMEditor");
            DynamicallyLoadedModuleNames.Add("RigVMEditor");
        }
    }
}
