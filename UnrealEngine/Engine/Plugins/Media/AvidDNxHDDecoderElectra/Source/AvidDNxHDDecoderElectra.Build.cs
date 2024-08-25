// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

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

				PublicDependencyModuleNames.Add("DirectX");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

				if (Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
				{
					PublicAdditionalLibraries.AddRange(new string[] {
						Path.Combine(Target.WindowsPlatform.DirectXLibDir, "dxerr.lib"),
					});
				}
			}
		}
	}
}
