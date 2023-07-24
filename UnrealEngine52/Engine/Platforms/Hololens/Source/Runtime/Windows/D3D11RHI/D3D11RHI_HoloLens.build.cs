// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class D3D11RHI_HoloLens : D3D11RHI
	{
		protected override bool bIncludeExtensions { get => false; }

		public D3D11RHI_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("../Platforms/HoloLens/Source/Runtime/Windows/D3D11RHI/Private");
		}
	}
}
