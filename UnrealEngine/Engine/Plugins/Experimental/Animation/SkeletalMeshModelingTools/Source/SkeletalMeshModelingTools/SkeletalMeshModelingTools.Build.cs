// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletalMeshModelingTools : ModuleRules
{
	public SkeletalMeshModelingTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"SkeletalMeshModelingTools/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"Engine",
				"ToolMenus",
				"UnrealEd",
				"InputCore",
				"ModelingComponentsEditorOnly",
				"MeshModelingTools",
				"MeshModelingToolsExp",
				"MeshModelingToolsEditorOnly",
				"MeshModelingToolsEditorOnlyExp",
				"ModelingToolsEditorMode",
				"InteractiveToolsFramework",
				"EditorInteractiveToolsFramework",
				"StylusInput",
				"ToolWidgets",
			}
		);
	}
}
