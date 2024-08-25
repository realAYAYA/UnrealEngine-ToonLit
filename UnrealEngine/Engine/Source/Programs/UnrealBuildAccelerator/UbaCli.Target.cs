// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaCliTarget : TargetRules
{
	public UbaCliTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaCli";
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
