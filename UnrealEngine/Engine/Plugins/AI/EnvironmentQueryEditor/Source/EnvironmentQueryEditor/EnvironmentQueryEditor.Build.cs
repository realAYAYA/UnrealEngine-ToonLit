// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EnvironmentQueryEditor : ModuleRules
{
	public EnvironmentQueryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

		// These nodes are not public so are hard to subclass
		PrivateIncludePaths.AddRange(
			new string[] {
					 Path.Combine(EngineDir, @"Source/Editor/GraphEditor/Private"),
					 Path.Combine(EngineDir, @"Source/Editor/AIGraph/Private")
			});

		PrivateIncludePathModuleNames.AddRange(
		   new string[] {
				"AssetRegistry",
				"DesktopPlatform",
		   });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetDefinition",
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Engine", 
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd", 
				"GraphEditor",
				"KismetWidgets",
                "PropertyEditor",
                "AIGraph",
                "AIModule",
				"ToolMenus",
			}
		);
	}
}
