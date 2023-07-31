// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class DirectMLDefault : ModuleRules
{
    public DirectMLDefault(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;
		// Win64
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// PublicSystemIncludePaths
			string IncPath = Path.Combine(ModuleDirectory, "include/");
			PublicSystemIncludePaths.Add(IncPath);
			// PublicAdditionalLibraries
			string PlatformDir = Target.Platform.ToString();
			string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
			string LibFileName = "DirectML";

			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".lib"));

			// PublicDelayLoadDLLs
			string DLLReleaseFileName = LibFileName + ".dll";
			string DLLDebugFileName = LibFileName + ".Debug.dll";

			bool ReleaseMode = true;
			string DLLFileName;

			if (ReleaseMode == true)
            {
				DLLFileName = DLLReleaseFileName;
			}
            else
            {
				DLLFileName = DLLDebugFileName;
			}

			PublicDelayLoadDLLs.Add(DLLFileName);
			// RuntimeDependencies
			string BinaryThirdPartyDirPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "bin", PlatformDir));
			string DLLFullPath = Path.Combine(BinaryThirdPartyDirPath, DLLFileName);
			RuntimeDependencies.Add(DLLFullPath);

			//Console.WriteLine("Module:" + ModuleDirectory);
			//Console.WriteLine("Full:" + Path.Combine(LibDirPath, "DirectML.lib"));
			//Console.WriteLine("DLL:" + DLLFullPath);
			//Console.WriteLine("Platform:" + PlatformDir);

			// PublicDefinitions
			PublicDefinitions.Add("DML_TARGET_VERSION=0x5000");
			PublicDefinitions.Add("DIRECTML_USE_DLLS");
			PublicDefinitions.Add("WITH_DIRECTML");
			PublicDefinitions.Add("DIRECTML_BIN_PATH=" + BinaryThirdPartyDirPath.Replace('\\', '/'));
			PublicDefinitions.Add("DIRECTML_PLATFORM_PATH=bin/" + PlatformDir);
			PublicDefinitions.Add("DIRECTML_DLL_NAME=" + DLLFileName);
		}
		
	}
}
