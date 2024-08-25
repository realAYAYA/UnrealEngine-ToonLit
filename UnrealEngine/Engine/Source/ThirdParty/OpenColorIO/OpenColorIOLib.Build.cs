// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class OpenColorIOLib : ModuleRules
{
	private string ProjectBinariesDir
	{
		get
		{
			return "$(TargetOutputDir)";
		}
	}

	public OpenColorIOLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bIsPlatformAdded = false;

		string PlatformDir = Target.Platform.ToString();
		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		string BinaryDir = Path.Combine(EngineDir, "Binaries", "ThirdParty", "OpenColorIO");
		string DeployDir = Path.Combine(ModuleDirectory, "Deploy", "OpenColorIO");

		PublicSystemIncludePaths.Add(Path.Combine(DeployDir, "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string Arch = Target.Architecture.WindowsLibDir;
			string DLLName = "OpenColorIO_2_3.dll";
			string LibDirectory = Path.Combine(BinaryDir, PlatformDir, Arch);

			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "OpenColorIO.lib"));
			RuntimeDependencies.Add(
				Path.Combine(ProjectBinariesDir, DLLName),
				Path.Combine(LibDirectory, DLLName)
			);

			bIsPlatformAdded = true;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string Arch = Target.Architecture.LinuxName;
			string SOName = "libOpenColorIO.so";
			string LibDirectory = Path.Combine(BinaryDir, "Unix", Arch);

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, SOName));
			RuntimeDependencies.Add(
				Path.Combine(ProjectBinariesDir, SOName),
				Path.Combine(LibDirectory, SOName)
			);
			//@todo: Test removing the following dependencies on linux - should not be needed?
			RuntimeDependencies.Add(
				Path.Combine(ProjectBinariesDir, "libOpenColorIO.so.2.3"),
				Path.Combine(LibDirectory, "libOpenColorIO.so.2.3")
			);

			bIsPlatformAdded = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(BinaryDir, PlatformDir);
			string DylibName = "libOpenColorIO.2.3.dylib";

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, DylibName));
			RuntimeDependencies.Add(
				Path.Combine(LibDirectory, DylibName)
			);

			bIsPlatformAdded = true;
		}

		// @note: Any update to this definition should be mirrored in the wrapper module's WITH_OCIO.
		PublicDefinitions.Add("WITH_OCIO_LIB=" + (bIsPlatformAdded ? "1" : "0"));
	}
}
