// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DX12_HoloLens : DX12
	{
		protected override bool bUsesWindowsD3D12 { get => true; }
		protected override bool bUsesWindowsD3D12Libs { get => false; }
		// TODO: Enable when Microsoft supports this
		protected override bool bUsesAgilitySDK { get => false; }

		public DX12_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
