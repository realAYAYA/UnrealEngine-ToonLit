// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MIDIDevice : ModuleRules
	{
        public MIDIDevice(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
				}
			);
			AddEngineThirdPartyPrivateStaticDependencies(Target, "portmidi");
		}
	}
}
