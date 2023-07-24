// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

[SupportedPlatforms("Win64")]
public class TextureShare : ModuleRules
{
	public TextureShare(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		// [temporary] We need this to be able to use some private data types. This should
		// be removed once we move the nD rendering pipeline to RDG (TextureShareSceneViewExtension.h).
		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source", "Runtime", "Renderer", "Private"));

		// Internal dependency (debug log purpose)
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Plugins", "VirtualProduction", "TextureShare", "Source", "TextureShareCore", "Private"));

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

		// Show more log
		bool bEnableExtraDebugLog = false;
		if (bEnableExtraDebugLog)
		{
			//Show log in SDK-for Debug and DebugGame builds
			PublicDefinitions.Add("TEXTURESHARE_DEBUGLOG=1");
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARE_DEBUGLOG=0");
		}

		// Allow using Vulkan render device
		bool bSupportDeviceVulkan = false;

		if (bSupportDeviceVulkan)
		{
			PublicDefinitions.Add("TEXTURESHARE_VULKAN=1");

			// Support Vulkan device
			PrivateDependencyModuleNames.AddRange(new string[] { "VulkanRHI" });
			PrivateIncludePathModuleNames.Add("VulkanRHI");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
		}
		else
		{
			PublicDefinitions.Add("TEXTURESHARE_VULKAN=0");
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
