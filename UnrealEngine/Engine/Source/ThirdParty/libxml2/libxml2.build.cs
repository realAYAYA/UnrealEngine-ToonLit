// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libxml2 : ModuleRules
{
	const string CurrentLibxml2Version = "libxml2-2.9.10";
	
	public libxml2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string xml2Path = Path.Combine(Target.UEThirdPartySourceDirectory, "libxml2", CurrentLibxml2Version);

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
				PublicIncludePaths.Add(Path.Combine(xml2Path, "include"));
				PublicAdditionalLibraries.Add(Path.Combine(xml2Path, "lib", Target.Architecture, "libxml2.a"));
			}
		}
	}
}

