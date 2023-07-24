// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceControlWindowExtender : ModuleRules
{
	public SourceControlWindowExtender(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine",
            	"ToolMenus",
				"SlateCore",
				"Slate",
				"SourceControl",
				"SourceControlWindows",
				"UnrealEd"
			}
		);
	}
}
