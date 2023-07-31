// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleProResMedia : ModuleRules
	{
		public AppleProResMedia(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[] {
                    "ProResLib",
					"ProResToolbox",
                    "WmfMediaFactory",
                    "WmfMedia",
					"ImageWriteQueue",
					"MovieRenderPipelineCore",
                });

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "CoreUObject",
                    "Engine",
                    "MovieSceneCapture",
                    "Projects",
                    "WmfMediaFactory",
                    "WmfMedia"
                }
            );

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicDelayLoadDLLs.Add("mf.dll");
                PublicDelayLoadDLLs.Add("mfplat.dll");
                PublicDelayLoadDLLs.Add("mfplay.dll");
                PublicDelayLoadDLLs.Add("shlwapi.dll");

                PublicSystemLibraries.Add("mf.lib");
                PublicSystemLibraries.Add("mfplat.lib");
                PublicSystemLibraries.Add("mfuuid.lib");
                PublicSystemLibraries.Add("shlwapi.lib");
                PublicSystemLibraries.Add("d3d11.lib");
            }
        }
    }
}
