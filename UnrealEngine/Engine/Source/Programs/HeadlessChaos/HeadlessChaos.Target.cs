// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class HeadlessChaosTarget : TestTargetRules
{
	public HeadlessChaosTarget(TargetInfo Target) : base(Target)
	{
		ExeBinariesSubFolder = LaunchModuleName = "HeadlessChaos";

		// HeadlessChaos doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;

        bHasExports = false;

        bUseLoggingInShipping = true;

        // UnrealHeaderTool is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
        bIsBuildingConsoleApplication = true;

		GlobalDefinitions.Add("CHAOS_SERIALIZE_OUT=1");

		// our gtest does not have debug crt libs
		bDebugBuildsActuallyUseDebugCRT = false;

		// Enable additional debug functionality in the constraint management system to allow better unit testing
		GlobalDefinitions.Add("CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED=1");
		BuildEnvironment = TargetBuildEnvironment.Unique;
	}
}
