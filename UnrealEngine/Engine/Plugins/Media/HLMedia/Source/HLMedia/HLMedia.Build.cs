// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class HLMedia : ModuleRules
{
    public HLMedia(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "Media",
            });

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "HLMediaFactory",
                "MediaAssets",
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
			    "CoreUObject",
                "Engine",
                "Projects",
                "RenderCore",
                "RHI",
				"MediaUtils",
            });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Media",
            });

        PrivateIncludePaths.AddRange(
            new string[] {
                "HLMedia/Private",
            });

        var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        PrivateDependencyModuleNames.AddRange(new string[] 
        {
            "HLMediaLibrary",
            "D3D11RHI",
			"D3D12RHI"
        });

        PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/HLMediaLibrary/inc"));

        PublicSystemLibraries.Add("mfplat.lib");
        PublicSystemLibraries.Add("mfreadwrite.lib");
        PublicSystemLibraries.Add("mfuuid.lib");
			
		// For D3D11on12
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12", "DX11");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/Windows/D3D11RHI/Private/Windows"));
        }
    }
}
