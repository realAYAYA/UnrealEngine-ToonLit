// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaStorageProxyTarget : TargetRules
{
	public UbaStorageProxyTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "UbaStorageProxy";
		UbaAgentTarget.CommonUbaSettings(this, Target);
	}
}
