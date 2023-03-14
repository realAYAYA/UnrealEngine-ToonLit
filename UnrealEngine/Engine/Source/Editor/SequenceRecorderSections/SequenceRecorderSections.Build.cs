// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SequenceRecorderSections : ModuleRules
	{
		public SequenceRecorderSections(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "MovieScene",
                    "MovieSceneTracks",
					"SequenceRecorder",
					"TimeManagement"
				}
				);
		}
	}
}
