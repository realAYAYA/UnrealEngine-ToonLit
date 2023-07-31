// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CustomizableObjectEditor : ModuleRules
{

	public CustomizableObjectEditor(ReadOnlyTargetRules TargetRules) : base(TargetRules)
	{
		ShortName = "MuCOE";

		DefaultBuildSettings = BuildSettingsVersion.V2;

		// Strangely enough:
		// - this has to be enabled for the windows editor build, or it raises an exception at runtime (compiling Mutable).
		// - this cannot be enabled in Linux, or it doesn't compile.
		// No class in this module requires RTTI, but this module uses RTTI for classes in the MutableTools module.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			bUseRTTI = true;
		}

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
				"KismetWidgets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ImageCore",
				"Engine",
				
				"Slate",
				"SlateCore",
				"EditorWidgets",
				"ApplicationCore",
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
				"ClothingSystemRuntimeInterface",
				"DeveloperToolSettings",
				"ToolMenus",
				"EditorFramework",
				"UMG",
				"Persona",
				"CurveEditor",

				"MutableRuntime",
				"MutableTools",
				"GameplayTags"
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
				}
			);

		PrivateIncludePaths.AddRange(new string[] {
				"CustomizableObject/Private",
				"MutableRuntime/Private",
				"MutableTools/Private",
			});
	}
}
