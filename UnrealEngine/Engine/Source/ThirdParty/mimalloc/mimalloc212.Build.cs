// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Mimalloc 2.1.2 has not been updated with the Epic modifications yet
// Do not use in Core until this is addressed
public class mimalloc212 : ModuleRules
{
	readonly string Version = "2.1.2";

	public mimalloc212(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, Version, "include"));
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, Version, "src"));
	}
}
