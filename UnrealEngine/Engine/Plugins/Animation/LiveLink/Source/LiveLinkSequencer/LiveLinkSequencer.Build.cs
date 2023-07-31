// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkSequencer : ModuleRules
	{
		public LiveLinkSequencer(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"LiveLinkComponents",
					"LiveLinkInterface",
					"LiveLinkMovieScene",
					"MovieScene",
					"TakeTrackRecorders",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"LevelSequence",
					"LiveLink",
					"MovieSceneTools",
					"MovieSceneTracks",
					"Projects",
					"Sequencer",
					"SequenceRecorder",
					"SerializedRecorderInterface",
					"Slate",
					"SlateCore",
					"TakesCore",
					"TakeRecorder",
					"TargetPlatform",
					"EditorFramework",
					"UnrealEd",
                }
			); 
		}
	}
}
