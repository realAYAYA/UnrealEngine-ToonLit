// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class nghttp2 : ModuleRules
{
	protected readonly string Version = "1.47.0";

	public nghttp2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VersionPath = Path.Combine(ModuleDirectory, Version);
		string LibraryPath = Path.Combine(VersionPath, "lib");

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Unix", Target.Architecture, "Release", "libnghttp2.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string[] Architectures = new string[] {
				"ARM64",
				"x64",
			};
 
			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Android", Architecture, "Release", "libnghttp2.a"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "Release", "libnghttp2.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", "Release", "nghttp2.lib"));
			PublicDefinitions.Add("NGHTTP2_STATICLIB=1");
		}

		// Our build requires OpenSSL and zlib, so ensure they're linked in
		AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
		{
			"OpenSSL",
			"zlib"
		});
	}
}
