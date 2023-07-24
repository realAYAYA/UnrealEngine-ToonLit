// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;
using System.Globalization;

public class CUDA : ModuleRules
{
	public CUDA(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"RenderCore",
			"RHI",
			"Engine",
		});
		
		PublicDependencyModuleNames.Add("CUDAHeader");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) || Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicDefinitions.Add("PLATFORM_SUPPORTS_CUDA=1");
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			PrivateIncludePathModuleNames.Add("VulkanRHI");

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11", "DX12");
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
		}
		else
		{
			PublicDefinitions.Add("PLATFORM_SUPPORTS_CUDA=0");
		}
	}
}
