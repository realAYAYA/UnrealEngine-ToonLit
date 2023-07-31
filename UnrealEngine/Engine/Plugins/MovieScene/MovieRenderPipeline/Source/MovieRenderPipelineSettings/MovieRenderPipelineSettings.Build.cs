// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieRenderPipelineSettings : ModuleRules
{
	public MovieRenderPipelineSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"NonRealtimeAudioRenderer",
				"AudioMixer",
				"UMG",
				"Slate",
				"RenderCore",
				"RHI",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
                "MovieScene",
                "MovieSceneTracks",
				"LevelSequence",
				"Engine",
				"MovieRenderPipelineCore",
				"OpenColorIO",
			}
		);
	}
}
