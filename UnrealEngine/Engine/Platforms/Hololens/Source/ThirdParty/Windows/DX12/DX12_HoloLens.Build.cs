// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DX12_HoloLens : DX12
	{
		protected override bool bSupportsD3D12Core { get => true; }

		public DX12_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicSystemLibraries.Add("dxgi.lib"); // For DXGIGetDebugInterface1
		}
	}
}
