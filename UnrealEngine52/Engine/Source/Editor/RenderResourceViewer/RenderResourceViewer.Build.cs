// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderResourceViewer : ModuleRules
{
	public RenderResourceViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"WorkspaceMenuStructure",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InputCore",
				"SlateCore",
				"Slate",
				"RHI",
			}
		);
	}
}
