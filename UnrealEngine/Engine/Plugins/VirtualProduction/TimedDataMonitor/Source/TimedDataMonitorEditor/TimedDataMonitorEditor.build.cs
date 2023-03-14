// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimedDataMonitorEditor : ModuleRules
{
	public TimedDataMonitorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorWidgets",
				"Engine",
				"EditorStyle",
				"InputCore",
				"MessageLog",
				"Projects",
				"Settings",
				"Slate",
				"SlateCore",
				"TimedDataMonitor",
				"TimeManagement",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
	}
}
