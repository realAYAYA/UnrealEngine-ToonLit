// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class AMD_AGS : ModuleRules
{
	public AMD_AGS(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Architecture.bIsX64)
		{
			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "AMD", "AMD_AGS");
			string IncludeDir = Path.Combine(ThirdPartyDir, "inc");
			string LibrariesDir = Path.Combine(ThirdPartyDir, "lib", "VS2017");

			string LibraryName = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				? "amd_ags_x64_2017_MDd.lib"
				: "amd_ags_x64_2017_MD.lib";

			PublicDefinitions.Add("WITH_AMD_AGS=1");

			PublicSystemIncludePaths.Add(IncludeDir);
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, LibraryName));
		}
		else
		{
			PublicDefinitions.Add("WITH_AMD_AGS=0");
		}
	}
}

