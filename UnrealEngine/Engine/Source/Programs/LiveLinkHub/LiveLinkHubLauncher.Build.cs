// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using UnrealBuildTool;
using System.IO;

// This is named LiveLinkHubLauncher in order to not clash with the LiveLinkHub module which is in the livelink plugin.
public class LiveLinkHubLauncher : ModuleRules
{
	public LiveLinkHubLauncher(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("LiveLinkHub");

		// LaunchEngineLoop dependencies
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"InstallBundleManager",
				"MediaUtils",
				"Messaging",
				"MoviePlayer",
				"MoviePlayerProxy",
				"Projects",
				"PreLoadScreen",
				"PIEPreviewDeviceProfileSelector",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"TraceLog", 
			}
		);

		// LaunchEngineLoop IncludePath dependencies
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
				"AutomationWorker",
				"AutomationController",
				"AutomationTest",
				"DerivedDataCache",
				"HeadMountedDisplay", 
				"MRMesh", 
				"ProfileVisualizer", 
				"ProfilerService", 
				"SlateRHIRenderer", 
				"SlateNullRenderer",
			}
		);

		// LaunchEngineLoop editor dependencies
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				
				"PropertyEditor",
				"DerivedDataCache",
				"ToolWidgets",
				"UnrealEd"
		});

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnixCommonStartup"
				}
			);
		}

		ShortName = "LLHub";
	}
}
