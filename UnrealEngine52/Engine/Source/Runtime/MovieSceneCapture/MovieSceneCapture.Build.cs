// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCapture : ModuleRules
{
	public MovieSceneCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
		{
			PublicIncludePathModuleNames.Add("ImageWrapper");
			DynamicallyLoadedModuleNames.Add("ImageWrapper");
		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
				"TimeManagement",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AVIWriter",
                "Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"ImageWriteQueue",
				"Json",
				"JsonUtilities",
				"MovieScene",
                "MovieSceneTracks",
                "RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"AudioMixer"
			}
		);
    }
}
