// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class EventLoopUnitTestsTarget : TestTargetRules
{
	public EventLoopUnitTestsTarget(TargetInfo Target) : base(Target)
	{
		bCompileAgainstCoreUObject = true;
		bCompileAgainstApplicationCore = true;

		// Optionally add global definitions for Catch2 benchmarking etc.
		GlobalDefinitions.Add("CATCH_CONFIG_ENABLE_BENCHMARKING=1");
	}
}