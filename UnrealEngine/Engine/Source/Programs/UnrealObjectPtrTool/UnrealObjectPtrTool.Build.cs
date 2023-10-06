// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealObjectPtrTool : ModuleRules
{
	public UnrealObjectPtrTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
	}
}
