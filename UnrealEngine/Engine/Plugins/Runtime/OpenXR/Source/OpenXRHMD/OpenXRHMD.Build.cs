// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenXRHMD : ModuleRules
	{
		public OpenXRHMD(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"OpenXR"
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"XRBase",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "BuildSettings",
                    "InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"RenderCore",
                    "Slate",
                    "SlateCore",
					"AugmentedReality",
					"EngineSettings",
				}
				);

			if (Target.bBuildEditor == true)
            {
				PrivateDependencyModuleNames.Add("EditorFramework");
                PrivateDependencyModuleNames.Add("UnrealEd");
			}

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

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("OculusOpenXRLoader");
			}
		}
	}
}
