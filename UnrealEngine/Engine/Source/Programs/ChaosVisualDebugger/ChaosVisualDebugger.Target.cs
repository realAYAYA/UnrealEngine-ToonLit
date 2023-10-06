// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class ChaosVisualDebuggerTarget : TargetRules
{
	public ChaosVisualDebuggerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "ChaosVisualDebugger";

		// Lean and mean
		bBuildDeveloperTools = true;

		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		//bCompileAgainstApplicationCore = false;

		// This a Windows app (sets entry point to WinMain(), instead of main())
		bIsBuildingConsoleApplication = false;
	}
}
