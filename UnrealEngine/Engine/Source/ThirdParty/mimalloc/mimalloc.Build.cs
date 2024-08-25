// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class mimalloc : ModuleRules
{
	readonly string Version = "2.0.0";

	public mimalloc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, Version, "include"));
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, Version, "src"));
	}
}
