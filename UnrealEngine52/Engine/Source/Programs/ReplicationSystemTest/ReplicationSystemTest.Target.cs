// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class ReplicationSystemTestTarget : TargetRules
{
	public ReplicationSystemTestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "ReplicationSystemTest";

		bUseMallocProfiler = false;

		// No editor-only data is needed
		bBuildWithEditorOnlyData = false;

		bCompileAgainstEngine = true;
		bCompileAgainstCoreUObject = false;
		bCompileAgainstApplicationCore = true;
        bCompileWithPluginSupport = true;
        bBuildDeveloperTools = false;
        bBuildRequiresCookedData = true; // this program requires no data

		// No ICU internationalization as it causes shutdown errors.
		bCompileICU = false;

		// Don't need slate
		bUsesSlate = false;

		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bIsBuildingConsoleApplication = true;
		bLegalToDistributeBinary = true;

		// Always want logging so we can see the test results
        bUseLoggingInShipping = true;

		// Network config
		bWithPushModel = true;		
		bUseIris = true;

		GlobalDefinitions.Add("UE_TRACE_ENABLED=1");
	}
}

