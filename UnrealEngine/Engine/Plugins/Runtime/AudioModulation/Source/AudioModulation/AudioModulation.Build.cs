// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioModulation : ModuleRules
	{
		// Set this to false & disable MetaSound plugin dependency
		// by setting MetaSound's field '"Enabled": false' in the
		// AudioModulation.uplugin if running Modulation without
		// MetaSound support.
		public static bool bIncludeMetaSoundSupport = true;

		public AudioModulation(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioExtensions",
					"Core",
					"WaveTable"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioMixer",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"SignalProcessing"
				}
			);

			if (bIncludeMetaSoundSupport)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"MetasoundEngine",
						"MetasoundFrontend",
						"MetasoundGraphCore"
					}
				);
			}

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"Slate",
						"SlateCore"
					}
				);
			}

			PublicDefinitions.Add("WITH_AUDIOMODULATION=1");
			if (bIncludeMetaSoundSupport)
			{
				PublicDefinitions.Add("WITH_AUDIOMODULATION_METASOUND_SUPPORT=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_AUDIOMODULATION_METASOUND_SUPPORT=0");
			}
		}
	}
}