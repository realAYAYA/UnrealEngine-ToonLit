// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class ChaosUserDataPTTestsTarget : TestTargetRules
{
	public ChaosUserDataPTTestsTarget(TargetInfo Target) : base(Target)
	{
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstApplicationCore = true;
	}
}
