// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("SceneOutliner"), "Private"),
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ClassViewer",
				"MainFrame",
                "PlacementMode",
				"SlateReflector",
                "AppFramework",
                "PortalServices",
                "Persona",
				"DataLayerEditor",
				"MergeActors",
				"Layers",
				"WorldBrowser",
				"NewLevelDialog",
				"LocalizationDashboard",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"HeadMountedDisplay",
				"UnrealEd",
				"VREditor",
				"CommonMenuExtensions"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
				"Analytics",
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"LauncherPlatform",
				"InputCore",
				"Slate",
				"SlateCore",
				"Engine",
				"MessageLog",
				"SourceControl",
				"SourceControlWindows",
				"StatsViewer",
				"EditorFramework",
				"UnrealEd", 
				"DeveloperSettings",
				"RenderCore",
				"DeviceProfileServices",
				"ContentBrowser",
				"SceneOutliner",
				"ActorPickerMode",
				"RHI",
				"Projects",
				"TargetPlatform",
				"TypedElementFramework",
				"TypedElementRuntime",
				"EngineSettings",
				"PropertyEditor",
				"Kismet",
				"KismetWidgets",
				"Sequencer",
				"Foliage",
				"HierarchicalLODOutliner",
				"HierarchicalLODUtilities",
				"MaterialShaderQualitySettings",
				"PixelInspectorModule",
				"CommonMenuExtensions",
				"ToolMenus",
				"StatusBar",
				"AppFramework",
				"EditorSubsystem",
				"EnvironmentLightingViewer",
				"DesktopPlatform",
				"DataLayerEditor",
				"TranslationEditor",
				"SubobjectEditor",
				"SubobjectDataInterface",
				"DerivedDataEditor",
				"EditorWidgets",
				"ToolWidgets",
				"VirtualizationEditor",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SlateReflector",
				"AutomationWindow",
				"Layers",
				"WorldBrowser",
				"WorldPartitionEditor",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
                "PlacementMode",
				"HeadMountedDisplay",
				"VREditor",
                "Persona",
				"LevelAssetEditor",
				"MergeActors"
			}
		);

		if (Target.bBuildTargetDeveloperTools)
		{
			DynamicallyLoadedModuleNames.Add("SessionFrontend");
		}


		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
