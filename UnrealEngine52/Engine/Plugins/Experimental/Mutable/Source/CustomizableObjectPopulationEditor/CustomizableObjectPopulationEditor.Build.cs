// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CustomizableObjectPopulationEditor : ModuleRules
{

	public CustomizableObjectPopulationEditor(ReadOnlyTargetRules TargetRules) : base(TargetRules)
	{
		ShortName = "MuCOPE";

		DefaultBuildSettings = BuildSettingsVersion.V2;

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Kismet",
				"EditorWidgets",
				"MeshUtilities",
				"ContentBrowser",
				"SkeletonEditor",
				"Persona",
				"WorkspaceMenuStructure",
				"AdvancedPreviewScene",

				"MessageLog",
				"KismetWidgets",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				
				"Slate",
				"SlateCore",
				"RenderCore",
				"RHI",
				"UnrealEd",
				"TargetPlatform",
				"RawMesh",
				"PropertyEditor",
				"LevelEditor",
				"AssetTools",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"AdvancedPreviewScene",
				"AppFramework",
				"Projects",
				"ClothingSystemRuntimeCommon",

				"MutableRuntime",
				"MutableTools",
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"SceneOutliner",
				"ClassViewer",
				"ContentBrowser",
				"WorkspaceMenuStructure",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"CustomizableObject",
				"CustomizableObjectEditor",
				"CustomizableObjectPopulation",
			}
		);

		PrivateIncludePaths.AddRange(new string[] {
				"CustomizableObjectPopulation/Private",
			});
	}
}
