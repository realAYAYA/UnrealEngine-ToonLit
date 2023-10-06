// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class RHI : ModuleRules
{
	public RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("TraceLog");
		PrivateDependencyModuleNames.Add("ApplicationCore");

		if (Target.bCompileAgainstEngine)
		{
			DynamicallyLoadedModuleNames.Add("NullDrv");

			if (Target.Type != TargetRules.TargetType.Server)   // Dedicated servers should skip loading everything but NullDrv
			{
				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop))
                {
					PublicDefinitions.Add("RHI_WANT_BREADCRUMB_EVENTS=1");
				}

				if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
                {
					PublicDefinitions.Add("RHI_WANT_RESOURCE_INFO=1");
                }

				// UEBuildAndroid.cs adds VulkanRHI for Android builds if it is enabled
				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
				{
					DynamicallyLoadedModuleNames.Add("D3D11RHI");
				}

				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
				{
					//#todo-rco: D3D12 requires different SDK headers not compatible with WinXP
					DynamicallyLoadedModuleNames.Add("D3D12RHI");
				}

				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) || Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
				{
					DynamicallyLoadedModuleNames.Add("VulkanRHI");
				}

				if ((Target.Platform.IsInGroup(UnrealPlatformGroup.Windows)) ||
					(Target.IsInPlatformGroup(UnrealPlatformGroup.Linux) && Target.Type != TargetRules.TargetType.Server))  // @todo should servers on all platforms skip this?
				{
					DynamicallyLoadedModuleNames.Add("OpenGLDrv");
				}
			}
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "ProfileVisualizer" });
		}
    }
}
