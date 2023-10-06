// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class WMFCodecs : ModuleRules
{
	public WMFCodecs(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Engine",
			"AVCodecsCore",
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"RenderCore",
			"Core",
		});
		
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicDelayLoadDLLs.Add("mfplat.dll");
			PublicDelayLoadDLLs.Add("mfuuid.dll");
			PublicDelayLoadDLLs.Add("mfreadwrite.dll");
		}
	}
}
