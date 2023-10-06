// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class AutoRTFMTestsTarget : TargetRules
{
	public AutoRTFMTestsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		Name = "AutoRTFMTests";
		LaunchModuleName = "AutoRTFMTests";

		// Compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;

		// Logs are still useful to print the results
		bUseLoggingInShipping = true;

		// Make a console application under Windows, so entry point is main() everywhere
		bIsBuildingConsoleApplication = true;

		// Disable unity builds by default for AutoRTFMTest
		bUseUnityBuild = false;

		// Set the RTFM clang compiler
		if (!bGenerateProjectFiles)
		{
			 bUseAutoRTFMCompiler = true;
		}
	}
}
