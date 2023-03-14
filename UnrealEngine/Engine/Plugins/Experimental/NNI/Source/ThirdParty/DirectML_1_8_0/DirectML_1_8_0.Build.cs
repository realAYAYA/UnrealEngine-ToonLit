// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class DirectML_1_8_0 : ModuleRules
{
    public DirectML_1_8_0(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string PlatformDir = Target.Platform.ToString();
			string BinDirPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "bin", PlatformDir));
			string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
			string IncDirPath = Path.Combine(ModuleDirectory, "include/");
			string LibFileName = "DirectML";
			string DllFileName = LibFileName + ".dll";
			string DllFullPath = Path.Combine(BinDirPath, DllFileName);

			PublicSystemIncludePaths.Add(IncDirPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".lib"));
			PublicDelayLoadDLLs.Add(DllFileName);
			RuntimeDependencies.Add("$(TargetOutputDir)/DML/DirectML.dll", DllFullPath);

			// PublicDefinitions
			PublicDefinitions.Add("WITH_DIRECTML=1");
			PublicDefinitions.Add("DIRECTML_PATH=DML");
		}
	}
}
