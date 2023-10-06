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
					"AutomationMessages",
					"Core",
					"CoreUObject",
					"Engine",
					"Json",
					"JsonUtilities",
					"ScreenShotComparisonTools",
				}
			);
		}
	}
}
