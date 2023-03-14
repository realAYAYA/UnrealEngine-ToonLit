// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationUtilsEditor : ModuleRules
	{
		public AutomationUtilsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ScreenShotComparisonTools",
					"DesktopPlatform",
					"AutomationMessages",
					"Json",
					"JsonUtilities"
				}
			);
		}
	}
}
