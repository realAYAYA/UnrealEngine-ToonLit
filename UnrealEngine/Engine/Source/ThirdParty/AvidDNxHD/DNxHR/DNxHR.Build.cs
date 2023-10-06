// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class DNxHR : ModuleRules
{
	public DNxHR(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string IncPath = Path.Combine(ModuleDirectory, "include");
			PublicSystemIncludePaths.Add(IncPath);

			string LibPath = Path.Combine(ModuleDirectory, "lib64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "DNxHR.lib"));

            string DllFileName = "DNxHR.dll";
            PublicDelayLoadDLLs.Add(DllFileName);
            RuntimeDependencies.Add("$(TargetOutputDir)/" + DllFileName, Path.Combine(LibPath, DllFileName));
        }
    }
}
