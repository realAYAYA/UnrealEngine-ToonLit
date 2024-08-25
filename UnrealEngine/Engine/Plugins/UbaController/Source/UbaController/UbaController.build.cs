// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UbaController : ModuleRules
{
	public UbaController(ReadOnlyTargetRules TargetRules) : base(TargetRules)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"DistributedBuildInterface",
			"Projects",
			"RenderCore",
			"TargetPlatform",
			"Horde",
			"HTTP",
			"Sockets",
			"Json"
		});

		string Arch = TargetRules.Architecture.WindowsName == "arm64ec" ? "arm64" : TargetRules.Architecture.WindowsName;
		string UbaSourcePath = Path.Combine(EngineDirectory, "Source", "Programs", "UnrealBuildAccelerator");

		PublicIncludePaths.Add(UbaSourcePath);
		PublicIncludePaths.Add(Path.Combine(UbaSourcePath, "Core", "Public"));
		PublicIncludePaths.Add(Path.Combine(UbaSourcePath, "Common", "Public"));
        PublicIncludePaths.Add(Path.Combine(UbaSourcePath, "Host", "Public"));
        PublicIncludePathModuleNames.Add("BLAKE3");

		string UbaBinariesPath = Path.Combine(EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", Arch);

		PublicAdditionalLibraries.Add(Path.Combine(UbaBinariesPath, "UbaHost.lib"));
		PrivateRuntimeLibraryPaths.Add(Path.Combine(ModuleDirectory, "lib"));

		PublicDelayLoadDLLs.Add("UbaHost.dll");
	}
}