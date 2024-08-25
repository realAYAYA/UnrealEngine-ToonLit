// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildBase;
using System.IO;

public class NVML : ModuleRules
{
	private string ProjectBinariesDir
	{
		get
		{
			return "$(TargetOutputDir)";
		}
	}

	public NVML(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string PublicPath = Path.Combine(ModuleDirectory, "Public");

		PublicSystemIncludePaths.Add(PublicPath);

		string DllName = "";
		string DllPath = "";
		string AdditionalLibraryPath = "";

		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";
		string Platform = Target.Platform.ToString();
		if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architectures.Contains(UnrealArch.X64))
		{
			string Architecture = UnrealArch.X64.LinuxName;
			DllPath = $"{ModuleDirectory}/Binaries/{Platform}/{Architecture}/{ConfigFolder}/";
			DllName = "libNvmlWrapper.so";
			AdditionalLibraryPath = Path.Combine(DllPath, DllName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			DllPath = $"{ModuleDirectory}/Binaries/{Platform}/{ConfigFolder}/";
			DllName = "NvmlWrapper.dll";
			AdditionalLibraryPath = Path.Combine(DllPath, "NvmlWrapper.lib");
		}

		// Add additional public lib for NVMLWrapper lib
		if (AdditionalLibraryPath != "" && File.Exists(AdditionalLibraryPath))
		{
			PublicAdditionalLibraries.Add(AdditionalLibraryPath);
		}

		// Add runtime library dep on the NVMLWrapper dll
		if (DllName != "" && File.Exists(Path.Combine(DllPath, DllName)))
		{
			RuntimeDependencies.Add(
					Path.Combine(ProjectBinariesDir, DllName),
					Path.Combine(DllPath, DllName),
					StagedFileType.NonUFS);

			PublicRuntimeLibraryPaths.Add(ProjectBinariesDir);
			PublicDelayLoadDLLs.Add(DllName);
		}
	}
}
