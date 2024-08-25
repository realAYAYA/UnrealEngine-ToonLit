// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaTestTarget : TargetRules
{
	public UbaTestTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaTest";
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
