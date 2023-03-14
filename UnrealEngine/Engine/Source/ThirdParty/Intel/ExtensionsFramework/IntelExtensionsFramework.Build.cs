// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class IntelExtensionsFramework : ModuleRules
{
	public IntelExtensionsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string IntelExtensionsFrameworkPath = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel", "ExtensionsFramework");

			PublicAdditionalLibraries.Add(Path.Combine(IntelExtensionsFrameworkPath, "igdext64.lib"));
			PublicSystemIncludePaths.Add(IntelExtensionsFrameworkPath);

			PublicSystemLibraries.Add("shlwapi.lib");
			PublicSystemLibraries.Add("setupapi.lib");
			PublicSystemLibraries.Add("cfgmgr32.lib");

			PublicDefinitions.Add("INTEL_EXTENSIONS=1");
		}
		else
		{
			PublicDefinitions.Add("INTEL_EXTENSIONS=0");
		}
	}
}