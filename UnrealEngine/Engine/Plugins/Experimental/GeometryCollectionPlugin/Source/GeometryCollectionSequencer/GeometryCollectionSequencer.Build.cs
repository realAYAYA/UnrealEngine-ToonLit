// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GeometryCollectionSequencer : ModuleRules
    {
        public GeometryCollectionSequencer(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePathModuleNames.AddRange(
                new string[] {
                "Sequencer",
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AssetTools",
                    "Core",
                    "CoreUObject",
                    "Engine",
					"LevelSequence",
                    "MovieScene",
                    "MovieSceneTools",
                    "MovieSceneTracks",
                    "RHI",
                    "SequencerCore",
                    "Sequencer",
                    "Slate",
                    "SlateCore",
                    "TimeManagement",
					"EditorFramework",
                    "UnrealEd",
                    "GeometryCollectionTracks",
					"Chaos",
                    "GeometryCollectionEngine",
                }
            );
        }
    }
}
