// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PhysicsAssetEditor : ModuleRules
{
	public PhysicsAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
             new string[] {
                "Editor/UnrealEd/Public",
                "Editor/Persona/Public"
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"RenderCore",
				"Slate",
				"SlateCore",
				"LevelEditor",
				"EditorFramework",
				"UnrealEd",
                "Kismet",
                "Persona",
                "SkeletonEditor",
                "GraphEditor",
                "AnimGraph",
                "AnimGraphRuntime",
                "AdvancedPreviewScene",
                "DetailCustomizations",
                "PinnedCommandList",
				"ToolMenus",
				"PhysicsCore",
				"PhysicsUtilities",
				"MeshUtilitiesCommon",
				"ApplicationCore",
				"EditorStyle",
				"ToolWidgets"
			}
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor",
                "MeshUtilities",
			}
		);
	}
}
