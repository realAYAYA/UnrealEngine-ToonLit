// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class WebTestsTarget : TestTargetRules
{
	public WebTestsTarget(TargetInfo Target) : base(Target)
	{
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstApplicationCore = true;
	}
}
