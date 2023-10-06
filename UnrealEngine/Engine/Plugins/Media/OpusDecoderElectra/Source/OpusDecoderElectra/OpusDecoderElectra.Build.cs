// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpusDecoderElectra : ModuleRules
	{
		public OpusDecoderElectra(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"ElectraCodecFactory",
					"ElectraDecoders",
					"libOpus"
                });
		}
	}
}
