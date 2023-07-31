// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using EpicGames.Core;

[SupportedPlatforms("Win64", "Linux", "Mac")]
public class UnrealInsightsTarget : TargetRules
{
	[CommandLine("-Monolithic")]
	public bool bMonolithic = false;

	public UnrealInsightsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = bMonolithic ? TargetLinkType.Monolithic : TargetLinkType.Modular;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		LaunchModuleName = "UnrealInsights";
		if (bBuildEditor)
		{
			ExtraModuleNames.Add("EditorStyle");
		}
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bForceBuildTargetPlatforms = true;
		bCompileWithStatsWithoutEngine = true;
		bCompileWithPluginSupport = true;

		// For source code editor access & regex (crossplatform)
		bIncludePluginsForTargetPlatforms = true;
		bCompileICU = true;

		// For UI functionality
		bBuildDeveloperTools = true;

		bHasExports = false;

		// Have UnrealInsights implicitly launch the trace store.
		GlobalDefinitions.Add("WITH_UNREAL_TRACE_LAUNCH=1");
	}
}
