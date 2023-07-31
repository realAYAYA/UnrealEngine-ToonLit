// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DX11Audio_HoloLens : DX11Audio
	{
		public DX11Audio_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
				Target.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
				Target.UEThirdPartySourceDirectory + "Windows/DirectX";
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			string LibDir = null;
			PublicSystemLibraries.AddRange(
				new string[]
				{
					LibDir + "dxguid.lib",
					LibDir + "xapobase.lib"
				}
			);
		}
	}
}
