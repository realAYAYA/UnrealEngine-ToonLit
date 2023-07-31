// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerPlatformAudioLink: ModuleRules
{
	public AudioMixerPlatformAudioLink(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");


		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"AudioMixer",
				"AudioMixerCore",
				"AudioLinkEngine"
		});
					
		if (Target.bCompileAgainstEngine) 
		{
			PrivateDependencyModuleNames.Add("BinkAudioDecoder");

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile"
				);
		}
	}
}
