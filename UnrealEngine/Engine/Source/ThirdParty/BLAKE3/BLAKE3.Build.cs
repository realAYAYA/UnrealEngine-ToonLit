// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BLAKE3 : ModuleRules
{
	protected readonly string Version = "1.3.1";
	protected string VersionPath { get => Path.Combine(ModuleDirectory, Version); }
	protected string LibraryPath { get => Path.Combine(PlatformModuleDirectory, Version, "lib"); }

	public BLAKE3(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;


		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "c"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", Target.Architecture.bIsX64 ? string.Empty : "arm64", "Release", "BLAKE3.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libBLAKE3.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Unix", Target.Architecture.LinuxName, "Release", "libBLAKE3.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Android", "ARM64", "Release", "libBLAKE3.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Android", "x64", "Release", "libBLAKE3.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			string LibDir = (Target.Architecture == UnrealArch.IOSSimulator)
				? "Simulator"
				: "Release";

			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, PlatformSubdirectoryName, LibDir, "libBLAKE3.a"));
		}
	}
}
