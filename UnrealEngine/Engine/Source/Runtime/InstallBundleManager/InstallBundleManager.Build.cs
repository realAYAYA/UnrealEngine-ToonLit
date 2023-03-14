// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InstallBundleManager : ModuleRules
{
	public InstallBundleManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Projects",
				"ApplicationCore",
				"Json"
			}
		);
	}
}
