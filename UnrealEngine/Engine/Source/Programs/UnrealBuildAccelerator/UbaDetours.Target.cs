// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaDetoursTarget : TargetRules
{
	public UbaDetoursTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaDetours";
		bShouldCompileAsDLL = true;
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
