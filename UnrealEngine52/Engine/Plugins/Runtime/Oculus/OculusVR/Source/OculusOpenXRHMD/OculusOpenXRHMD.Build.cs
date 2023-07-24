// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OculusOpenXRHMD : ModuleRules
    {
        public OculusOpenXRHMD(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.AddRange(
                new string[] {
                    // Relative to Engine\Plugins\Runtime\Oculus\OculusOpenXR\Source
                    "../../../OpenXR/Source/OpenXRHMD/Private",
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
					"../../../../../Source/Runtime/Engine/Classes/Components",
                    "../../../../../Source/Runtime/Engine/Classes/Kismet",
                });

            PublicIncludePathModuleNames.AddRange(
                new string[] {
                    "Launch",
                    "OpenXRHMD",
                });			

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "RHI",
                    "RenderCore",
                    "Renderer",
                    "Slate",
                    "SlateCore",
                    "ImageWrapper",
                    "MediaAssets",
                    "Analytics",
                    "OpenGLDrv",
                    "VulkanRHI",
                    "HeadMountedDisplay",
		    "OpenXR",
                    "OculusOpenXRLoader",
                    "Projects",
                });
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "OpenXRHMD",
                });

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("EditorFramework");
            }

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                // D3D
                {
                    PrivateDependencyModuleNames.AddRange(new string[] {
						"D3D11RHI",
						"D3D12RHI",
					});

                    AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11", "DX12");
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
                }

                // Vulkan
                {
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                }
            }
            else if (Target.Platform == UnrealTargetPlatform.Android)
            {
                // Vulkan
                {
                    AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                }
            }
        }
    }
}
