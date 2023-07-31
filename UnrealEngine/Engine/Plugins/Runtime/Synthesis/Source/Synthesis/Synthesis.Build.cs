// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Synthesis : ModuleRules
	{
		public Synthesis(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AudioMixer",
					"SignalProcessing",
					"UMG",
					"Slate",
					"SlateCore",
					"InputCore",
					"Projects",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.Add("AudioSynesthesiaCore");
			}
		}
	}
}