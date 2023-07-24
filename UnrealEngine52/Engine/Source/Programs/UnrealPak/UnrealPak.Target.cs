// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealPakTarget : TargetRules
{
	public UnrealPakTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		LaunchModuleName = "UnrealPak";

		bBuildDeveloperTools = false;
		bUseMallocProfiler = false;
		bCompileWithPluginSupport = true;
		bIncludePluginsForTargetPlatforms = true;

		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = true;

		// Currently this app uses AssetRegistry and CoreUObject, but not Engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;

		// ICU is not needed
		bCompileICU = false;

		// UnrealPak is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		bCompileAgainstApplicationCore = true;
		bIsBuildingConsoleApplication = true;

		GlobalDefinitions.Add("UE_TRACE_ENABLED=1");
	}
}
