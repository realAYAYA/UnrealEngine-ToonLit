// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using UnrealBuildTool;

public class SessionFrontend : ModuleRules
{
	public SessionFrontend(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"ApplicationCore",
				"InputCore",
				"Json",
				"SessionServices",
				"SlateCore",

				// @todo gmp: remove these dependencies by making the session front-end extensible
				"AutomationWindow",
				"ScreenShotComparison",
				"ScreenShotComparisonTools",
				"TargetPlatform",
				"WorkspaceMenuStructure",
			}
		);

		if (Target.GlobalDefinitions.Contains("UE_DEPRECATED_PROFILER_ENABLED=1"))
		{
			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.Add("Profiler");
			}
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
				"TargetDeviceServices",
			}
		);
	}
}
