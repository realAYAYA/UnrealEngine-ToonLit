// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioCaptureEditor : ModuleRules
{
	public AudioCaptureEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"SequenceRecorder",
				"AudioEditor",
				"AudioCapture",
				"AudioCaptureCore",
				"AudioCaptureRtAudio"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Allow us to use direct sound
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
		}
	}
}
