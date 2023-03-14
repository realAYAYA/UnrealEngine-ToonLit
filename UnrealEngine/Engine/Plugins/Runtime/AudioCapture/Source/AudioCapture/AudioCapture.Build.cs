// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioCapture : ModuleRules
	{
		public AudioCapture(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
					"AudioCaptureCore"
				}
			);

            if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
				Target.Platform == UnrealTargetPlatform.Mac)
            {
                PrivateDependencyModuleNames.Add("AudioCaptureRtAudio");
            }
            else if (Target.Platform == UnrealTargetPlatform.IOS)
            {
                PrivateDependencyModuleNames.Add("AudioCaptureAudioUnit");
            }
            else if (Target.Platform == UnrealTargetPlatform.Android)
            {
                PrivateDependencyModuleNames.Add("AudioCaptureAndroid");
            }
        }
	}
}