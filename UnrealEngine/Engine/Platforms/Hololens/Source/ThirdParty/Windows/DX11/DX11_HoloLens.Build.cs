// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DX11_HoloLens : DX11
	{
		public DX11_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";

			PublicSystemIncludePaths.Add(DirectXSDKDir + "/Include");

			PublicSystemLibraries.AddRange(
				new string[] {
				"dxguid.lib",
				}
			);

		}
	}
}
