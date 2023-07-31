// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LogVisualizer : ModuleRules
{
	public LogVisualizer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
				"LevelEditor"
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Engine/Classes",
				"Editor/WorkspaceMenuStructure/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Json",
				"Slate",
				"SlateCore",
				
				"Engine",
				"EditorFramework",
				"UnrealEd",
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
