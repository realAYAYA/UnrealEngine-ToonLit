// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineServicesCommonEngineUtils : ModuleRules
{
	public OnlineServicesCommonEngineUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bCompileAgainstEngine)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine"
				}
			);
		}

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreOnline",
				"CoreUObject",
				"OnlineServicesInterface",
				"OnlineServicesCommon",
				"OnlineBase",
				"Sockets"
			}
		);
	}
}
