// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class IntelExtensionsFramework : ModuleRules
{
	public IntelExtensionsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Architecture.bIsX64)
		{
			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel", "ExtensionsFramework");
			string IncludeDir = ThirdPartyDir;
			string LibrariesDir = ThirdPartyDir;

			PublicDefinitions.Add("INTEL_EXTENSIONS=1");

			PublicSystemIncludePaths.Add(IncludeDir);
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, "igdext64.lib"));
		}
		else
		{
			PublicDefinitions.Add("INTEL_EXTENSIONS=0");
		}
	}
}