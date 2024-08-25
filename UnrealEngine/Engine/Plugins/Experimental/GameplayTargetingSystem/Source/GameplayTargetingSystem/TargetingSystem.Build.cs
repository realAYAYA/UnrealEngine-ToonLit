// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TargetingSystem : ModuleRules
{
	// TODO: Rename parent directory to TargetingSystem so it matches module name
	public TargetingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTasks",
				"GameplayAbilities",
				"GameplayTags",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DeveloperSettings"
			}
		);
	}
}