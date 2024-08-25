// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Collections.Generic;
using UnrealBuildTool;

public class ModelViewViewModelDebuggerEditor : ModuleRules 
{
	public ModelViewViewModelDebuggerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "MVVMDebuggerEditor";

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ModelViewViewModel",
				"ModelViewViewModelDebugger",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"LevelEditor",
				"MessageLog",
				"Projects",
				"SlateCore",
				"Slate",
				"ToolMenus",
				"ToolWidgets",
				"WorkspaceMenuStructure",
			});
	}
}
