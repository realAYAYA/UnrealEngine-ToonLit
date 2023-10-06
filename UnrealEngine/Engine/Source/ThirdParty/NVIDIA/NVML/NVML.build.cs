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
		
		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";
		string Platform = Target.Platform.ToString();
		if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architectures.Contains(UnrealArch.X64))
		{
			string Architecture = UnrealArch.X64.LinuxName;
			DllPath = $"{ModuleDirectory}/Binaries/{Platform}/{Architecture}/{ConfigFolder}/";
			DllName = "libNvmlWrapper.so";

			PublicAdditionalLibraries.Add(Path.Combine(DllPath, DllName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			DllPath = $"{ModuleDirectory}/Binaries/{Platform}/{ConfigFolder}/";
			DllName = "NvmlWrapper.dll";

			PublicAdditionalLibraries.Add(Path.Combine(DllPath, "NvmlWrapper.lib"));
		}

		if (DllName != "")
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
