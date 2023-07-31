// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioCaptureTimecodeProvider : ModuleRules
	{
		public AudioCaptureTimecodeProvider(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("AudioCaptureTimecodeProvider/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioCapture",
                    "AudioCaptureCore",
                    "Core",
					"CoreUObject",
					"Engine",
					"LinearTimecode",
					"TimeManagement"
				}
			);
		}
	}
}
