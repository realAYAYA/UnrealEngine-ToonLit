// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class LowLevelTestsTarget : TestTargetRules
{
	public LowLevelTestsTarget(TargetInfo Target) : base(Target)
	{
		bWithLowLevelTestsOverride = true;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

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
