// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class LibTiff : ModuleRules
{
	public LibTiff(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bWithLibTiff = false;
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib", Target.Platform.ToString());
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "tiff.lib"));
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Source", Target.Platform.ToString()));
			bWithLibTiff = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib", Target.Platform.ToString());
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libtiff.a"));
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Source", Target.Platform.ToString()));
			bWithLibTiff = true;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib/Unix", Target.Architecture.LinuxName);
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libtiff.a"));
			string IncludePath = Path.Combine(ModuleDirectory, "Source/Unix", Target.Architecture.LinuxName);
			PublicSystemIncludePaths.Add(IncludePath);
			bWithLibTiff = true;
		}

		if (bWithLibTiff)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Source"));
			AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "LibJpegTurbo");
		}

		PublicDefinitions.Add("WITH_LIBTIFF=" + (bWithLibTiff ? '1' : '0'));
	}
}
