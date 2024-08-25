// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class LibJpegTurbo : ModuleRules
{
	protected readonly string Version = "3.0.0";

	protected string VersionPath { get => Path.Combine(ModuleDirectory, Version); }
	protected string LibraryPath { get => Path.Combine(VersionPath, "lib"); }

	public LibJpegTurbo(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", "Release", "turbojpeg-static.lib"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Unix", Target.Architecture.LinuxName, "Release", "libturbojpeg.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libturbojpeg.a"));
		}
	}
}
