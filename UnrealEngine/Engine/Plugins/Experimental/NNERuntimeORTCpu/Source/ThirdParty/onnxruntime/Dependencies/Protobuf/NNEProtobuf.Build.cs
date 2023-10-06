// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NNEProtobuf : ModuleRules
{
	public NNEProtobuf(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// PublicDependencyModuleNames.Add("zlib");

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{

			string LibraryPath = Path.Combine(ModuleDirectory, "lib");

			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Linux", "libprotobuf.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac", "libprotobuf.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", "libprotobuf.lib"));
			}

			PublicDefinitions.Add("WITH_PROTOBUF");
		}
	}
}
