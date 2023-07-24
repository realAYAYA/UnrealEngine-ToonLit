// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class DirectML : ModuleRules
{
    public DirectML(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		string PlatformDir = Target.Platform.ToString();
		string BinDirPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "bin", PlatformDir));
		string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		string IncDirPath = Path.Combine(ModuleDirectory, "include/");
		string LibFileName = "DirectML";
		string DllFileName = LibFileName + ".dll";
		string DllFullPath = Path.Combine(BinDirPath, DllFileName);
		string DbgDllFileName = LibFileName + ".Debug.dll";
		string DbgDllFullPath = Path.Combine(BinDirPath, DbgDllFileName);

		// Win64
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(IncDirPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".lib"));
			PublicDelayLoadDLLs.Add(DllFileName);
			RuntimeDependencies.Add("$(TargetOutputDir)/DML/" + DllFileName, DllFullPath);

			PublicDefinitions.Add("DML_TARGET_VERSION=0x5100");
			PublicDefinitions.Add("WITH_DIRECTML");
			PublicDefinitions.Add("DIRECTML_PATH=DML");

			if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
				Target.Configuration != UnrealTargetConfiguration.Test)
			{
				PublicDefinitions.Add("WITH_DIRECTML_DEBUG");
				PublicDelayLoadDLLs.Add(DllFileName);
				RuntimeDependencies.Add("$(TargetOutputDir)/DML/" + DbgDllFileName, DbgDllFullPath);
			}
		}
	}
}
