// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WmfMedia : ModuleRules
	{
		public WmfMedia(ReadOnlyTargetRules Target) : base(Target)
		{
            bEnableExceptions = true;

            DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "WmfMediaFactory"
                    });

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
                    "Engine",
                    "MediaUtils",
					"Projects",
					"RenderCore",
                    "RHI",
                });


            PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
				PrivateDependencyModuleNames.Add("HeadMountedDisplay");
			}

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PrivateDependencyModuleNames.Add("D3D11RHI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");

				PublicDelayLoadDLLs.Add("mf.dll");
				PublicDelayLoadDLLs.Add("mfplat.dll");
				PublicDelayLoadDLLs.Add("mfplay.dll");
				PublicDelayLoadDLLs.Add("shlwapi.dll");
			}
		}
	}
}
