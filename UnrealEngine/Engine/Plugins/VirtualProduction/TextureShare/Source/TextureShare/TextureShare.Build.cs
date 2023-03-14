// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public class TextureShare : ModuleRules
{
	public TextureShare(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(EngineDirectory,"Source/Runtime/Renderer/Private"),
				Path.Combine(EngineDirectory,"Source/Runtime/Windows/D3D11RHI/Private"),
				Path.Combine(EngineDirectory,"Source/Runtime/Windows/D3D11RHI/Private/Windows"),
				Path.Combine(EngineDirectory,"Source/Runtime/D3D12RHI/Private"),
				Path.Combine(EngineDirectory,"Plugins/VirtualProduction/TextureShare/Source/TextureShareCore/Private"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"RHI",
				"RHICore",
				"Renderer",
				"RenderCore",
				"Slate",
				"SlateCore",
				"TextureShareCore"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"D3D11RHI",
					"D3D12RHI"
				}
			);
		}

		// Allow using Vulkan render device
		bool bSupportDeviceVulkan = false;

		if (bSupportDeviceVulkan)
		{
			PublicDefinitions.Add("TEXTURESHARE_VULKAN=1");

			// Support Vulkan device
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			PrivateDependencyModuleNames.AddRange(new string[] { "VulkanRHI" });
			PrivateIncludePathModuleNames.Add("VulkanRHI");
			PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/VulkanRHI/Private"));
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/VulkanRHI/Private/Windows"));
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/VulkanRHI/Private/Linux"));
			}
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARE_VULKAN=0");
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
