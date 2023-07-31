// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using UnrealBuildBase;

public class Catch2 : ModuleRules
{
	/// <summary>
	/// Library name can vary with platform.
	/// For NDA platforms inherit from this module and override this property to set a different library name.
	/// </summary>
	public virtual string LibName
	{
		get
		{
			bool IsDebugConfig = Target.Configuration < UnrealTargetConfiguration.Development;
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Microsoft))
			{
				return string.Format("Catch2{0}.lib", IsDebugConfig ? "d" : string.Empty);
			}
			return string.Format("libCatch2{0}.a", IsDebugConfig ? "d" : string.Empty);
		}
	}

	public virtual string Catch2Root
	{
		get
		{
			bool IsPlatformExtension = !(Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop) || Target.Platform == UnrealTargetPlatform.Android);
			if (IsPlatformExtension)
			{
				return Path.Combine(Unreal.EngineDirectory.FullName, "Platforms", Target.Platform.ToString(), "Source", "ThirdParty", "Catch2");
			}
			else
			{
				return Path.Combine(Unreal.EngineDirectory.FullName, "Source", "ThirdParty", "Catch2");
			}
		}
	}

	public virtual string RelativeBaseLibPath
	{
		get
		{
			bool IsPlatformExtension = !(Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop) || Target.Platform == UnrealTargetPlatform.Android);

			string RelativeLibPath = !IsPlatformExtension ? Target.Platform.ToString() : string.Empty;
			string Arch = string.Empty;
			string Variation = string.Empty;
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				Arch = "arm64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				Arch = "x86_64-unknown-linux-gnu";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				Arch = "x64";
				if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2019)
				{
					Variation = "VS2019";
				}
				else if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2022)
				{
					Variation = "VS2022";
				}
			}
			if (!string.IsNullOrEmpty(Arch))
			{
				RelativeLibPath = Path.Combine(RelativeLibPath, Arch);
			}
			if (!string.IsNullOrEmpty(Variation))
			{
				RelativeLibPath = Path.Combine(RelativeLibPath, Variation);
			}

			return RelativeLibPath;
		}
	}

	public Catch2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool IsDebugConfig = Target.Configuration < UnrealTargetConfiguration.Development;


		string RelativeLibPath = Path.Combine(RelativeBaseLibPath, IsDebugConfig ? "debug" : "release", LibName);

		PublicAdditionalLibraries.Add(Path.Combine(Catch2Root, "v3.0.1", "lib", RelativeLibPath));
		PublicIncludePaths.Add(Path.Combine(Unreal.EngineDirectory.FullName, "Source", "ThirdParty", "Catch2", "v3.0.1", "src"));
	}
}
