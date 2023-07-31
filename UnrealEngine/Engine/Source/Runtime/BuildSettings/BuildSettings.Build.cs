// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BuildSettings : ModuleRules
{
	public BuildSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("Core");

		bRequiresImplementModule = false;

		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_MAJOR={0}", Target.Version.MajorVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_MINOR={0}", Target.Version.MinorVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_VERSION_HOTFIX={0}", Target.Version.PatchVersion));
		PrivateDefinitions.Add(string.Format("ENGINE_IS_LICENSEE_VERSION={0}", Target.Version.IsLicenseeVersion? "true" : "false"));
		PrivateDefinitions.Add(string.Format("ENGINE_IS_PROMOTED_BUILD={0}", Target.Version.IsPromotedBuild? "true" : "false"));

		PrivateDefinitions.Add(string.Format("CURRENT_CHANGELIST={0}", Target.Version.Changelist));
		PrivateDefinitions.Add(string.Format("COMPATIBLE_CHANGELIST={0}", Target.Version.EffectiveCompatibleChangelist));

		PrivateDefinitions.Add(string.Format("BRANCH_NAME=\"{0}\"", Target.Version.BranchName));

		PrivateDefinitions.Add(string.Format("BUILD_VERSION=\"{0}\"", Target.BuildVersion));

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
