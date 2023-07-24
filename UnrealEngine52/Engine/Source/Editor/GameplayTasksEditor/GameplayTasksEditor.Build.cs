// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayTasksEditor : ModuleRules
	{
        public GameplayTasksEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			OverridePackageType = PackageOverrideType.EngineDeveloper;

			PrivateIncludePaths.AddRange(
                new string[] {
					System.IO.Path.Combine(GetModuleDirectory("AssetTools"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("GameplayTasksEditor"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("GraphEditor"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("Kismet"), "Private"),
				}
			);

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"ClassViewer",
                    "GameplayTags",
					"GameplayTasks",
                    "InputCore",
                    "PropertyEditor",
					"Slate",
					"SlateCore",
					"BlueprintGraph",
                    "Kismet",
					"KismetCompiler",
					"GraphEditor",
					"MainFrame",
					"EditorFramework",
					"UnrealEd",
                    "EditorWidgets",
				}
			);
		}
	}
}
