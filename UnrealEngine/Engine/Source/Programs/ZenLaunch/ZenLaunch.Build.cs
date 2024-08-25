// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZenLaunch : ModuleRules
{
	public ZenLaunch(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
		PrivateDependencyModuleNames.Add("Json");
		PrivateDependencyModuleNames.Add("Zen");
	}
}
