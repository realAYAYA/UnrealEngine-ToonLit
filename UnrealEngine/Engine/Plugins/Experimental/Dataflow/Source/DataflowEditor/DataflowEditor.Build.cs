// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowEditor : ModuleRules
	{
        public DataflowEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"SkeletonEditor"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"ApplicationCore",
					"AssetDefinition",
					"AssetTools",
					"AssetRegistry",
					"BaseCharacterFXEditor",
					"Chaos",
					"Core",
					"CoreUObject",
					"DataflowAssetTools",
					"DataflowCore",
					"DataflowEngine",
					"DataflowEnginePlugin",
					"DataflowNodes",
					"DynamicMesh",
					"Engine",
					"EditorFramework",
					"EditorInteractiveToolsFramework",
					"EditorStyle",
					"GeometryCore",
					"GeometryFramework",
					"GraphEditor",
					"InputCore",
					"InteractiveToolsFramework",
					"LevelEditor",
					"MeshDescription",
					"MeshConversion",
					"MeshModelingToolsEditorOnlyExp",
					"MeshModelingToolsExp",
					"ModelingComponentsEditorOnly",
					"ModelingComponents",
					"Projects",
					"PropertyEditor",
					"RenderCore",
					"RHI",
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"ToolMenus",
					"ToolWidgets",
					"TypedElementRuntime",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"XmlParser",
					"EditorWidgets",
					"KismetWidgets",      // SScrubControlPanel
					"AnimGraph"           // UAnimSingleNodeInstance
				}
			);
		}
	}
}
