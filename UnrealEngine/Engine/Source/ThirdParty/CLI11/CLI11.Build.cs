// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CLI11 : ModuleRules
{
	protected readonly string Version = "2.2.0";

	public CLI11(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VersionPath = Path.Combine(ModuleDirectory, Version);

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));
	}
}
