// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class Nsync : ModuleRules
{
	public Nsync(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		// All platforms not Win64
		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
		{
			// PublicSystemIncludePaths
			string IncPath = Path.Combine(ModuleDirectory, "include/");
			PublicSystemIncludePaths.Add(IncPath);
			IncPath = Path.Combine(ModuleDirectory, "include/public");
			PublicSystemIncludePaths.Add(IncPath);

			// PublicAdditionalLibraries
			string PlatformDir = Target.Platform.ToString();
			string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
			string[] LibFileNames = new string[] {
				"libnsync_cpp",
				"libnsync"
			};
			foreach (string LibFileName in LibFileNames)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".a"));
			}

			// PublicDefinitions
			PublicDefinitions.Add("WITH_ONNXRUNTIME_NSYNC");
		}
	}
}
