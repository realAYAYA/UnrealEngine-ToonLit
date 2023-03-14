// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterLightCardEditor : ModuleRules
{
	public DisplayClusterLightCardEditor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {

				"AdvancedPreviewScene",
				"ApplicationCore",
				"AppFramework",
				"Core",
				"CoreUObject",
				"DisplayCluster",
				"DisplayClusterConfiguration",
				"DisplayClusterOperator",
				"DisplayClusterLightCardExtender",
				"DisplayClusterLightCardEditorShaders",
				"DisplayClusterScenePreview",
				"EditorStyle",
				"Engine",
				"InputCore",
				"OpenCV",
				"OpenCVHelper",
				"ProceduralMeshComponent",
				"PropertyEditor",
				"RenderCore",
				"Renderer",
				"RHI",
				"SceneOutliner",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"ToolMenus",
				"ToolWidgets",
				"WorkspaceMenuStructure"
			});

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
			}
		);
	}
}
