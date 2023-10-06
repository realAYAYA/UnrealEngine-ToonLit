// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64", "Linux")]
public class NetworkPredictionTestsTarget : TestTargetRules
{
	public NetworkPredictionTestsTarget(TargetInfo Target) : base(Target)
	{
		bCompileAgainstEngine = true;
		bCompileAgainstApplicationCore = true;
		bUsesSlate = false;

		bUsePlatformFileStub = true;
		bMockEngineDefaults = true;

		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}
