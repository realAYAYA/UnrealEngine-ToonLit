// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms("Win64")]
public class LiveCodingConsoleTarget : TargetRules
{
	public LiveCodingConsoleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		LaunchModuleName = "LiveCodingConsole";

		bBuildDeveloperTools = false;
		bCompileWithPluginSupport = true;
		bIncludePluginsForTargetPlatforms = true;
		bWithLiveCoding = true;

		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstApplicationCore = true;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = false;

		// ICU is needed for regex during click to source code
		bCompileICU = true;
	}
}
