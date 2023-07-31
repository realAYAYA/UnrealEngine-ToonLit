// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class WaveformTransformations : ModuleRules
	{
		public WaveformTransformations(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AudioExtensions",
					"AudioSynesthesiaCore",
					"Core", 
					"CoreUObject",
					"Engine",
					"SignalProcessing"
				}
			);
		}
	}
}