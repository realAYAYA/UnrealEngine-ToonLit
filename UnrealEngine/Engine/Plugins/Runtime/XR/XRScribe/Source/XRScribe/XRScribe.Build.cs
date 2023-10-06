// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	// TODO Possibly split into capture module and replay module??
	public class XRScribe : ModuleRules
	{
        public XRScribe(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"OpenXR",
				"OpenXRHMD",
				"RHI"
			});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
                "DeveloperSettings",
				"Projects",
			});

			// To get access to OpenXRPlatformRHI
			PublicIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("OpenXRHMD"), "Private"));

			// For the RHI/SDK modules, we'll make both public so XRScribeTests can access them.
			// We could just make the SDKs private, and have the tests include the headers. But we
			// might actually want to do something in the tests eventually.

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDependencyModuleNames.AddRange(new string[] {
					"D3D11RHI",
					"D3D12RHI"
				});

				if (!bUsePrecompiled || Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicDependencyModuleNames.AddRange(new string[] {
						"DX11",
						"DX12"
					});
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Android)
			{
				PublicDependencyModuleNames.Add("OpenGLDrv");

				if (!bUsePrecompiled || Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicDependencyModuleNames.Add("OpenGL");
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Android
				|| Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PublicDependencyModuleNames.Add("VulkanRHI");

				if (!bUsePrecompiled || Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicDependencyModuleNames.Add("Vulkan");
				}
			}
		}
	}
}
