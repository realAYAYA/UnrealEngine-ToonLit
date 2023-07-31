// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;

public class BuildPlugin_HoloLens : BuildPlugin.TargetPlatform
{
	const string HoloLensArchitecture = "arm64+x64";

	public BuildPlugin_HoloLens()
	{
	}

	[Obsolete("Deprecated in UE5.1; function signature changed")]
	public override void CompilePluginWithUBT(string UBTExe, FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, string TargetName, TargetType TargetType, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, List<FileReference> ManifestFileNames, string InAdditionalArgs)
	{
		// Make sure to save the manifests for each architecture with unique names so they don't get overwritten.
		// This fixes packaging issues when building from binary engine releases, where the build produces a manifest for the plugin for ARM64, which
		// then gets overwritten by the manifest for x64. Then during packaging, the plugin is referencing a manifest for the wrong architecture.
		foreach (string Arch in HoloLensArchitecture.Split('+'))
		{
			FileReference ManifestFileName = FileReference.Combine(HostProjectFile.Directory, "Saved", String.Format("Manifest-{0}-{1}-{2}-{3}.xml", TargetName, Platform, Configuration, Arch));
			ManifestFileNames.Add(ManifestFileName);
			string Arguments = String.Format("-plugin={0} -iwyu -noubtmakefiles -manifest={1} -nohotreload", CommandUtils.MakePathSafeToUseWithCommandLine(HostProjectPluginFile.FullName), CommandUtils.MakePathSafeToUseWithCommandLine(ManifestFileName.FullName));
			Arguments += String.Format(" -Architecture={0}", Arch);
			if (!String.IsNullOrEmpty(InAdditionalArgs))
			{
				Arguments += InAdditionalArgs;
			}
			CommandUtils.RunUBT(CmdEnv, UBTExe, HostProjectFile, TargetName, Platform, Configuration, Arguments);
		}
	}

	public override void CompilePluginWithUBT(FileReference UnrealBuildToolDll, FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, string TargetName, TargetType TargetType, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, List<FileReference> ManifestFileNames, string InAdditionalArgs)
	{
		// Make sure to save the manifests for each architecture with unique names so they don't get overwritten.
		// This fixes packaging issues when building from binary engine releases, where the build produces a manifest for the plugin for ARM64, which
		// then gets overwritten by the manifest for x64. Then during packaging, the plugin is referencing a manifest for the wrong architecture.
		foreach (string Arch in HoloLensArchitecture.Split('+'))
		{
			FileReference ManifestFileName = FileReference.Combine(HostProjectFile.Directory, "Saved", String.Format("Manifest-{0}-{1}-{2}-{3}.xml", TargetName, Platform, Configuration, Arch));
			ManifestFileNames.Add(ManifestFileName);
			string Arguments = String.Format("-plugin={0} -iwyu -noubtmakefiles -manifest={1} -nohotreload", CommandUtils.MakePathSafeToUseWithCommandLine(HostProjectPluginFile.FullName), CommandUtils.MakePathSafeToUseWithCommandLine(ManifestFileName.FullName));
			Arguments += String.Format(" -Architecture={0}", Arch);
			if (!String.IsNullOrEmpty(InAdditionalArgs))
			{
				Arguments += InAdditionalArgs;
			}
			CommandUtils.RunUBT(CmdEnv, UnrealBuildToolDll, HostProjectFile, TargetName, Platform, Configuration, Arguments);
		}

	}
}
