// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaTestAppTarget : TargetRules
{
	public UbaTestAppTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaTestApp";
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
