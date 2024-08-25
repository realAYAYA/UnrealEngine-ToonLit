// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StateGraphTestsTarget : TestTargetRules
{
	public StateGraphTestsTarget(TargetInfo Target) : base(Target)
	{
		bForceEnableExceptions = true;
		EnablePlugins.Add("StateGraph");
	}
}
