// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioMixer : ModuleRules
	{
		public AudioMixer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.Add("TargetPlatform");
			PublicIncludePathModuleNames.Add("TargetPlatform");

			PublicIncludePathModuleNames.Add("Engine");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"AudioLinkEngine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"NonRealtimeAudioRenderer",
					"AudioMixerCore",
					"SignalProcessing",
					"AudioPlatformConfiguration",
					"SoundFieldRendering",
					"AudioExtensions",
					"AudioLinkCore",
					"AudioLinkEngine",
					"HeadMountedDisplay"
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus",
					"UELibSampleRate"
					);

			// Circular references that need to be cleaned up
			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"NonRealtimeAudioRenderer",
					"SoundFieldRendering"
				}
			);
		}
	}
}
