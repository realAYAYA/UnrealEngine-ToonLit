// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BenchmarkTool : ModuleRules
{
	public BenchmarkTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
	}
}
