// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class VulkanRHI : ModuleRules
{
	protected virtual bool bShouldIncludePlatformPrivate { get { return true; } }

	public VulkanRHI(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;
		bLegalToDistributeObjectCode = true;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateIncludePaths.Add("Runtime/VulkanRHI/Private/Windows");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PrivateIncludePaths.Add("Runtime/VulkanRHI/Private/Linux");
			}
		}
		else if (bShouldIncludePlatformPrivate)
		{
			PrivateIncludePaths.Add("Runtime/VulkanRHI/Private/" + Target.Platform);
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("VK_USE_PLATFORM_WIN32_KHR=1");
			PublicDefinitions.Add("VK_USE_PLATFORM_WIN32_KHX=1");
		}
		else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Android))
		{
			PublicDefinitions.Add("VK_USE_PLATFORM_ANDROID_KHR=1");
		}


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"CoreUObject",
				"ApplicationCore",
				"Engine", 
				"RHI",
				"RHICore",
				"RenderCore", 
				"HeadMountedDisplay",
                "PreLoadScreen",
				"BuildSettings"
            }
        );

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Launch"
				}
			);

			// for Swappy
			PublicDefinitions.Add("USE_ANDROID_SWAPPY=1");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				"Launch",
				"GoogleGameSDK"
				}
			);
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) || Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) || Target.Platform == UnrealTargetPlatform.Android
			|| Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) || Target.Platform == UnrealTargetPlatform.Mac)
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
        }

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.Add("ApplicationCore");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}

