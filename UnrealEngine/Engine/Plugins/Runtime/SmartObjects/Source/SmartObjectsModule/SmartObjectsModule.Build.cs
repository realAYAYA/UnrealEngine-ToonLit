// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SmartObjectsModule : ModuleRules
	{
		public SmartObjectsModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"AIModule",
				"GameplayTags",
				"GameplayAbilities",
				"RHI",
				"StructUtils",
				"WorldConditions",
				"NavigationSystem",
				"TargetingSystem",
				"PropertyBindingUtils",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
				"InputCore"
			}
			);

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(
					new string[] {
						"UnrealEd",
						"BlueprintGraph",
					}
				);
			}

			SetupGameplayDebuggerSupport(Target);
		}
	}
}
