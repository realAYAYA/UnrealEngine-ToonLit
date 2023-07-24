// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayBehaviorsEditorModule : ModuleRules
	{
		public GameplayBehaviorsEditorModule(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayBehaviorsModule",
				"SlateCore"
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
			}
			);
		}
	}
}