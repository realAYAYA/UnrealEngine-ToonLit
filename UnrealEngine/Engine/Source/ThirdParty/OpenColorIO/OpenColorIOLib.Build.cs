// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class OpenColorIOLib : ModuleRules
{
	public OpenColorIOLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bIsPlatformAdded = false;

		string PlatformDir = Target.Platform.ToString();
		string BinaryDir = "$(EngineDir)/Binaries/ThirdParty/OpenColorIO";
		string DeployDir = Path.Combine(ModuleDirectory, "Deploy/OpenColorIO-2.2.0");

		PublicSystemIncludePaths.Add(Path.Combine(DeployDir, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string Arch = Target.Architecture.WindowsLibDir;
			string DLLName = "OpenColorIO_2_2.dll";
			string LibDirectory = Path.Combine(BinaryDir, PlatformDir, Arch);

			PublicAdditionalLibraries.Add(Path.Combine(DeployDir, "lib", PlatformDir, Arch, "OpenColorIO.lib"));
			RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DLLName), Path.Combine(LibDirectory, DLLName));

			bIsPlatformAdded = true;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string Arch = Target.Architecture.LinuxName;
			string SOName = "libOpenColorIO.so";
			string LibDirectory = Path.Combine(BinaryDir, "Unix", Arch);

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, SOName));
			RuntimeDependencies.Add(Path.Combine(LibDirectory, SOName));
			RuntimeDependencies.Add(Path.Combine(LibDirectory, "libOpenColorIO.so.2.2"));

			bIsPlatformAdded = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDirectory = Path.Combine(BinaryDir, PlatformDir);
			string DylibName = "libOpenColorIO.2.2.dylib";

			PublicAdditionalLibraries.Add(Path.Combine(LibDirectory, DylibName));
			RuntimeDependencies.Add(Path.Combine(LibDirectory, DylibName));

			bIsPlatformAdded = true;
		}

		// @note: Any update to this definition should be mirrored in the wrapper module's WITH_OCIO.
		PublicDefinitions.Add("WITH_OCIO_LIB=" + (bIsPlatformAdded ? "1" : "0"));
	}
}
