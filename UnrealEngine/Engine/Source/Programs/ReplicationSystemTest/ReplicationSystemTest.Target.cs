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

		bEnableTrace = true;
		GlobalDefinitions.Add("UE_NET_TEST_FAKE_REP_TAGS=1");
	}
}

