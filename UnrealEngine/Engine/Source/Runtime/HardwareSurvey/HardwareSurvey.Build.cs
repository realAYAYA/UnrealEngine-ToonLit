// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HardwareSurvey : ModuleRules
{
	public HardwareSurvey(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ApplicationCore"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"Analytics",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
			}
		);

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}
