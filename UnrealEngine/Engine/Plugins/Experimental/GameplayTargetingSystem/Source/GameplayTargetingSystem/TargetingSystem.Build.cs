// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
public class TargetingSystem : ModuleRules
{
	// TODO: Rename parent directory to TargetingSystem so it matches module name
	public TargetingSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// TODO: Move source code to have proper Private/Public subfolders
		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

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

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
			}
		);
	}
}