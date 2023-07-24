// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public class TextureShareCore : ModuleRules
{
	public TextureShareCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
		});

		// Show more log for internal sync processes
		bool bEnableExtraDebugLog = false;
		if (bEnableExtraDebugLog)
		{
			PublicDefinitions.Add("TEXTURESHARECORE_DEBUGLOG=1");
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARECORE_DEBUGLOG=0");
		}

		bool bEnableExtraDebugLogForInternalBarriers = false;
		if (bEnableExtraDebugLogForInternalBarriers && bEnableExtraDebugLog)
		{
			PublicDefinitions.Add("TEXTURESHARECORE_BARRIER_DEBUGLOG=1");
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARECORE_BARRIER_DEBUGLOG=0");
		}

		// Allow using Vulkan render device
		bool bSupportDeviceVulkan = false;

		if (bSupportDeviceVulkan)
		{
			PublicDefinitions.Add("TEXTURESHARECORE_VULKAN=1");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");

			if (Target.bShouldCompileAsDLL==false)
			{
				PublicDefinitions.Add("TEXTURESHARECORE_INITIALIZEVULKANEXTENSIONS");

				PrivateDependencyModuleNames.AddRange(new string[] { "VulkanRHI" });
				PrivateIncludePathModuleNames.Add("VulkanRHI");
			}
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARECORE_VULKAN=0");
		}

		if (Target.bShouldCompileAsDLL)
		{
			// Special build rules for SDK
			PublicDefinitions.Add("TEXTURESHARECORE_SDK=1");
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARECORE_SDK=0");
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicIncludePaths.AddRange(
				new string[] {
				Path.Combine(EngineDirectory,"Source/Runtime/Windows/D3D11RHI/Public"),
				Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Public"),
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
		}
	}
}
