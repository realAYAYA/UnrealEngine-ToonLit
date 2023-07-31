// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OculusHMD : ModuleRules
	{
		public OculusHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
					"../../../../../Source/Runtime/Engine/Classes/Components",
					"../../../../../Source/Runtime/Engine/Classes/Kismet",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"Launch",
					"ProceduralMeshComponent",
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
					"OVRPlugin",
					"OculusOpenXRLoader",
					"ProceduralMeshComponent",
					"Projects",
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
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

					PrivateIncludePaths.AddRange(
						new string[]
						{
							"OculusMR/Public",
						});

					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11", "DX12");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
				}

				// Vulkan
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				}

				// OVRPlugin
				{
					//PublicDelayLoadDLLs.Add("OVRPlugin.dll");
					RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OVRPlugin.dll");
				}

				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OpenXR/OVRPlugin.dll");
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// We are not currently supporting Mixed Reality on Android, but we need to include IOculusMRModule.h for OCULUS_MR_SUPPORTED_PLATFORMS definition
				PrivateIncludePaths.AddRange(
						new string[]
						{
							"OculusMR/Public"
						});

				// Vulkan
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				}

				// AndroidPlugin
				{
					string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
					AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "OculusMobile_APL.xml"));
				}
			}
		}
	}
}
