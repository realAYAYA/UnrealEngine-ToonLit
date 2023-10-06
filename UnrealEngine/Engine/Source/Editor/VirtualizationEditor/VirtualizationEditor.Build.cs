// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualizationEditor : ModuleRules
{
	public VirtualizationEditor(ReadOnlyTargetRules Target)
		 : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ContentBrowserData",
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"ToolMenus",
				"SourceControl",
				"Slate",
				"SlateCore",
				"Virtualization"
			});

		PrivateIncludePathModuleNames.AddRange
		(
			new string[]
			{
				"WorkspaceMenuStructure",
			}
		);
	}
}
