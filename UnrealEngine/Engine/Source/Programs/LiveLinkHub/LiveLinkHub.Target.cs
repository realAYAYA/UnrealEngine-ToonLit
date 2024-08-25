// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using EpicGames.Core;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class LiveLinkHubTarget : TargetRules
{
	// Whether this should be built as a monolithic executable.
	[CommandLine("-Monolithic")]
	public bool bMonolithic = false;

	// Whether the hub is being built for distribution alongside a cooked editor.
	// Will dictate whether to mount LLH's remapped engine folder to "/Engine/"
	[CommandLine("-CookedEditorDistribution")]
	public bool bCookedEditorDistribution = false;

	// Whether to disable building third party plugins.
	[CommandLine("-EnableThirdPartyPlugins=")]
	public bool bEnableThirdPartyPlugins = true;

	public LiveLinkHubTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = bMonolithic ? TargetLinkType.Monolithic : TargetLinkType.Modular;
		LaunchModuleName = "LiveLinkHubLauncher";

		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		SolutionDirectory = "Programs/LiveLink";
		
		// These plugins are required for running LiveLinkHub. 
		// They may be a direct dependency or a dependency of one of our plugins.
		AdditionalPlugins.AddRange(new string[]
		{
			"LiveLink",
			"LiveLinkHub",
			"LiveLinkCamera",
			"LiveLinkLens", // Needed for Vicon
			"LensComponent", // Needed by LiveLinkLens
			"LiveLinkInputDevice",
			"ContentBrowserAssetDataSource",
			"ProceduralMeshComponent", // Needed by LensComponent
			"PropertyAccessEditor",
			"PythonScriptPlugin",
			"QuicMessaging",
			"StructUtils",
			"UdpMessaging"
		});
		
		
		if (bEnableThirdPartyPlugins)
		{
			OptionalPlugins.AddRange(new string[]
			{
				"LiveLinkViconDataStream",
				"MocopiLiveLink",
				"PoseAILiveLink",
				"Smartsuit"
			});
		}

		OptionalPlugins.AddRange(new string[]
		{
			"AppleARKitFaceSupport"
		});

		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = true;
		bCompileAgainstEditor = true;
		bBuildWithEditorOnlyData = true;
		bIncludePluginsForTargetPlatforms = false;
		bLegalToDistributeBinary = true;

		bUsesSlate = true;

		bCompileICU = false;
		bCompilePython = false; 
		bCompileWithPluginSupport = true;
		bBuildDeveloperTools = true;
		bIsBuildingConsoleApplication = false;
		bUseLoggingInShipping = true;

		GlobalDefinitions.Add("WITH_LIVELINK_HUB=1");
		GlobalDefinitions.Add("AUTOSDKS_ENABLED=0");
		GlobalDefinitions.Add("PLATFORM_SUPPORTS_MESSAGEBUS=1");

		if (bCookedEditorDistribution)
		{
			GlobalDefinitions.Add("COOKED_EDITOR_DISTRIBUTION=1");
		}

		bEnableTrace = true;

		OptedInModulePlatforms = new UnrealTargetPlatform[] { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac,
															  UnrealTargetPlatform.Linux, UnrealTargetPlatform.LinuxArm64 };
	}
}
