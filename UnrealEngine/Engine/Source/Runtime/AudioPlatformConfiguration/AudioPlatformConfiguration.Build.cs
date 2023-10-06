// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioPlatformConfiguration : ModuleRules
	{
		public AudioPlatformConfiguration(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject"
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"CoreUObject"
				}
			);

			PrivateIncludePathModuleNames.Add("Engine");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "UELibSampleRate");
			PrivateDefinitions.Add("WITH_LIBSAMPLERATE=1");
		}
	}
}
