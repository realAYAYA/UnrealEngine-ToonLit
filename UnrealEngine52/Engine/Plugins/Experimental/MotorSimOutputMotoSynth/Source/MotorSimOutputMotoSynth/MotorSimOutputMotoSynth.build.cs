// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MotorSimOutputMotoSynth : ModuleRules
	{
		public MotorSimOutputMotoSynth(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
					"MotoSynth",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AudioMixer",
					"AudioMotorSim",
					"SignalProcessing",
				}
			);
		}
	}
}