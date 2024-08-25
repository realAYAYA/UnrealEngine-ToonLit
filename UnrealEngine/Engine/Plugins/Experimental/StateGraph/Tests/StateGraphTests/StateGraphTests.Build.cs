// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StateGraphTests : TestModuleRules
{
	public StateGraphTests(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.Never;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"StateGraph"
			});

		UpdateBuildGraphPropertiesFile(new Metadata() { TestName = "StateGraph", TestShortName = "State Graph"});
	}
}
