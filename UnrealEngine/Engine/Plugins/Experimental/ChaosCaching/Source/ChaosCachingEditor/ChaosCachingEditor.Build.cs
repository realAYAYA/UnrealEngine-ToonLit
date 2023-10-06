// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosCachingEditor : ModuleRules
	{
        public ChaosCachingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"InputCore",
					"Engine",
					"UnrealEd",
					"PropertyEditor",
					"BlueprintGraph",
					"ToolMenus",
					"PhysicsCore",
					"ChaosCaching",
					"GeometryCollectionEngine",
					"LevelEditor",
					"LevelSequence",
                    "MovieScene",
                    "MovieSceneTools",
                    "MovieSceneTracks",
                    "Sequencer",
                    "TimeManagement",
					"EditorFramework",
					"TakesCore",
					"TakeRecorder",
					"SceneOutliner",
					"TakeTrackRecorders",
				});

			SetupModulePhysicsSupport(Target);
			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
