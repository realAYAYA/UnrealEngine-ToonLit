// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerSDL : ModuleRules
{
	public AudioMixerSDL(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");

		string PlatformName = Target.Platform.ToString();
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PlatformName = "Linux";
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateIncludePaths.Add("Runtime/Linux/AudioMixerSDL/Private/" + PlatformName);
		}

		{
			// Bink Audio isn't yet built for other SDL platforms, however
			// the module won't provide a lib and will define WITH_BINK_AUDIO
			// to 0 for those platforms.
			PrivateDependencyModuleNames.Add("BinkAudioDecoder");
		}

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"AudioMixer",
				"AudioMixerCore"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"UEOgg",
			"Vorbis",
			"VorbisFile",
			"SDL2"
			);
	}
}
