// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libstrophe : ModuleRules
{
	protected virtual string StropheVersion { get { return (Target.Platform == UnrealTargetPlatform.Mac) ? "libstrophe-0.9.1" : "libstrophe-0.9.3"; } } 

	protected virtual string LibRootDirectory { get { return ModuleDirectory; } }

	protected virtual string StrophePackagePath { get { return Path.Combine(LibRootDirectory, StropheVersion); } }

	protected virtual string StropheLibRootPath { get { return (Target.Platform == UnrealTargetPlatform.Mac) ? StrophePackagePath : Path.Combine(StrophePackagePath, "Lib"); } }

	protected virtual string StropheIncludePath { get { return Path.Combine(StrophePackagePath, "Include"); } }

	protected virtual string ConfigName { get { return (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release"; } }

	protected virtual bool bRequireExpat { get { return true; } }

	public libstrophe(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PrivateDefinitions.Add("XML_STATIC");
		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, StropheVersion));

		if (bRequireExpat)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Expat");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(Path.Combine(StropheLibRootPath, "Android", "arm64", ConfigName, "libstrophe.a"));
			PublicAdditionalLibraries.Add(Path.Combine(StropheLibRootPath, "Android", "x64", ConfigName, "libstrophe.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(StropheLibRootPath, Target.Platform.ToString(), ConfigName, "libstrophe.a"));
			PublicSystemLibraries.Add("resolv");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string LibrayPath = Path.Combine(StropheLibRootPath, "Win64", "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), ConfigName) + "/";
			PublicAdditionalLibraries.Add(LibrayPath + "strophe.lib");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(StropheLibRootPath, "Unix", Target.Architecture.LinuxName.ToString(), ConfigName, "libstrophe" + ((Target.LinkType != TargetLinkType.Monolithic) ? "_fPIC" : "") + ".a"));
			PublicSystemLibraries.Add("resolv");
		}
	}
}