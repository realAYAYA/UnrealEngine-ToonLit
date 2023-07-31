// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorkspaceMenuStructure : ModuleRules
{
	public WorkspaceMenuStructure(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SlateCore",
				"Slate"
			}
		);
	}
}
