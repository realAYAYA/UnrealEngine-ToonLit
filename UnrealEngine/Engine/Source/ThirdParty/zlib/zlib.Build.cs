// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class zlib : ModuleRules
{
	protected readonly string Version = "1.3";
	protected string VersionPath { get => Path.Combine(ModuleDirectory, Version); }
	protected string LibraryPath { get => Path.Combine(VersionPath, "lib"); }

	public zlib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", Target.Architecture.WindowsLibDir, "Release", "zlibstatic.lib"));
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", "Release", "zlibstatic.lib"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libz.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Unix", Target.Architecture.LinuxName, "Release", "libz.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android) || Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
		{
			PublicSystemLibraries.Add("z");
		}
	}
}
