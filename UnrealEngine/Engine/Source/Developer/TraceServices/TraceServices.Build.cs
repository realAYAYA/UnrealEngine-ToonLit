// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceServices : ModuleRules
{
	public TraceServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Cbor",
				"Core",
				"TraceAnalysis",
				"SymsLib"
			});
	}
}
