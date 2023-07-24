// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class CUDAHeader : ModuleRules
{
    public CUDAHeader(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		string cudaPath = Target.UEThirdPartySourceDirectory + "NVIDIA/CUDA";
		PublicSystemIncludePaths.Add(cudaPath);
	}
}
