// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioLinkEngine : ModuleRules
	{
		public AudioLinkEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// We depend on USoundSubmix/AudioDevice which is in Engine.
					"Core",
					"CoreUObject",
					"Engine",
				}
			);
		}
	}
}
