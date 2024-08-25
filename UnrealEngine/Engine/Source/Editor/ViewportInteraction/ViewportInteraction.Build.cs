// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class ViewportInteraction : ModuleRules
    {
        public ViewportInteraction(ReadOnlyTargetRules Target) : base(Target)
        {
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
					"RenderCore",
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
