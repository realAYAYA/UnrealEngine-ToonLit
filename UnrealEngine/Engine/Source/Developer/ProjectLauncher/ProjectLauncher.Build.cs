// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectLauncher : ModuleRules
{
	public ProjectLauncher(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"LauncherServices",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"ApplicationCore",
                "InputCore",
				"Slate",
				"SlateCore",
                
                "WorkspaceMenuStructure",
				"ToolWidgets",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetDeviceServices",
			}
		);
	}
}
