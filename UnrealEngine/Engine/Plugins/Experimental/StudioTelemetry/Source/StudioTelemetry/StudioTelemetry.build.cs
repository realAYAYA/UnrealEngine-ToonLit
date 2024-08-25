// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class StudioTelemetry : ModuleRules
{
	public StudioTelemetry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"HTTP"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
                "EngineSettings",
                "BuildSettings",
                "Analytics",
                "AnalyticsET",
				"TelemetryUtils",
				"RHI"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Horde",
					"AssetRegistry",
					"UnrealEd",
					"DerivedDataCache",
					"Zen",
					"IoStoreOnDemand",
					"ContentBrowser",
					"ContentBrowserData",
					"TelemetryUtils"
				}
			);
		}
	}
}
