// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimedDataMonitor : ModuleRules
{
	public TimedDataMonitor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"StageDataCore",
				"TimeManagement",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
			});
	}
}
