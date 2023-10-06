// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollisionAnalyzer : ModuleRules
{
	public CollisionAnalyzer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"DesktopPlatform",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                
				"Engine",
				"WorkspaceMenuStructure",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
        );
	}
}
