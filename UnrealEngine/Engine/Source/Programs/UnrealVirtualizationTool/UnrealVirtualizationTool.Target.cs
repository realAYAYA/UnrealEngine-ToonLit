// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealVirtualizationToolTarget : TargetRules
{
	public UnrealVirtualizationToolTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		LaunchModuleName = "UnrealVirtualizationTool";

		bBuildDeveloperTools = false;

		bCompileAgainstEditor = false;
		bBuildWithEditorOnlyData = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstApplicationCore = true;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = false;
		bUsesSlate = false;

		// This app is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		bIsBuildingConsoleApplication = true;

		// Enable Developer plugins
		bCompileWithPluginSupport = true;

		// Reduce the final exe/pdb size
		WindowsPlatform.bStripUnreferencedSymbols = true;
		WindowsPlatform.bMergeIdenticalCOMDATs = true;

		bEnableTrace = true;
		GlobalDefinitions.Add("UE_SUPPORT_FULL_PACKAGEPATH=1");
	}
}
