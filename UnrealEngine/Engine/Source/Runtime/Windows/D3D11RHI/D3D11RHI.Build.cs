// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

[SupportedPlatformGroups("Windows")]
public class D3D11RHI : ModuleRules
{
	public D3D11RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(new string[] { "Shaders" });

		PrivateDependencyModuleNames.AddAll(
			"CoreUObject",
			"Engine",
			"RHICore",
			"RenderCore"
		);

		PublicDependencyModuleNames.AddAll(
			"Core",
			"RHI"
		);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDependencyModuleNames.Add("HeadMountedDisplay");
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
	}
}
