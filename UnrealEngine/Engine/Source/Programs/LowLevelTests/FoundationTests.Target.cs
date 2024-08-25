// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class FoundationTestsTarget : TestTargetRules
{
	public FoundationTestsTarget(TargetInfo Target) : base(Target)
	{
		// Collects all tests decorated with #if WITH_LOW_LEVELTESTS from dependencies
		bWithLowLevelTestsOverride = true;
		bCompileWithPluginSupport = true;

		bBuildWithEditorOnlyData = Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop)
			&& (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.Development);
	}
}
