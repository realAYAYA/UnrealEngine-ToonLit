// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class UbaVisualizerTarget : TargetRules
{
	public UbaVisualizerTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaVisualizer";
		UbaAgentTarget.CommonUbaSettings(this, Target);
		bIsBuildingConsoleApplication = false;
	}
}
