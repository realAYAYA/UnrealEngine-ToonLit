// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class PROJ_HoloLens : PROJ
	{
		public PROJ_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64) // emulation target, bBuildForEmulation
			{
				PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-uwp", "include"));

				string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-uwp", "lib");
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "proj.lib"));
			}
			else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64) // device target, bBuildForDevice
			{
				PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-arm64-uwp", "include"));

				string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-arm64-uwp", "lib");
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "proj.lib"));
			}
			else
			{
				throw new System.Exception("Unknown architecture for HoloLens platform!");
			}

		}
	}
}
