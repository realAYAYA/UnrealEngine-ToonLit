// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class FreeType2 : ModuleRules
{
	protected virtual string FreeType2Version
	{
		get
		{
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
			{
				return "FreeType2-2.10.4";
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix)
			)
			{
				return "FreeType2-2.10.0";
			}
			else if (Target.Platform == UnrealTargetPlatform.TVOS)
			{
				return "FreeType2-2.4.12";
			}
			else
			{
				return "FreeType2-2.6";
			}
		}
	}

	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string FreeType2IncPath
	{
		get
		{
			string IncPath = (FreeType2Version == "FreeType2-2.6") ? "Include" : "include";
			return Path.Combine(IncRootDirectory, "FreeType2", FreeType2Version, IncPath);
		}
	}
	protected virtual string FreeType2LibPath
	{
		get
		{
			string LibPath = (FreeType2Version == "FreeType2-2.6") ? "Lib" : "lib";
			return Path.Combine(LibRootDirectory, "FreeType2", FreeType2Version, LibPath);
		}
	}

	public FreeType2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_FREETYPE=1");

		PublicSystemIncludePaths.Add(FreeType2IncPath);

		string LibPath;

		if (FreeType2Version.StartsWith("FreeType2-2.10."))
		{
			// FreeType needs these to deal with bitmap fonts
			AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "UElibPNG");

			PublicDefinitions.Add("WITH_FREETYPE_V210=1"); // TODO: Remove this once everything is using FreeType 2.10.0
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibPath = Path.Combine(FreeType2LibPath,
					"Win64",
					"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? Path.Combine(LibPath, "Debug", "freetyped.lib")
				: Path.Combine(LibPath, "Release", "freetype.lib");
				//: Path.Combine(LibPath, "RelWithDebInfo", "freetype.lib");

			PublicAdditionalLibraries.Add(LibPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? "libfreetyped.a"
				: "libfreetype.a";

			PublicAdditionalLibraries.Add(Path.Combine(FreeType2LibPath, "Mac", LibPath));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? Path.Combine("Debug", "libfreetyped.a")
				: Path.Combine("Release", "libfreetype.a");

			PublicAdditionalLibraries.Add(Path.Combine(FreeType2LibPath, "IOS", LibPath));
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			LibPath = (Target.Architecture == "-simulator")
				? "Simulator"
				: "Device";

			PublicAdditionalLibraries.Add(Path.Combine(FreeType2LibPath, "TVOS", LibPath, "libfreetype2412.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			LibPath = Path.Combine(FreeType2LibPath, "Android");
			string LibName = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? Path.Combine("Debug", "libfreetyped.a")
				: Path.Combine("Release", "libfreetype.a");

			// filtered out in the toolchain
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ARM64", LibName));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "x64", LibName));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.Type == TargetType.Server)
			{
				string Err = string.Format("{0} dedicated server is made to depend on {1}. We want to avoid this, please correct module dependencies.", Target.Platform.ToString(), this.ToString());
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			LibPath = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT
				? "libfreetyped_fPIC.a"
				: "libfreetype_fPIC.a";

			PublicAdditionalLibraries.Add(Path.Combine(FreeType2LibPath, "Unix", Target.Architecture, LibPath));
		}
	}
}
