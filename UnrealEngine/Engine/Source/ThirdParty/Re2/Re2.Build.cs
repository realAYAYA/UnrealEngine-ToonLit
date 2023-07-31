// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Re2 : ModuleRules
{
	protected readonly string Version = "2022-06-01";

	public Re2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VersionPath = Path.Combine(ModuleDirectory, Version);
		string LibraryPath = Path.Combine(VersionPath, "lib");

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Unix", Target.Architecture, "Release", "libre2.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libre2.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", Target.WindowsPlatform.Architecture.ToString().ToLowerInvariant(), "Release", "re2.lib"));
		}
	}
}
