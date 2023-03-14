// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class D3D11RHI : ModuleRules
{
	protected virtual bool bIncludeExtensions { get => true; }

	public D3D11RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("../Shaders/Shared");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"RHICore",
				"RenderCore"
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
					"HeadMountedDisplay"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		if (bIncludeExtensions)
		{ 
        	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
        	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
		}


        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}
	}
}
