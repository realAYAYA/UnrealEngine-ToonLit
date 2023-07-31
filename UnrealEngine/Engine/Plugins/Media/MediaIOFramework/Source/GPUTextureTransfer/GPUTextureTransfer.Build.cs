// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GPUTextureTransfer : ModuleRules
	{
		public GPUTextureTransfer(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "GPUDirect");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				PrivateDependencyModuleNames.Add("VulkanRHI");
				PublicDefinitions.Add("DVP_SUPPORTED_PLATFORM=1");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
				PrivateDependencyModuleNames.Add("VulkanRHI");
				PublicDefinitions.Add("DVP_SUPPORTED_PLATFORM=0");
			}
			else
			{
				PublicDefinitions.Add("DVP_SUPPORTED_PLATFORM=0");
			}

			PublicDependencyModuleNames.AddRange(
            	new string[]
                {
                    "Core",
                    "RHI"
                });

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                	"GPUDirect",
					"RenderCore",
					"RHI"
				});

			PublicDefinitions.Add("PERF_LOGGING=0");

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}

	}
}
