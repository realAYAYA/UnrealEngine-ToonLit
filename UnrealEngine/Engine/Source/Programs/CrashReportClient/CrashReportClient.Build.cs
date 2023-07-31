// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashReportClient : ModuleRules
{
	public CrashReportClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange
		(
			new string[] 
			{ 
				"Runtime/Launch/Public",
				"Programs/CrashReportClient/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"CrashReportCore",
				"HTTP",
				"Json",
				"Projects",
				"PakFile",
				"XmlParser",
				"Analytics",
				"AnalyticsET",
				"DesktopPlatform",
				"LauncherPlatform",
				"InputCore",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"MessageLog",
			}
		);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
				"SlateReflector",
				}
			);
		}

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include

		PrivateDefinitions.AddRange(
			new string[]
			{
				"CRASH_REPORT_WITH_MTBF=1",
			}
		);
	}
}
