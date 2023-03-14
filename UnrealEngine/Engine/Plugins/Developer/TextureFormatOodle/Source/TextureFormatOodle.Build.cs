// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using System.Security.Cryptography;
using UnrealBuildTool;

public class TextureFormatOodle : ModuleRules
{
	protected string DynamicLibNamePrefix;
	protected string DynamicLibNameSuffix;

	protected void SetDynamicLibNameStrings()
	{
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			DynamicLibNamePrefix = "oo2tex_win64_";
			DynamicLibNameSuffix = ".dll";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicLibNamePrefix = "liboo2texmac64.";
			DynamicLibNameSuffix = ".dylib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			DynamicLibNamePrefix = "liboo2texlinux64.";
			DynamicLibNameSuffix = ".so";
		}
		else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			DynamicLibNamePrefix = "liboo2texlinuxarm64.";
			DynamicLibNameSuffix = ".so";
		}
		else
		{
			throw new BuildException("Platform {0} not supported in TextureFormatOodle.", Target.Platform);
		}

		PublicDefinitions.Add("TFO_DLL_PREFIX=\"" + DynamicLibNamePrefix + "\"");
		PublicDefinitions.Add("TFO_DLL_SUFFIX=\"" + DynamicLibNameSuffix + "\"");
	}

	protected void AddDynamicLibsForVersion(string Version)
	{
		string PlatformDir = Target.Platform.ToString();

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PlatformDir = "Win64";
		}

		string DynamicLibName = DynamicLibNamePrefix + Version + DynamicLibNameSuffix;
		string SdkBase = Path.Combine(ModuleDirectory, "..", "Sdks", Version);
		string FullDynamicLibName = Path.Combine(SdkBase, "redist", PlatformDir, DynamicLibName);

		if (!File.Exists(FullDynamicLibName))
		{
			throw new BuildException("Platform {0} can't find dynamic lib for  TextureFormatOodle {1}.", Target.Platform, FullDynamicLibName);
		}

		FullDynamicLibName = "$(EngineDir)/" + UnrealBuildTool.Utils.MakePathRelativeTo(FullDynamicLibName, EngineDirectory);

		RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DynamicLibName), FullDynamicLibName, StagedFileType.NonUFS);
	}

	public TextureFormatOodle(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "TextureFormatOodle";

		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"DerivedDataCache",
			"Engine",
			"TargetPlatform",
			"TextureCompressor",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"ImageCore",
			"ImageWrapper",
			"TextureBuild",
		});

		SetDynamicLibNameStrings();

		// dynamic libs , for every version, not just current :

		AddDynamicLibsForVersion("2.9.5");
		AddDynamicLibsForVersion("2.9.6");
		AddDynamicLibsForVersion("2.9.7");
		AddDynamicLibsForVersion("2.9.8");

		string LatestOodleVersion = "2.9.8";

		string IncludeDirectory = Path.Combine(ModuleDirectory, "..", "Sdks", LatestOodleVersion, "include");
		PrivateIncludePaths.Add(IncludeDirectory);
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "Jobify"));

	}
}
