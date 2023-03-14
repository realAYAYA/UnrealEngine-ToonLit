// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NonRealtimeAudioRenderer : ModuleRules
	{
		public NonRealtimeAudioRenderer(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateIncludePathModuleNames.Add("TargetPlatform");

            PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AudioMixer/Private",
				}
			);

            PublicIncludePaths.Add("Runtime/AudioMixer/Public");

            PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "BinkAudioDecoder",
                    "AudioMixerCore",
                    "SignalProcessing",
                    "AudioMixer",
                }
			);

            AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus",
					"UELibSampleRate"
					);
        }
	}
}
