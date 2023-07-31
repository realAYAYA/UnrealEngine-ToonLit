// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RHI_HoloLens : RHI
	{
		public RHI_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.bCompileAgainstEngine)
			{
				if (Target.Type != TargetRules.TargetType.Server)   // Dedicated servers should skip loading everything but NullDrv
				{
					DynamicallyLoadedModuleNames.Add("D3D11RHI");
					DynamicallyLoadedModuleNames.Add("D3D12RHI");
				}
			}
		}
	}
}
