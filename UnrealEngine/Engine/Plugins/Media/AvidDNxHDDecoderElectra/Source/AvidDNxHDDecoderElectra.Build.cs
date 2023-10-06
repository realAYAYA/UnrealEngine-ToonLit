// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvidDNxHDDecoderElectra : ModuleRules
	{
		public AvidDNxHDDecoderElectra(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
                    "Projects",
					"ElectraCodecFactory",
					"ElectraDecoders"
                });

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"DNxHR",
						//"DNxUncompressed",
					}
				);
			}
		}
	}
}
