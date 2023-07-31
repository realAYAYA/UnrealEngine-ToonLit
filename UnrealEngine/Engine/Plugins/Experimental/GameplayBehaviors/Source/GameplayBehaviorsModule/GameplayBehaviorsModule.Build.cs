// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayBehaviorsModule : ModuleRules
	{
		public GameplayBehaviorsModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "AIModule",
				"GameplayTasks",
				"GameplayTags",
				"GameplayAbilities",
            }
			);
		}
	}
}
