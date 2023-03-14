// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetTools : ModuleRules
{
	public AssetTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Merge",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "CurveAssetEditor",
				"Engine",
                "InputCore",
				"ApplicationCore",
				"Slate",
				"SourceControl",
				"PropertyEditor",
				"Kismet",
				"Landscape",
                "Foliage",
                "Projects",
				"RHI",
				"MaterialEditor",
				"ToolMenus",
				"PhysicsCore",
				"DeveloperSettings",
				"ClassViewer",
				"EngineSettings",
				"InterchangeCore",
				"InterchangeEngine",
				"PhysicsUtilities",
				"AssetRegistry",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
				"ContentBrowser",
				"CollectionManager",
                "CurveAssetEditor",
				"DesktopPlatform",
				"EditorWidgets",
				"GameProjectGeneration",
                "PropertyEditor",
                "ActorPickerMode",
				"Kismet",
				"MainFrame",
				"MaterialEditor",
				"MessageLog",
				"PackagesDialog",
				"Persona",
				"FontEditor",
                "AudioEditor",
				"SourceControl",
				"Landscape",
                "SkeletonEditor",
                "SkeletalMeshEditor",
                "AnimationEditor",
                "AnimationBlueprintEditor",
                "AnimationModifiers",
			    "TextureEditor",
				"DataTableEditor",
				"Cascade",
				"PhysicsAssetEditor",
				"CurveTableEditor",
				"StaticMeshEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"CollectionManager",
				"CurveTableEditor",
				"DataTableEditor",
				"DesktopPlatform",
				"EditorWidgets",
				"GameProjectGeneration",
                "ActorPickerMode",
				"MainFrame",
				"MessageLog",
				"PackagesDialog",
				"Persona",
				"FontEditor",
                "AudioEditor",
                "SkeletonEditor",
                "SkeletalMeshEditor",
                "AnimationEditor",
                "AnimationBlueprintEditor",
                "AnimationModifiers"
            }
		);
	}
}
