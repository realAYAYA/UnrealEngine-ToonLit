// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SignalProcessing : ModuleRules
	{
		public SignalProcessing(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] 
				{
					"IntelISPC"
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus",
					"UELibSampleRate"
					);

			// This is used to get FSoundQualityInfo struct for IAudioEncoder.
            PrivateIncludePathModuleNames.Add("TargetPlatform");

			// This is used to reference the EAudioMixerChannel enumeration.
            PrivateIncludePathModuleNames.Add("AudioMixerCore");
        }
	}
}
