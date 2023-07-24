// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NiagaraSimCachingEditor : ModuleRules
	{
        public NiagaraSimCachingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("NiagaraSimCachingEditor/Private");
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PublicDependencyModuleNames.AddRange(
				new[]
				{
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"InputCore",
					"Engine",
					"UnrealEd",
					"PropertyEditor",
					"ToolMenus",
					"Niagara",
					"NiagaraSimCaching",
					"LevelSequence",
                    "MovieScene",
                    "MovieSceneTools",
                    "MovieSceneTracks",
                    "Sequencer",
                    "TimeManagement",
					"EditorFramework",
					"TakesCore",
					"TakeRecorder",
					"TakeTrackRecorders",
					"CacheTrackRecorder",
				});
		}
	}
}
