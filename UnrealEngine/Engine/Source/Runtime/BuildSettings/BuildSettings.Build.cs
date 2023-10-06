// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class BuildSettings : ModuleRules
{
	public BuildSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		DeterministicWarningLevel = WarningLevel.Off; // This module intentionally uses __DATE__ and __TIME__ macros
		PrivateIncludePathModuleNames.Add("Core");

		bRequiresImplementModule = false;

		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_MAJOR={0}", Target.Version.MajorVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_MINOR={0}", Target.Version.MinorVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_HOTFIX={0}", Target.Version.PatchVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_IS_LICENSEE_VERSION={0}", Target.Version.IsLicenseeVersion? "true" : "false"));
		PrivateDefinitions.Add(string.Format("ENGINE_IS_PROMOTED_BUILD={0}", Target.Version.IsPromotedBuild? "true" : "false"));

		bool bContainsCurrentChangelist = false;
		foreach (string item in Target.GlobalDefinitions)
		{
			if (item.Contains("CURRENT_CHANGELIST"))
			{
				bContainsCurrentChangelist = true;
				break;
			}
		}
		if (!bContainsCurrentChangelist)
		{
			PrivateDefinitions.Add(string.Format("CURRENT_CHANGELIST={0}", Target.Version.Changelist));
		}
		PrivateDefinitions.Add(string.Format("COMPATIBLE_CHANGELIST={0}", Target.Version.EffectiveCompatibleChangelist));

		PrivateDefinitions.Add(string.Format("BRANCH_NAME=\"{0}\"", Target.Version.BranchName));

		PrivateDefinitions.Add(string.Format("BUILD_VERSION=\"{0}\"", Target.BuildVersion));
		if (!String.IsNullOrEmpty(Target.Version.BuildURL))
		{
			PrivateDefinitions.Add(string.Format("BUILD_SOURCE_URL=\"{0}\"", Target.Version.BuildURL));
		}
				
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
