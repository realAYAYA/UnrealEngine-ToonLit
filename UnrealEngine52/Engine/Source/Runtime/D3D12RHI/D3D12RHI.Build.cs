// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	protected virtual bool bUsesWindowsD3D12 { get => Target.Platform.IsInGroup(UnrealPlatformGroup.Windows); }

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

		PublicIncludePathModuleNames.Add("HeadMountedDisplay");

		///////////////////////////////////////////////////////////////
        // Platform specific defines
        ///////////////////////////////////////////////////////////////

		if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

		if (bUsesWindowsD3D12)
		{
			PublicDefinitions.Add("D3D12RHI_PLATFORM_HAS_CUSTOM_INTERFACE=0");

			if (Target.WindowsPlatform.bPixProfilingEnabled &&
				Target.Configuration != UnrealTargetConfiguration.Shipping &&
				Target.Configuration != UnrealTargetConfiguration.Test)
            {
				PublicDefinitions.Add("PROFILE");
				PublicDependencyModuleNames.Add("WinPixEventRuntime");
			}
		}

		if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDefinitions.Add("D3D12RHI_USE_D3DDISASSEMBLE=0");
		}
	}
}
