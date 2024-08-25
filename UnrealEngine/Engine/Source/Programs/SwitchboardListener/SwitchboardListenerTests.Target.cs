// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


[SupportedPlatforms("Win64", "Linux")]
public class SwitchboardListenerTestsTarget : TestTargetRules
{
	public SwitchboardListenerTestsTarget(TargetInfo Target) : base(Target)
	{
		SolutionDirectory = "Programs/Switchboard";
		LaunchModuleName = "SwitchboardListenerTests";

		// Don't pull in LaunchEngineLoop
		GlobalDefinitions.Add("SWITCHBOARD_LISTENER_EXCLUDE_MAIN=1");

		ExtraModuleNames.Add(LaunchModuleName);
	}
}
