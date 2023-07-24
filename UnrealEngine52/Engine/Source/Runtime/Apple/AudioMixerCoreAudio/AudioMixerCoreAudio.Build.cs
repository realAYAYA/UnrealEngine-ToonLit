// Copyright Epic Games, Inc. All Rights Reserved.



using UnrealBuildTool;



public class AudioMixerCoreAudio : ModuleRules
{
	public AudioMixerCoreAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");


		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"AudioMixerCore",
				"BinkAudioDecoder"
			}
			);

		PrecompileForTargets = PrecompileTargetsType.None;

        if (Target.bCompileAgainstEngine)
        {
            // Engine module is required for CompressedAudioInfo implementations.
            PrivateDependencyModuleNames.Add("Engine");

            AddEngineThirdPartyPrivateStaticDependencies(Target,
                "UEOgg",
                "Vorbis",
                "VorbisFile"
            );
        }



		PublicFrameworks.AddRange(new string[]
		{
			"AudioToolbox",
			"CoreAudio"
		});

		if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicFrameworks.Add("AudioUnit");
        }


		PublicDefinitions.Add("WITH_OGGVORBIS=1");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}

		PublicDefinitions.Add("WITH_AUDIO_MIXER_THREAD_COMMAND_DEBUG=0");
	}
}
