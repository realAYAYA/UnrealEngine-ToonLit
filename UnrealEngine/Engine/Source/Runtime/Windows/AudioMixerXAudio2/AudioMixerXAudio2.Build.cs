// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerXAudio2 : ModuleRules
{
	public AudioMixerXAudio2(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		if (Target.bCompileAgainstEngine)
        {
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);
        }
        PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"CoreUObject",
					"AudioMixer",
					"AudioMixerCore"
                }
		);

		PrecompileForTargets = PrecompileTargetsType.None;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"DX11Audio",
			"XAudio2_9"
        );

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
