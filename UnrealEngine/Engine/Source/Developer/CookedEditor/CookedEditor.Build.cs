// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CookedEditor : ModuleRules
{
	public CookedEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		if (!Target.bCompileAgainstEngine)
		{
			throw new BuildException("CookedEditor module is meant for cooking only operations, and currently requires Engine to be enabled. This module is being included in a non-Engine-enabled target.");
		}

		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"AssetRegistry",
				"NetCore",
				"CoreOnline",
				"CoreUObject",
				"Projects",
				"Engine",
			});
		
		PublicDependencyModuleNames.AddRange(new string[]
			{
				"TargetPlatform",
			});

		PublicIncludePathModuleNames.Add("WindowsTargetPlatform");

		if (IsPlatformAvailable(UnrealTargetPlatform.Linux))
		{
			PublicDefinitions.Add("COOKEDEDITOR_WITH_LINUXTARGETPLATFORM=1");
			PublicIncludePathModuleNames.Add("LinuxTargetPlatform");
		}
		else
		{
			PublicDefinitions.Add("COOKEDEDITOR_WITH_LINUXTARGETPLATFORM=0");
		}

		if (IsPlatformAvailable(UnrealTargetPlatform.Mac))
		{
			PublicDefinitions.Add("COOKEDEDITOR_WITH_MACTARGETPLATFORM=1");
			PublicIncludePathModuleNames.Add("MacTargetPlatform");
		}
		else
		{
			PublicDefinitions.Add("COOKEDEDITOR_WITH_MACTARGETPLATFORM=0");
		}
	}
}
