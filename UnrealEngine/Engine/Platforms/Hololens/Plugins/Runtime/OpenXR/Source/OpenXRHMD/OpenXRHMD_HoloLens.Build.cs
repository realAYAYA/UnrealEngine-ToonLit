// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenXRHMD_HoloLens : OpenXRHMD
	{
		public OpenXRHMD_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
					"D3D11RHI",
					"D3D12RHI",
				});

			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11", "DX12");

		}
	}
}
