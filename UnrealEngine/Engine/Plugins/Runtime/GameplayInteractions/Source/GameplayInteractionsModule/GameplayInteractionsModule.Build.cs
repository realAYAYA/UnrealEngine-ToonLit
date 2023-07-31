// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayInteractionsModule : ModuleRules
	{
		public GameplayInteractionsModule(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "GameplayInteractions";
			
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AIModule",
					"ContextualAnimation",
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"GameplayTasks",
					"MassEntity",
					"NavCorridor",
					"NavigationSystem",
					"SmartObjectsModule",
					"StateTreeModule",
					"StructUtils",
				}
			);
		}
	}
}
