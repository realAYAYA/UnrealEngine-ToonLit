// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StateGraphManager : ModuleRules
{
	public StateGraphManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"Engine",
			"StateGraph"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject"
		});
	}
}
