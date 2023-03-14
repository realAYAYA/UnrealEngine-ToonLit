// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class FontConfig : ModuleRules
{
	protected virtual string ThirdPartyDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string FontConfigVersion { get { return "fontconfig-2.13.94"; } }

	public FontConfig(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Path.Combine(ThirdPartyDirectory, "FontConfig", FontConfigVersion, "include", "fontconfig"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyDirectory, "FontConfig", FontConfigVersion, "lib", "Unix", "x86_64-unknown-linux-gpu", "libfontconfig.a"));
		}
	}
}
