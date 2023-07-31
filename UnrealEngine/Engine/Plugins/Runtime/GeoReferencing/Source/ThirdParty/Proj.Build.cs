// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PROJ : ModuleRules
{
	protected readonly string VcPkgInstalled = "vcpkg-installed";

	public PROJ(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		bEnableExceptions = true;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-windows", "include"));

			string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-windows", "lib");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "proj.lib"));
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64_arm64-osx", "include"));

            string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64_arm64-osx", "lib");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libproj.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-arm64-ios", "include"));

            string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-arm64-ios", "lib");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libproj.a"));
        }
		else if(Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-linux", "include"));

			string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-linux", "lib");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libproj.a"));
		}
		else if(Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			// we use the windows include because the triplet folders will be removed because they have arch-specific path components
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, "overlay-x64-windows", "include"));

			string[] Triplets = new string[] {
				"arm64-android",
				"x64-android",
			};
 
			// ubt will remove libraries with arch-specific path components, so inject all the libs for all the archs
			foreach(var Triplet in Triplets)
			{
				string LibPath = Path.Combine(ModuleDirectory, VcPkgInstalled, Triplet, "lib");
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libproj.a"));
			}
		}
	}
}
