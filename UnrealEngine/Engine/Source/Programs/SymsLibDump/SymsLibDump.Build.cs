// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SymsLibDump : ModuleRules
{
	public SymsLibDump(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"Projects",
				"SymsLib",
			}
		);
	}
}
