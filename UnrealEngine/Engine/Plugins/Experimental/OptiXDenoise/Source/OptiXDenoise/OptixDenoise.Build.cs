// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OptiXDenoise : ModuleRules
	{
		public OptiXDenoise(ReadOnlyTargetRules Target) : base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"Renderer",
					"RHI",
					"CUDA",
					"Projects",
					"OptiXDenoiseBase"
				}
			);
			
			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("MessageLog");
			}

			if(Target.Platform == UnrealTargetPlatform.Win64)
			{
				PrivateDependencyModuleNames.Add("D3D12RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			}
		}
	}
}
