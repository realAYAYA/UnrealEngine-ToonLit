// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class ViewportInteraction : ModuleRules
    {
        public ViewportInteraction(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateIncludePaths.Add(ModuleDirectory);

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
					"EditorFramework",
                    "UnrealEd",
                    "Slate",
                    "SlateCore",
                    "RHI"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "LevelEditor"
                }
            );
        }
    }
}
