// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VPxDecoderElectra : ModuleRules
	{
		public VPxDecoderElectra(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"ElectraCodecFactory",
					"ElectraDecoders",
					"LibVpx"
                });
		}
	}
}
