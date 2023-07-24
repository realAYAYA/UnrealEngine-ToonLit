// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosClothAssetEditor : ModuleRules
{
	public ChaosClothAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add other public dependencies that you statically link with here ...
				
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BaseCharacterFXEditor",
				"Core",
				"UnrealEd",
				"InputCore",
				"InteractiveToolsFramework",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Projects",
				"AdvancedPreviewScene",
				"ContentBrowser",
				"DynamicMesh",
				"EditorFramework", // FEditorModeInfo
				"EditorInteractiveToolsFramework",
				"ModelingComponentsEditorOnly", // Static/skeletal mesh tool targets
				"ModelingComponents",
				"MeshModelingToolsExp",
				"MeshModelingToolsEditorOnlyExp",
				"EditorStyle",
				"EditorSubsystem",
				"GeometryCore",
				"GeometryFramework",
				"LevelEditor",
				"StatusBar",
				"ToolMenus",
				"ToolWidgets",
				"WorkspaceMenuStructure",
				"ClothingSystemRuntimeCommon",
				"ClothingSystemEditorInterface",
				"ClothingSystemRuntimeInterface",
				"Chaos",
				"ChaosClothAsset",
				"ChaosClothAssetEngine",
				"ClothingSystemEditor",
				"ChaosClothAssetTools",
				"ChaosClothAssetEditorTools",
				"DataflowCore",
				"DataflowEditor",
				"DataflowEngine",
				"DataflowEnginePlugin",
				"RenderCore",
				"TypedElementFramework",
				"TypedElementRuntime",
				"AssetDefinition",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
