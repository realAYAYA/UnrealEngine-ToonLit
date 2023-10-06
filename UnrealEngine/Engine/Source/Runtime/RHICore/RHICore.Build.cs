// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RHICore : ModuleRules
{
	public RHICore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "RHI" });
		PublicDependencyModuleNames.AddRange(new string[] { "RenderCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("RHICORE_PLATFORM_DXGI_H=<dxgi.h>");
		}
	}
}
