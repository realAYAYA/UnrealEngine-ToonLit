// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationWindow : ModuleRules
	{
		public AutomationWindow(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"AutomationController",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "DesktopPlatform",
                    "InputCore",
					"ApplicationCore",
                    "Slate",
                    "SlateCore",
                    "ToolWidgets",
                    "CoreUObject",
                    "Json",
                    "JsonUtilities",
					"AutomationTest"
				}
			);

            // Added more direct dependencies to the editor for testing functionality
            if (Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
						"EditorFramework",
                        "UnrealEd",
						"Kismet",
						"Engine", // Needed for UWorld/GWorld to find current level
				    }
                );
            }

            PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SessionServices",
				}
			);
		}
	}
}
