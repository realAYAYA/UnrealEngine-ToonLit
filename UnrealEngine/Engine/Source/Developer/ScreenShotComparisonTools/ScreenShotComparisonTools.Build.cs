// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparisonTools : ModuleRules
{
	public ScreenShotComparisonTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AutomationMessages",
				
				"ImageWrapper",
				"Json",
				"JsonUtilities",
				"Slate",
				"UnrealEdMessages",
				"DesktopPlatform"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MessagingCommon",
			}
		);

		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
