// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class UElibPNG : ModuleRules
{
	// no longer needed, once all subclasses remove overrides, delete
	protected virtual string LibRootDirectory { get { return ""; } }
	protected virtual string IncRootDirectory { get { return ""; } }

	protected virtual string LibPNGVersion
	{
		get
		{
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
			{
				return "libPNG-1.6.37";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac ||
				(Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.Arm64))
			{
				return "libPNG-1.5.27";
			}
			else
			{
				return "libPNG-1.5.2";
			}
		}
	}

	protected virtual string IncPNGPath { get { return Path.Combine(ModuleDirectory, LibPNGVersion); } }
	protected virtual string LibPNGPath { get { return Path.Combine(PlatformModuleDirectory, LibPNGVersion, "lib"); } }

	public UElibPNG(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibDir;

		PublicDefinitions.Add("WITH_LIBPNG_1_6=" + (LibPNGVersion.Contains("-1.6.") ? "1" : "0"));

		// On Windows x64, use the LLVM compiled version with changes made by us to improve performance
		// due to better vectorization and FMV support that will take advantage of the different instruction
		// sets depending on CPU supported features.
		// Please, take care of bringing those changes over if you upgrade the library
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) &&
			Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
		{
			string LibFileName = string.Format("libpng15_static{0}.lib", Target.Configuration != UnrealTargetConfiguration.Debug ? "" : "d");
			LibDir = Path.Combine(LibPNGPath, "Win64-llvm", Target.Configuration != UnrealTargetConfiguration.Debug ? "Release" : "Debug");
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, LibFileName));
		}
		// arm64 case
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			LibDir = Path.Combine(LibPNGPath, "Win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), Target.Architecture.WindowsLibDir);

			string LibFileName = "libpng" + (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT ? "d" : "") + "_64.lib";
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, LibFileName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Mac", "libpng.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS))
		{
			LibDir = (Target.Architecture == UnrealArch.IOSSimulator || Target.Architecture == UnrealArch.TVOSSimulator)
				? "Simulator"
				: "Device";

			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, PlatformSubdirectoryName, LibDir, "libpng152.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Android", "ARM64", "libpng.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Android", "x64", "libpng.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "Unix", Target.Architecture.LinuxName, "libpng.a"));
		}

		PublicSystemIncludePaths.Add(IncPNGPath);
	}
}
