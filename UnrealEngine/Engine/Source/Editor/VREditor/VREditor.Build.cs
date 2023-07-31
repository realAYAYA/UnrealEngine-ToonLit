// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VREditor : ModuleRules
	{
        public VREditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					System.IO.Path.Combine(GetModuleDirectory("LevelEditor"), "Private"),
				}
			);

			PublicIncludePaths.Add(ModuleDirectory);

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "AppFramework",
				    "Core",
				    "CoreUObject",
					"ApplicationCore",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    
					"EditorFramework",
					"UnrealEd",
					"UMG",
					"LevelEditor",
					"HeadMountedDisplay",
					"Analytics",
                    "LevelSequence",
                    "Sequencer",
                    "Projects",
					"ToolMenus",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"ViewportInteraction"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"LevelEditor",
					"PlacementMode",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
                    "PlacementMode"
                }
			);
		}
	}
}