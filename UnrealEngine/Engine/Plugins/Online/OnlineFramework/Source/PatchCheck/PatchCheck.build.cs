// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PatchCheck : ModuleRules
{
	public PatchCheck(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"OnlineSubsystem",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"OnlineSubsystem",
			}
		);

		PrivateDefinitions.Add("PATCH_CHECK_FAIL_ON_GENERIC_FAILURE=" + (bFailOnGenericFailure ? "1" : "0"));
		PublicDefinitions.Add("PATCH_CHECK_PLATFORM_ENVIRONMENT_DETECTION=" + (bPlatformEnvironmentDetection ? "1" : "0"));
	}

	protected virtual bool bFailOnGenericFailure { get { return true; } }
	protected virtual bool bPlatformEnvironmentDetection { get { return false; } }
}
