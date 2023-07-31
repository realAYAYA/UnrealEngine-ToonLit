// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class LibJpegTurbo : ModuleRules
{
	public LibJpegTurbo(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IncPath = Path.Combine(ModuleDirectory, "include");
		PublicSystemIncludePaths.Add(IncPath);

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib/Win64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "turbojpeg-static.lib"));
			PublicSystemIncludePaths.Add(Path.Combine(IncPath, "Win64"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibPath = Path.Combine(ModuleDirectory, "lib/Unix", Target.Architecture);

			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libturbojpegd.a"));
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libturbojpeg.a"));
			}

			PublicSystemIncludePaths.Add(Path.Combine(IncPath, "Unix", Target.Architecture));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPathMac = Path.Combine(ModuleDirectory, "lib", Target.Platform.ToString(), "libturbojpeg.a");
			string IncPathMac = Path.Combine(IncPath,                Target.Platform.ToString());

			PublicAdditionalLibraries.Add(LibPathMac);
			PublicSystemIncludePaths.Add(IncPathMac);
		}
	}
}
