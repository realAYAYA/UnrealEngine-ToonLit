// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCaptureDialog : ModuleRules
{
	public MovieSceneCaptureDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MovieScene",
				"MovieSceneCapture",
				"MovieSceneTools",
				"PropertyEditor",
				"SessionServices",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			}
		);
	}
}
