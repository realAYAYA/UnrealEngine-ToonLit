// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceServices : ModuleRules
{
	public TraceServices(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Cbor",
				"Core",
				"SymsLib",
				"TraceAnalysis",
			}
		);
	}
}
