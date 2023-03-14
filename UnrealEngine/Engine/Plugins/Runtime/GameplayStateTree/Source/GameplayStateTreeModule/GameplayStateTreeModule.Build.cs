// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayStateTreeModule : ModuleRules
	{
		public GameplayStateTreeModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AIModule",
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTasks",
					"StateTreeModule",
					"StructUtils"
				}
			);
		}
	}
}
