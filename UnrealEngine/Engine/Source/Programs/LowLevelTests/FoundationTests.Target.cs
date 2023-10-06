// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class FoundationTestsTarget : TestTargetRules
{
	public FoundationTestsTarget(TargetInfo Target) : base(Target)
	{
		// Collects all tests decorated with #if WITH_LOW_LEVELTESTS from dependencies
		bWithLowLevelTestsOverride = true;

		bBuildWithEditorOnlyData = Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop)
			&& (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.Development);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string VersionScriptFile = Path.GetTempPath() + "LLTWorkaroundScrip.ldscript";
			using (StreamWriter Writer = File.CreateText(VersionScriptFile))
			{
				// Workaround for a linker bug when building LowLevelTests for Android
				Writer.WriteLine("{ local: *; };");
			}
			AdditionalLinkerArguments = " -Wl,--version-script=\"" + VersionScriptFile + "\"";
		}
	}
}
