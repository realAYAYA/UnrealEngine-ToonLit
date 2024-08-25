// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.Linq;

public class BuildSettings : ModuleRules
{
	public BuildSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		DeterministicWarningLevel = WarningLevel.Off; // This module intentionally uses __DATE__ and __TIME__ macros
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateIncludePathModuleNames.Add("Core");

		bRequiresImplementModule = false;

		PrivateDefinitions.Add($"ENGINE_VERSION_MAJOR={Target.Version.MajorVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_MINOR={Target.Version.MinorVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_HOTFIX={Target.Version.PatchVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_STRING=\"{Target.Version.MajorVersion}.{Target.Version.MinorVersion}.{Target.Version.PatchVersion}-{Target.BuildVersion}\"");
		PrivateDefinitions.Add($"ENGINE_IS_LICENSEE_VERSION={(Target.Version.IsLicenseeVersion ? "true" : "false")}");
		PrivateDefinitions.Add($"ENGINE_IS_PROMOTED_BUILD={(Target.Version.IsPromotedBuild ? "true" : "false")}");
		
		if (!Target.GlobalDefinitions.Any(x => x.Contains("CURRENT_CHANGELIST", StringComparison.Ordinal)))
		{
			PrivateDefinitions.Add($"CURRENT_CHANGELIST={Target.Version.Changelist}");
		}

		PrivateDefinitions.Add($"COMPATIBLE_CHANGELIST={Target.Version.EffectiveCompatibleChangelist}");
		PrivateDefinitions.Add($"BRANCH_NAME=\"{Target.Version.BranchName}\"");
		PrivateDefinitions.Add($"BUILD_VERSION=\"{Target.BuildVersion}\"");
		PrivateDefinitions.Add($"BUILD_SOURCE_URL=\"{Target.Version.BuildURL}\"");
	}
}
