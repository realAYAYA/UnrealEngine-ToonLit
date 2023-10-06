// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioExtensions : ModuleRules
	{
		public AudioExtensions(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"SignalProcessing",
					"AudioMixerCore"
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"AudioMixer",
				}
			);
		}
	}
}
