// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class HarfBuzz : ModuleRules
{
	protected virtual string HarfBuzzVersion {
		get
		{
			if (Target.Platform == UnrealTargetPlatform.IOS ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Android) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix)
				)
			{
				return "harfbuzz-2.4.0";
			}
			else
			{
				return "harfbuzz-1.2.4";
			}
		}
	}

	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string IncHarfBuzzRootPath { get { return Path.Combine(IncRootDirectory, "HarfBuzz", HarfBuzzVersion, "src"); } }
	protected virtual string LibHarfBuzzRootPath
	{
		get
		{
			string LibPath = Path.Combine(LibRootDirectory, "HarfBuzz", HarfBuzzVersion);
			if (HarfBuzzVersion == "harfbuzz-2.4.0")
			{
				LibPath = Path.Combine(LibPath, "lib");
			}
			return LibPath;
		}
	}

	public HarfBuzz(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Can't be used without our dependencies
		if (!Target.bCompileFreeType || !Target.bCompileICU)
		{
			PublicDefinitions.Add("WITH_HARFBUZZ=0");
			return;
		}

		if (HarfBuzzVersion == "harfbuzz-2.4.0")
		{
			PublicDefinitions.Add("WITH_HARFBUZZ_V24=1"); // TODO: Remove this once everything is using HarfBuzz 2.4.0
			PublicDefinitions.Add("WITH_HARFBUZZ=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_HARFBUZZ=0");
		}
		string HarfBuzzPlatform = Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) ? "Win64" : Target.Platform.ToString();
		string HarfBuzzLibPath = Path.Combine(LibHarfBuzzRootPath, HarfBuzzPlatform);
		string BuildTypeFolderName = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				? "Debug"
				: "Release";
		string LibPath;

		// Includes
		PublicSystemIncludePaths.Add(IncHarfBuzzRootPath);

		// Libs
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			LibPath = Path.Combine(HarfBuzzLibPath, "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64)
			{
				LibPath = Path.Combine(LibPath, Target.Architecture.WindowsLibDir);
			}

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, BuildTypeFolderName, "harfbuzz.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? "libharfbuzzd.a"
				: "libharfbuzz.a";
			PublicAdditionalLibraries.Add(Path.Combine(HarfBuzzLibPath, LibPath));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (Target.Architecture == UnrealArch.IOSSimulator)
			{
				BuildTypeFolderName = "Simulator";
			}
			PublicAdditionalLibraries.Add(Path.Combine(HarfBuzzLibPath, BuildTypeFolderName, "libharfbuzz.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			// filtered out in the toolchain
			LibPath = Path.Combine(BuildTypeFolderName, "libharfbuzz.a");
			PublicAdditionalLibraries.Add(Path.Combine(LibHarfBuzzRootPath, "Android", "ARM64", LibPath));
			PublicAdditionalLibraries.Add(Path.Combine(LibHarfBuzzRootPath, "Android", "x64", LibPath));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.Type == TargetType.Server)
			{
				string Err = string.Format("{0} dedicated server is made to depend on {1}. We want to avoid this, please correct module dependencies.", Target.Platform.ToString(), this.ToString());
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT ? "libharfbuzzd_fPIC.a" : "libharfbuzz_fPIC.a";
			PublicAdditionalLibraries.Add(Path.Combine(LibHarfBuzzRootPath, "Unix", Target.Architecture.LinuxName, LibPath));
		}
	}
}
