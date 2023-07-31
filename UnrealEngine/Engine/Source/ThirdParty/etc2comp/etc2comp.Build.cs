// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class etc2comp : ModuleRules
{
	public etc2comp(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_ETC2COMP=1");
		
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "EtcLib", "Etc"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "EtcLib", "EtcCodec"));
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "Release", "EtcLib.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Mac", "Release", "libEtcLib.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Linux", "Release", "libEtcLib.a"));
		}
	}
}

