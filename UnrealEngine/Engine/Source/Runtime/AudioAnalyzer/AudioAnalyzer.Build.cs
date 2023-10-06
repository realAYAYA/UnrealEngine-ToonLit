// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioAnalyzer : ModuleRules
	{
		public AudioAnalyzer(ReadOnlyTargetRules Target) : base(Target)
		{
			bUseUnity = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"AudioMixer"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SignalProcessing"
				}
			);

			if (Target.Type == TargetType.Editor && Target.Platform == UnrealTargetPlatform.Win64)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "UELibSampleRate");
			}
		}
	}
}
