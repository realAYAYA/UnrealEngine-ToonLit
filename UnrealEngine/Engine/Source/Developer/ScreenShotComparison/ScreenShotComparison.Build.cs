// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparison : ModuleRules
{
	public ScreenShotComparison(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AutomationController",
				"ToolWidgets",
				"InputCore",
				"ScreenShotComparisonTools",
				"Slate",
				"SlateCore",
				"ImageWrapper",
				"CoreUObject",
				"DesktopWidgets",
				"SourceControl",
				"AutomationMessages",
				"Json",
				"JsonUtilities",
				"DirectoryWatcher"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SessionServices",
			}
		);
	}
}
