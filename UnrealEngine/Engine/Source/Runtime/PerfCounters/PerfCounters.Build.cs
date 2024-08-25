// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PerfCounters : ModuleRules
{
	public PerfCounters(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Json",
				"HTTPServer"
			}
		);
	}
}
