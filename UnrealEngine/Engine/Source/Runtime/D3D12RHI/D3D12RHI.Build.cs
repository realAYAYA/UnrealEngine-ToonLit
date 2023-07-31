// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	protected virtual bool bUsesWindowsD3D12 { get => false; }

	public D3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("../Shaders/Shared");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"RHICore",
				"RenderCore",
				}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
					"HeadMountedDisplay"
			}
			);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		///////////////////////////////////////////////////////////////
        // Platform specific defines
        ///////////////////////////////////////////////////////////////

		if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
			bUsesWindowsD3D12)
		{
			PublicDefinitions.Add("D3D12RHI_PLATFORM_HAS_CUSTOM_INTERFACE=0");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");

			if (Target.WindowsPlatform.bPixProfilingEnabled &&
				Target.Configuration != UnrealTargetConfiguration.Shipping &&
				Target.Configuration != UnrealTargetConfiguration.Test)
            {
				PublicDefinitions.Add("PROFILE");
				PublicDependencyModuleNames.Add("WinPixEventRuntime");
			}

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
            {
				AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
            	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
            }
            else
            {
				PrivateDefinitions.Add("D3D12RHI_USE_D3DDISASSEMBLE=0");
			}
        }
    }
}
