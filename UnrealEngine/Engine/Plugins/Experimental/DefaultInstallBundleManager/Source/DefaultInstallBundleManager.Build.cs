// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// This module must be loaded "PreLoadingScreen" in the .uproject file, otherwise it will not hook in time!

public class DefaultInstallBundleManager : ModuleRules
{
	public DefaultInstallBundleManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"InstallBundleManager",
				"PatchCheck",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"PakFile",
				"BuildPatchServices",
				"Json",
				"Engine",
				"RenderCore",
				"AnalyticsET",
				"OnlineSubsystem",
			}
		);
	}
}
