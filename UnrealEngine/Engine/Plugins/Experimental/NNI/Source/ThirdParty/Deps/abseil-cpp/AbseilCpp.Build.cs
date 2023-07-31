// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class AbseilCpp : ModuleRules
{
	public AbseilCpp(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		// Win64, Linux and PS5
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac 
			)
		{
			// PublicSystemIncludePaths
			string IncPath = Path.Combine(ModuleDirectory, "include/");
			PublicSystemIncludePaths.Add(IncPath);
			
			// PublicAdditionalLibraries
			string PlatformDir = Target.Platform.ToString();
			string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
			string[] LibFileNames = new string[] {
				"absl_base",
				"absl_city",
				"absl_hash",
				"absl_low_level_hash",
				"absl_raw_hash_set",
				"absl_raw_logging_internal",
				"absl_throw_delegate",
			};

			foreach (string LibFileName in LibFileNames)
			{
				if(Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".lib"));
				} 
				else if(Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
				{
					PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, "lib" + LibFileName + ".a"));
				}
			}
		}
	}
}
